#include <linux/sched.h>
#include <linux/unistd.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/random.h>
#include <linux/slab.h>
#include <linux/semaphore.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/highmem.h>
#include <asm/current.h>

#include "sys_monitor.h"
#include "interface.h"
#include "log.h"

#include "process.h"

#include "fd_cache.h"

#define SYSMON_DEBUG
#include "./../share/debug.h"

#define RUN_TESTCASE

//initize an new fd cache area of process record, called when an new process go into our monitor
void fd_cache_initize(process_record * record)
{
#ifdef RUN_TESTCASE
	int i = 0;
	unsigned int fds[100] = { 0 };
#endif

	init_rwsem(&record->file_fd_sem);
    
	record->fd_count = 0;
	record->hot_count = 0;
	record->fd_indexer = 0;
    record->file_fd_head = NULL;
    
	memset(record->hot_fd_cache, 0, sizeof(void *) * HOT_FD_CACHE_SIZE);
	memset(record->hot_cache_time, 0, sizeof(int) * HOT_FD_CACHE_SIZE);

	PDEBUG("process \"%s\" initized fd_cache, cache ready to use\n", record->filename);

#ifdef RUN_TESTCASE
	//prepare testcase
	for (i = 0; i < 100; i++)
	{
		fds[i] = (unsigned int)i;
	}

	for (i = 0; i < 100; i++)
	{
		unsigned int t = 0;
		int exchange = 0;

		get_random_bytes(&exchange, sizeof(int));
		exchange = exchange % 100;

		t = fds[i];
		fds[i] = fds[exchange];
		fds[exchange] = t;
	}

	for (i = 0; i < 100; i++)
	{
		PDEBUG("[%d]: %d\n", i, fds[i]);
	}

	//run testcase, insert
	for (i = 0; i < 100; i++)
	{
		char buffer[2];
		buffer[0] = fds[i];
		buffer[1] = '\0';

		insert_into_cache(fds[i], buffer);
	}

	//run testcase, check and delete
	for (i = 0; i < 100; i++)
	{
		char * path = get_cache_by_fd(fds[i]);
		if (fds[i] != (unsigned int)path[0])
		{
			PERROR("testcase error, fd #%d not match path #%d\n", fds[i], (int)path[0]);
		}

		delete_cache_by_fd(fds[i]);
	}

	if (0 != record->fd_count ||
		0 != record->hot_count ||
		NULL != record->file_fd_head)
	{
		PERROR("testcase error, record area not clean\n");
	}
	
	PDEBUG("testcase finish\n");
#endif
}

//cleanup fd cache area and resource, called when process gone away
void fd_cache_cleanup(process_record * record)
{
	file_fd_record * p = record->file_fd_head;

	while (NULL != p)
	{
		file_fd_record * t = p->next;

		if (NULL != p->filename)
		{
			kfree(p->filename);
		}

		kfree(p);
		p = t;
	}

	PDEBUG("process \"%s\" fd_cache cleanuped\n", record->filename);
}

//find record in current process hot cache, may fail, always called before find in link to speed up
file_fd_record * find_record_in_hot_cache(process_record * record, unsigned int fd, int & index)
{
	index = 0;

	return NULL;
}

//find record in current process, directly from record link, usually very slow
file_fd_record * find_record_in_link(process_record * record, unsigned int fd)
{
	file_fd_record * res = NULL;
	file_fd_record * p = record->file_fd_head;

	while (NULL != p)
	{
		if (p->fd == fd)
		{
			res = p;

			break;
		}

		p = p->next;
	}

	return res;
}

//allocate an file record from memory and initize it
file_fd_record * allocate_file_fd_record(int fd, char * path)
{
    file_fd_record * fd_record = NULL;

    fd_record = (file_fd_record *)kmalloc(sizeof(file_fd_record), GFP_KERNEL);
    if (NULL != fd_record)
    {
        int len = strlen(path);
        fd_record->filename = (char *)kmalloc(len + 1, GFP_KERNEL);
        if (NULL != fd_record->filename)
        {
            if (len > 0)
            {
                strcpy(fd_record->filename, path);
			}
	        
			fd_record->filename[len] = '\0';
		}
		else
		{
			PWARN("allocate memory failed\n");

			kfree(fd_record);

			fd_record = NULL;
		}
	}
	else
	{
		PWARN("allocate memory failed\n");
	}

	return fd_record;	
}

int kill_oldest_cache(process_record * record)
{
	int i = 0;
	int min_index = 0;
	int min = record->hot_cache_time[0];
	
	//find min age, which means oldest
	for (i = 1; i < HOT_FD_CACHE_SIZE; i++)
	{
		if (min > record->hot_cache_time[i])
		{
			min = record->hot_cache_time[i];
			min_index = i;
		}
	}

	PVERBOSE("kill oldest cache(age: %d, path: \"%s\", id: %d)", min, record->hot_fd_cache[min_index]->filename, min_index);

	record->hot_fd_cache[min_index] = NULL;
	record->hot_cache_time[min_index] = 0;

	return min_index;
}

//get index that fits fd to insert in
int get_insert_index(process_record * record, unsigned int fd)
{
	int fit_index = 0;
	unsigned int prev_fd = -1;
	unsigned int next_fd = 0xFFFFFFFF;
	int l = 0;
	int r = record->hot_count - 1;

	while (l <= r)
	{
		int m = (l + r) / 2;
		unsigned int t_fd = -1;
		
		prev_fd = -1;
		next_fd = 0xFFFFFFFF;

		//assert last
		fit_index = record->hot_count - 1;

		t_fd = record->hot_fd_cache[m]->fd;
		if (0 != m)
		{
			prev_fd = record->hot_fd_cache[m - 1]->fd;
		}
		if (record->hot_count - 1 != m)
		{
			next_fd = record->hot_fd_cache[m + 1]->fd;
		}

		if (prev_fd <= fd && t_fd >= fd)
		{
			fit_index = m;

			break;
		}
		
		if (t_fd <= fd && next_fd >= fd)
		{
			fit_index = m + 1;

			break;
		}

		if (t_fd < fd)
		{
			l = m + 1;
		}
		else
		{
			r = m - 1;
		}
	}

	PVERBOSE("found fd #%d insert index at %d, prev fd: %d, next fd: %d\n", fd, fit_index, prev_fd, next_fd);
}

void insert_into_hot_cache(process_record * record, file_fd_record * fd_record)
{
	int i = 0;
	//first, find an fit index
	int fit_index = get_insert_index(record, fd_record->fd);

	return;

	if (record->hot_count == HOT_FD_CACHE_SIZE)
	{
		//full, we need to make an room
		int index = kill_oldest_cache(record);
	}
	else
	{
		//move
		for (i = record->hot_count; i > fit_index; i--)
		{
			record->hot_fd_cache[i] = record->hot_fd_cache[i - 1];
			record->hot_cache_time[i] = record->hot_cache_time[i - 1];
		}	

		//settle
		record->hot_fd_cache[fit_index] = fd_record;
		//record->hot_cache_time[fit_index] = __FIX_ME__;

		//add reference
		record->hot_count++;
	}
}

//allocate and insert into current process record`s fd record link
file_fd_record * insert_into_link(process_record * record, unsigned int fd, char * path)
{
	file_fd_record * fd_record = NULL;

	fd_record = allocate_file_fd_record(fd, path);
	if (NULL != fd_record)
	{
		//insert to head
		fd_record->prev = NULL;
		fd_record->next = record->file_fd_head;
		record->file_fd_head->prev = fd_record;
		record->file_fd_head = fd_record;
	}

	return fd_record;
}

bool delete_from_record(process_record * record, unsigned int fd)
{
	bool b_hot = true;
	bool suc = false;
	file_fd_record * fd_record = NULL;

	fd_record = find_record_in_hot_cache(record, fd);
	if (NULL == fd_record)
	{
		//hot cache failed, go normally
		b_hot = false;
			
		fd_record = find_record_in_link(record, fd);
	}

	if (NULL != fd_record)
	{
		//delete from link
		if (fd_record == record->file_fd_head)
		{
			//head
			record->file_fd_head = record->file_fd_head->next;
			record->file_fd_head->prev = NULL;
		}
		else
		{
			//normal
			fd_record->prev->next = fd_record->next;
			fd_next->prev = fd_record->prev;
		}

		//delete from hot cache

		if (b_hot)
		{
			record->hot_count--;
		}

		record->fd_count--;
		if (0 == record->fd_count)
		{
			record->file_fd_head = NULL;
		}

		//finally, release memory
		kfree(fd_record);
	}
	else
	{
		PWARN("delete fd record failed, process: %d, fd: %d\n", current->pid, fd);
	}

	return suc;
}

char * get_cache_by_fd(unsigned int fd)
{
	char * path = NULL;
	pid_t pid = current->pid;
	process_record * record = NULL;

	//lock process record first
	lock_process_record();

	record = get_record_by_pid(pid);
	if (NULL != record)
	{
		file_fd_record * fd_record = NULL;
	
		//lock file fd also
		down_write(&record->file_fd_sem);

		//first, find fd from hot cache
		fd_record = find_record_in_hot_cache(record, fd);
		if (NULL == fd_record)
		{
			PVERBOSE("pid #%d hot cache not found fd #%d, now find normally\n", pid, fd);

			//hot cache failed, find normally
			fd_record = find_record_in_link(record, fd);
			if (NULL != fd_record)
			{
				//then add current fd to cache
				insert_into_hot_cache(record, fd_record);
			}
		}
		
		//get the filename from record
		if (NULL != fd_record)
		{
			path = fd_record->filename;
			
			PVERBOSE("pid #%d[fd #%d] is \"%s\"\n", pid, fd, path);
		}
		else
		{
			PWARN("pid #%d[fd #%d] does not exists\n", pid, fd);
		}

		up_write(&record->file_fd_sem);
	}
	else
	{
		PWARN("pid #%d process record not exists\n", pid);
	}

	unlock_process_record();

	return path;
}

void insert_into_cache(unsigned int fd, char * path)
{
	process_record * record = NULL;
	
	//lock process reocrd first
	lock_process_record();

	record = get_record_by_pid(current->pid);
	if (NULL != record)
	{
		file_fd_record * fd_record = NULL;
		
		//lock file fd also
		down_write(&record->file_fd_sem);

		fd_record = insert_into_link(record, fd, path);
		if (fd_record)
		{
			insert_into_hot_cache(record, fd_record);
			
			PVERBOSE("pid #%d[fd #%d] updated to \"%s\"\n", current->pid, fd, path);
		}

		up_write(&record->file_fd_sem);
	}
	else
	{
		PWARN("pid #%d process record not exists\n", current->pid);
	}

	unlock_process_record();
}

bool delete_cache_by_fd(unsigned int fd)
{
	bool suc = false;
	process_record * record = NULL;

	lock_process_record();

	record = get_record_by_pid(current->pid);
	if (NULL != record)
	{
		down_write(&record->file_fd_sem);

		suc = delete_from_record(record, fd);

		up_write(&record->file_fd_sem);

		PVERBOSE("pid #%d[fd #[%d] delete result: %b\n", current->pid, fd, suc);
	}
	else
	{
		PWARN("pid #%d process record not exists\n", current->pid);
	}

	unlock_process_record();

	return suc;
}
