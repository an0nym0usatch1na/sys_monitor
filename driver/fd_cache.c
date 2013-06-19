#include <linux/sched.h>
#include <linux/unistd.h>
#include <linux/init.h>
#include <linux/module.h>
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

//initize an new fd cache area of process record
void fd_cache_initize(process_record * record)
{
	init_rwsem(&record->file_fd_sem);
    
	record->fd_count = 0;
	record->hot_count = 0;
	record->fd_indexer = 0;
    record->file_fd_head = NULL;
    
	memset(record->hot_fd_cache, 0, sizeof(void *) * HOT_FD_CACHE_SIZE);
	memset(record->hot_cache_time, 0, sizeof(int) * HOT_FD_CACHE_SIZE);
}

//cleanup fd cache area resource
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
}

file_fd_record * find_record_in_hot_cache(process_record * record, int fd)
{
	return NULL;
}

file_fd_record * find_record_in_link(process_record * record, int fd)
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

//get index equal or nearest bigger
int get_insert_index(process_record * record, int fd)
{
	int fit_fd = 0;
	int l = 0;
	int r = record->hot_count - 1;

	while (l <= r)
	{
		int m = (l + r) / 2;
		int prev_fd = -1;
		int next_fd = 0x7FFFFFFF;
		int t_fd = -1;

		//assert last
		fit_fd = record->hot_count - 1;

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
			fit_fd = t_fd;

			break;
		}
		
		if (t_fd <= fd && next_fd >= fd)
		{
			fit_fd = t_fd + 1;

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
}

void insert_into_hot_cache(process_record * record, file_fd_record * fd_record)
{
	int i = 0;
	//first, find an fit index
	int fit_index = get_insert_index(record, fd_record->fd);
		
	if (record->hot_count == HOT_FD_CACHE_SIZE)
	{
		//full, we need to make an room
		int index = kill_oldest_cache(record);
	}
	else
	{
		//find fit index
		if (-1 == fit_index)
		{
			//into the last one
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
			record->hot_cache_time[fit_index] = __FIX_ME__;

			//add reference
			record->hot_count++;
		}
	}

	//find best fit room

	//settle down
}

//allocate and insert into current process record`s fd record link
file_fd_record * insert_into_link(process_record * record, int fd, char * path)
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

char * get_cache_by_fd(int fd)
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

void insert_into_cache(int fd, char * path)
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
