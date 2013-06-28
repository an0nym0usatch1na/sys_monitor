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

//testcase
void run_fd_testcase(void)
{
	int i = 0;
	unsigned int fds[100] = { 0 };
	process_record * record = get_record_by_pid(current->pid);

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
		if (exchange < 0)
		{
			exchange = -1 * exchange;
		}

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

	PVERBOSE("insert finish, no error found\n");

	//run testcase, check and delete
	for (i = 0; i < 100; i++)
	{
		char * path = get_cache_by_fd((unsigned int)i);
		if (NULL == path)
		{
			PERROR("testcase error, fd #%d does not exists\n", i);

			continue;
		}

		if (i != (int)path[0])
		{
			PERROR("testcase error, fd #%d not match path #%d\n", i, (int)path[0]);

			continue;
		}

		delete_cache_by_fd((unsigned int)i);
	}

	if (0 != record->fd_count ||
		0 != record->hot_count ||
		NULL != record->file_fd_head)
	{
		PERROR("testcase error, record area not clean\n");
	}
	
	PDEBUG("testcase finish\n");
}

//initize an new fd cache area of process record, called when an new process go into our monitor
void fd_cache_initize(process_record * record)
{
	init_rwsem(&record->file_fd_sem);
    
	record->fd_count = 0;
	record->hot_count = 0;
    record->file_fd_head = NULL;
   	record->hot_fd_head = NULL;

	memset(record->hot_fd_cache, 0, sizeof(void *) * HOT_FD_CACHE_SIZE);

	PDEBUG("process \"%s\" initized fd_cache, cache ready to use\n", record->filename);
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

//find record in current process hot cache, may fail, use binary search, always called before find in link to speed up
file_fd_record * find_record_in_hot_cache(process_record * record, unsigned int fd, int * index)
{
	int l = 0;
	int r = record->hot_count - 1;
	
	*index = -1;

	while (l <= r)
	{
		int m = (l + r) / 2;
		unsigned int t_fd = record->hot_fd_cache[m]->fd;

		if (t_fd < fd)
		{
			l = m + 1;
		}
		else if (t_fd > fd)
		{
			r = m - 1;
		}
		else	//t_fd == fd
		{
			*index = m;

			break;
		}
	}

	if (-1 != *index)
	{
		return record->hot_fd_cache[*index];
	}
	else
	{
		return NULL;
	}
}

//find record in current process, directly from full record link, usually very slow
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
file_fd_record * allocate_file_fd_record(unsigned int fd, char * path)
{
    file_fd_record * fd_record = NULL;

    fd_record = (file_fd_record *)kmalloc(sizeof(file_fd_record), GFP_KERNEL);
    if (NULL != fd_record)
    {
        int len = 0;
		
		fd_record->fd = fd;
		fd_record->hot_index = 0;
		
		len = strlen(path);
        fd_record->filename = (char *)kmalloc(len + 1, GFP_KERNEL);
        if (NULL != fd_record->filename)
        {
            if (len > 0)
            {
                strcpy(fd_record->filename, path);
			}
	        
			fd_record->filename[len] = '\0';

			PVERBOSE("allocated file fd record 0x%08x\n", fd_record);
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

//get index that fits fd to insert in, in which index`s *LEFT*
int get_insert_index(process_record * record, unsigned int fd)
{
	int fit_index = 0;
	unsigned int prev_fd = 0;
	unsigned int next_fd = 0;
	int l = 0;
	int r = record->hot_count - 1;

	while (l <= r)
	{
		int m = (l + r) / 2;
		unsigned int t_fd = -1;
		
		prev_fd = 0;
		next_fd = 0;

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

		if ((0 == m || prev_fd <= fd) && t_fd >= fd)
		{
			fit_index = m;

			break;
		}
		
		if (t_fd <= fd && (record->hot_count -1 == m || next_fd >= fd))
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

	PVERBOSE("found fd #%u insert index at %d, prev fd: %u, next fd: %u\n", fd, fit_index, prev_fd, next_fd);

	return fit_index;
}

void insert_into_hot_cache(process_record * record, file_fd_record * fd_record)
{
	int i = 0;
	int fit_index = 0;
	int oldest_index = -1;

	//if full, then we need to make an room
	if (record->hot_count == HOT_FD_CACHE_SIZE)
	{
		file_fd_record * oldest_record = NULL;

		ASSERT(NULL != record->hot_fd_head);

		//hot cache link`s last element must be oldest because we always insert young element to head
		oldest_record = record->hot_fd_head->time_prev;
		oldest_index = oldest_record->hot_index;

		ASSERT(oldest_index >= 0);

		//break up hot cache link
    	oldest_record->time_prev->time_next = oldest_record->time_next;
		oldest_record->time_next->time_prev = oldest_record->time_prev;

		PVERBOSE("kill oldest cache(path: \"%s\", fd: %u, id: %d)", oldest_record->filename, oldest_record->fd, oldest_record->hot_index);
	}

	//find the place to insert
	fit_index = get_insert_index(record, fd_record->fd);
	
	PVERBOSE("find fit index: %u, oldest_index: %u\n", fit_index, oldest_index);
		
	//move
	if (-1 == oldest_index)
	{
		//no oldest index need to skip
		for (i = record->hot_count; i > fit_index; i--)
		{
			ASSERT((i - 1) == record->hot_fd_cache[i - 1]->hot_index);

			record->hot_fd_cache[i] = record->hot_fd_cache[i - 1];
			record->hot_fd_cache[i]->hot_index = i;
		}
	}
	else if (oldest_index < fit_index)
	{
		//fit index will be remain, insert into its left, so fit_index--
		for (i = oldest_index; i < fit_index - 1; i++)
		{
			ASSERT((i + 1) == record->hot_fd_cache[i + 1]->hot_index);

			record->hot_fd_cache[i] = record->hot_fd_cache[i + 1];
			record->hot_fd_cache[i]->hot_index = i;
		}

		fit_index--;
	}
	else if (oldest_index > fit_index)
	{
		//fit index will go right 1 index, so its index will not remain, just use its index
		for (i = oldest_index; i > fit_index; i--)
		{
			ASSERT((i - 1) == record->hot_fd_cache[i - 1]->hot_index);

			record->hot_fd_cache[i] = record->hot_fd_cache[i - 1];
			record->hot_fd_cache[i]->hot_index = i;
		}
	}
	else	// (oldest_index == fit_index)
	{
		//insert into oldest index`s left, perfect, nothing need to do
	}

	//settle, fit index means witch item`s left fit us
	record->hot_fd_cache[fit_index] = fd_record;
	fd_record->hot_index = fit_index;

	//link hot cache up
	if (NULL == record->hot_fd_head)
	{
		fd_record->time_prev = fd_record;
		fd_record->time_next = fd_record;
	}
	else
	{
		ASSERT(record->hot_fd_head->time_prev->time_next == record->hot_fd_head);
		ASSERT(record->hot_fd_head->time_next->time_prev == record->hot_fd_head);
		
		fd_record->time_prev = record->hot_fd_head->time_prev;
		fd_record->time_next = record->hot_fd_head;

		record->hot_fd_head->time_prev->time_next = fd_record;
		record->hot_fd_head->time_prev = fd_record;
	}

	record->hot_fd_head = fd_record;
    
	ASSERT(record->hot_fd_head->time_prev->time_next == record->hot_fd_head);
	ASSERT(record->hot_fd_head->time_next->time_prev == record->hot_fd_head);

	//add reference
	if (-1 == oldest_index)
	{
		record->hot_count++;
	}

	//check
	DEBUG_CODE(
	{
		bool check_suc = true;
		unsigned int prev = 0;
		
		for (i = 0; i < record->hot_count; i++)
		{
			if (i != record->hot_fd_cache[i]->hot_index)
			{
				check_suc = false;
				
				break;
			}

			if (prev > record->hot_fd_cache[i]->fd)
			{
				check_suc = false;

				break;
			}

			prev = record->hot_fd_cache[i]->fd;
		}

		if (!check_suc)
		{						                    
			for (i = 0; i < record->hot_count; i++)
			{
				PERROR("*[%d]:(fd: %u, index: %d, path: \"%s\")\n", i, record->hot_fd_cache[i]->fd, record->hot_fd_cache[i]->hot_index, record->hot_fd_cache[i]->filename);
			}
			
			PERROR("fd to insert: %u, fit index: %d, oldest index: %d\n", fd_record->fd, fit_index, oldest_index);
		}
	});
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
		fd_record->time_next = NULL;
		fd_record->time_prev = NULL;
		
		if (NULL != record->file_fd_head)
		{
			record->file_fd_head->prev = fd_record;
		}

		PVERBOSE("file fd head update to 0x%08x from 0x%08x\n", fd_record, record->file_fd_head);

		record->file_fd_head = fd_record;
		record->fd_count++;

		PVERBOSE("allocate and insert fd #%d into file_fd link succ\n", fd);
	}

	return fd_record;
}

bool delete_from_record(process_record * record, unsigned int fd)
{
	int i = 0;
	bool b_hot = true;
	bool suc = false;
	int del_index = 0;
	file_fd_record * fd_record = NULL;

	fd_record = find_record_in_hot_cache(record, fd, &del_index);
	if (NULL == fd_record)
	{
		//hot cache failed, go normally
		b_hot = false;

		PVERBOSE("find fd #%d in hot cache failed, now try normal one\n", fd);
			
		fd_record = find_record_in_link(record, fd);
	}
	else
	{
		PVERBOSE("find fd #%d in hot cache #%d\n", fd, del_index);
	}

	if (NULL != fd_record)
	{
		PVERBOSE("fd_record: 0x%08x, file_fd_head: 0x%08x\n", fd_record, record->file_fd_head);

		//delete from link
		if (fd_record == record->file_fd_head)
		{
			//head
			record->file_fd_head = record->file_fd_head->next;
			if (NULL != record->file_fd_head)
			{
				PVERBOSE("opps, 0x%08x, 0x%08x\n", record, record->file_fd_head);

				record->file_fd_head->prev = NULL;
			}
		}
		else
		{
			//normal
			//must have prev
			fd_record->prev->next = fd_record->next;
			
			//check next, may be NULL
			if (NULL != fd_record->next)
			{
				fd_record->next->prev = fd_record->prev;
			}
		}

		PVERBOSE("fd_record: 0x%08x, file_fd_head: 0x%08x\n", fd_record, record->file_fd_head);

		//delete from hot cache
		if (b_hot)
		{
			PVERBOSE("delete from cache, delete index: %d, hot count: %d\n", del_index, record->hot_count);

			//delete from cache
			for (i = del_index; i < record->hot_count - 1; i++)
			{
				ASSERT((i + 1) == record->hot_fd_cache[i + 1]->hot_index);
				if (i + 1 != record->hot_fd_cache[i + 1]->hot_index)
				{
					int j = 0;
					for (j = 0; j < record->hot_count; j++)
					{
						PERROR("[%d]:(fd: %d, index: %d, path: \"%s\")\n", j, record->hot_fd_cache[j]->fd, record->hot_fd_cache[j]->hot_index, record->hot_fd_cache[j]->filename);
					}

					PERROR("delete index: %d\n", del_index);
				}

				record->hot_fd_cache[i] = record->hot_fd_cache[i + 1];
				record->hot_fd_cache[i]->hot_index = i;
			}

			//delete from hot cache time link
			if (fd_record == record->hot_fd_head)
			{
				record->hot_fd_head = record->hot_fd_head->time_next;
			}
			
			fd_record->time_prev->time_next = fd_record->time_next;
			fd_record->time_next->time_prev = fd_record->time_prev;

			record->hot_count--;
			if (0 == record->hot_count)
			{
				record->hot_fd_head = NULL;
			}
		}

		record->fd_count--;
		if (0 == record->fd_count)
		{
			record->file_fd_head = NULL;
		}

		//finally, release memory
		//kfree(fd_record);

		suc = true;
	}
	else
	{
		PDEBUG("delete fd record failed, process: %d, fd: %d\n", current->pid, fd);
	}

	PVERBOSE("delete finish, current fd count: %d, hot count: %d\n", record->fd_count, record->hot_count);

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
	
	unlock_process_record();

	if (NULL != record)
	{
		int index = 0;
		file_fd_record * fd_record = NULL;
	
		//lock file fd also
		down_read(&record->file_fd_sem);

		//first, find fd from hot cache
		fd_record = find_record_in_hot_cache(record, fd, &index);
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
		else
		{
			PVERBOSE("pid #%d fd #%d hot cache found at #%d\n", pid, fd, index);
		}
		
		//get the filename from record
		if (NULL != fd_record)
		{
			path = fd_record->filename;
			
			PDEBUG("pid #%d[fd #%d] is \"%s\"\n", pid, fd, path);
		}
		else
		{
			PDEBUG("pid #%d[fd #%d] does not exists\n", pid, fd);
		}

		up_read(&record->file_fd_sem);
	}
	else
	{
		PWARN("pid #%d process record not exists\n", pid);
	}

	return path;
}

void insert_into_cache(unsigned int fd, char * path)
{
	process_record * record = NULL;
	
	//lock process reocrd first
	lock_process_record();

	record = get_record_by_pid(current->pid);

	unlock_process_record();

	if (NULL != record)
	{
		file_fd_record * fd_record = NULL;
		
		//lock file fd also
		down_write(&record->file_fd_sem);

		fd_record = insert_into_link(record, fd, path);
		if (NULL != fd_record)
		{
			insert_into_hot_cache(record, fd_record);
			
			PDEBUG("pid #%d[fd #%d] updated to \"%s\"\n", current->pid, fd, path);
		}

		up_write(&record->file_fd_sem);
	}
	else
	{
		PWARN("pid #%d process record not exists\n", current->pid);
	}

	PVERBOSE("insert finish, current fd count: %d, hot count: %d\n", record->fd_count, record->hot_count);
}

bool delete_cache_by_fd(unsigned int fd)
{
	bool suc = false;
	process_record * record = NULL;

	lock_process_record();

	record = get_record_by_pid(current->pid);
	
	unlock_process_record();

	if (NULL != record)
	{
		down_write(&record->file_fd_sem);

		suc = delete_from_record(record, fd);

		up_write(&record->file_fd_sem);

		PDEBUG("pid #%d[fd #%d] delete result: %d\n", current->pid, fd, suc);
	}
	else
	{
		PWARN("pid #%d process record not exists\n", current->pid);
	}

	return suc;
}
