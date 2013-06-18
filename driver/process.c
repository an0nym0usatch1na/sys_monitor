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

#define SYSMON_DEBUG
#include "debug.h"

//main cache
process_record ** process_cache = NULL;
//for speed up
process_record * outstanding_header = NULL;
//sem for sync
struct rw_semaphore proc_sem;

//function declartion
void fd_cache_initize(process_record * record);

void fd_cache_cleanup(process_record * record);

int process_monitor_init(void) 
{
#ifdef _DEBUG
    int i = 0;
#endif

    int size = sizeof(void *) * HASH_SLOT_SIZE;

    process_cache = (process_record **)kmalloc(size, GFP_KERNEL);
    if (NULL == process_cache)
    {
        //alloc failed
        PERROR("allocate memory failed\n");

        return -1;
    }

    memset(process_cache, '\0', size);

#ifdef _DEBUG
    for (i = 0; i < size ; i++)
    {
        if (0 != *((char *)process_cache + i))
        {
            //memset wrong?
            PERROR("memset  error????\n");
        }
    }
#endif

	outstanding_header = NULL;
	init_rwsem(&proc_sem);

    PDEBUG("log module initize success, total %d byte(s) allocated\n", size);

    //success
    return 0;
}

void cleanup_process_record(process_record * record)
{
	if (NULL != record->filename)
	{
		kfree(record->filename);
	}

	if (NULL != record->argv)
	{
		kfree(record->argv);
	}

	if (NULL != record->env)
	{
		kfree(record->env);
	}

	fd_cache_cleanup(record);
}

void process_monitor_cleanup(void)
{
	process_record * p = outstanding_header;

	while (NULL != p)
	{
		process_record * t = p;
		
		cleanup_process_record(p);

		p = p->next;
		kfree(t);
	}

	if (NULL != process_cache)
	{
		kfree(process_cache);
	}

	PDEBUG("complete cleanup process cache\n");
}

int get_current_process_id(void)
{
    //use pid as slot id, it works under none muti-thread os
    int id = current->pid;
    if (current->pid < 0 || current->pid >= HASH_SLOT_SIZE)
    {
        PERROR("received an error pid %d\n", id);
						        
        id = -1;
    }

   	return id;
}

//read process memory safely with mm lock
int read_process_memory_locked(struct task_struct * tsk, unsigned long addr, void * buf, int len)
{
	struct mm_struct * mm = NULL;
	struct vm_area_struct * vma = NULL;
	struct page * page = NULL;
	void * old_buf = buf;

    mm = get_task_mm(tsk);
    if (!mm)
	{
		// get task mm struct failed
		PERROR("get_task_mm failed\n");

		return 0;
	}

	while (len) 
	{
    	int bytes = 0, ret = 0, offset = 0;
        void * maddr = NULL;
        
		ret = get_user_pages(tsk, mm, addr, 1, 0, 1, &page, &vma);
        if (ret <= 0)
		{
        	break;
		}
        
		bytes = len;
        offset = addr & (PAGE_SIZE - 1);
       	
		if (bytes > PAGE_SIZE - offset)
		{
        	bytes = PAGE_SIZE - offset;
		}

		maddr = kmap(page);
        
		copy_from_user_page(vma, page, addr, buf, maddr + offset, bytes);
        
		kunmap(page);
        
		page_cache_release(page);
     	
	  	len -= bytes;
		buf += bytes;
		addr += bytes;	
	}	

	return buf - old_buf;
}

char * get_current_process_arguments(int * length) 
{
	char * arg_buffer = NULL;
    struct mm_struct * mm = NULL;
	struct task_struct * task = current;

	*length = 0;

    if (NULL != task->mm)
	{
		int len = 0;

		mm = task->mm;

		down_read(&mm->mmap_sem);

		//calculate argument size
		len = mm->arg_end - mm->arg_start;
		if (len >= 0)
		{
			arg_buffer = (char *)kmalloc(len + 1, GFP_KERNEL);
			if (NULL != arg_buffer)
			{
				//read argument buffer from memory
				int read = read_process_memory_locked(current, mm->arg_start, arg_buffer, len);
				if (read != len)
				{
					PWARN("read process memory need %d, but only %d byte(s) read\n", len, read);
				}

				arg_buffer[read] = '\0';

				*length = read;

				PVERBOSE("current process arguments length: %d\n", len);
			}
			else
			{
				PERROR("allocate memory failed\n");
			}
		}

		up_read(&mm->mmap_sem);
	}	

	return arg_buffer;
}

char * get_current_process_environment(int * length)
{
	char * env_buffer = NULL;

	*length = 0;

	return env_buffer;
}

char * get_current_process_path(void) 
{
	char * path = NULL;
	struct task_struct * task = current;

	if (NULL != task->mm &&
		NULL != task->mm->exe_file) 
	{
		char * tmp = NULL;

		down_read(&task->mm->mmap_sem);
		
		tmp = (char *)kmalloc(PATH_MAX, GFP_KERNEL);
		if (NULL != tmp)
		{
			char * ptr = d_path(&task->mm->exe_file->f_path, tmp, PATH_MAX);
			if (!IS_ERR(ptr))
			{
				int len = strlen(ptr);
				path = (char *)kmalloc(len + 1, GFP_KERNEL);
				if (NULL != path) 
				{
					strcpy(path, ptr);

					PVERBOSE("get current process path \"%s\%\n", ptr);
				}
				else
				{
					PERROR("allocate memory failed\n");
				}
			}
			else	
			{
				PERROR("get file path failed, error code: 0x%08x\n", ptr);

				ptr = NULL;
			}

			kfree(tmp);
		}
		else 
		{
			PERROR("allocate memory failed\n");
		}

		up_read(&task->mm->mmap_sem);
	}
	else
	{
		PERROR("current task struct error\n");
	}

	return path;
}

process_record * build_process_record(process_record * record)
{
	bool new_alloc = false;

	if (NULL == record)
	{
		record = (process_record *)kmalloc(sizeof(process_record), GFP_KERNEL);
		if (NULL == record)
		{
			PERROR("allocate memory failed\n");

			return NULL;
		}

		new_alloc = true;
	}

	//arguments
	record->filename = get_current_process_path();
	record->argv = get_current_process_arguments(&record->argv_length);
	record->env = get_current_process_environment(&record->env_length);

	//time related
	record->before_execve = false;
	record->start_time.tv_sec = current->real_start_time.tv_sec;
	record->start_time.tv_nsec = current->real_start_time.tv_nsec;

	//link
	if (new_alloc)
	{
		if (NULL == outstanding_header)
		{
			outstanding_header = record;

			record->next = NULL;
			record->prev = NULL;
		}	
		else
		{
			record->prev = NULL;
			record->next = outstanding_header;
			outstanding_header->prev = record;
			outstanding_header = record;
		}
	}

	//file fd
	if (!new_alloc)
	{
		fd_cache_cleanup(record);
	}
	fd_cache_initize(record);

	return record;
}

void update_process_record(process_record * record)
{
	if (NULL != record)
	{
		build_process_record(record);
	}	
}

void notify_create(void)
{
	//currently do nonthing
}

void notify_exit(void)
{
	//currently do nothing
}

void notify_enter(void)
{
	process_record * record = NULL;
	int id = get_current_process_id();

	PVERBOSE("notify_enter executed, current process id is %d\n", id);

	if (-1 != id)
	{
		down_write(&proc_sem);

		record = process_cache[id];

		if (NULL != record) 
		{
			//record already exists, we need to identify it is an new process or previous recorded process
			if (record->start_time.tv_sec == current->real_start_time.tv_sec &&
				record->start_time.tv_nsec == current->real_start_time.tv_nsec)
			{
				//verified, previous recorded process
				if (record->before_execve)
				{
					//before execve flag set, we need to update process info
					update_process_record(record);

					//clear the flag
					record->before_execve = false;

					PDEBUG("process cache #%d updated to \"%s\" by before_execve flags\n", id, record->filename);
				}
				else
				{
					PVERBOSE("process cahce #%d referenced normally\n", id);
				}
			}
			else
			{
				//new process but used previous pid, clean it first
				cleanup_process_record(record);

				build_process_record(record);

				PDEBUG("process cache #%d reused to \"%s\"\n", id, record->filename);
			}
		}
		else
		{
			//well, first time
			record = build_process_record(NULL);
			if (NULL != record)
			{
				process_cache[id] = record;

				PDEBUG("process cache #%d updated from null to \"%s\"\n", id, record->filename);
			}
		}

		up_write(&proc_sem);
	}
}

void notify_execve(void)
{
	int id = get_current_process_id();

	PVERBOSE("notify_execve executed, current process id is %d\n", id);

	if (-1 != id)
	{
		down_write(&proc_sem);

		if (NULL != process_cache[id])
		{
			//set this flag to notify process path may changed
			process_cache[id]->before_execve = true;

			PDEBUG("set process \"%s\" (#%d) before_execve flag\n", get_process_path_by_pid((pid_t)id), id);
		}
		else
		{
			PWARN("notify_execve got an null process record\n");
		}

		up_write(&proc_sem);
	}
}

void lock_process_record(void)
{
	down_read(&proc_sem);
}

void unlock_process_record(void)
{
	up_read(&proc_sem);
}

process_record * get_record_by_pid(pid_t pid)
{
	int id = (int)pid;
	if (id < 0 || id >= HASH_SLOT_SIZE)
	{
		return NULL;
	}

	return process_cache[(int)pid];
}

char * get_process_path_by_pid(pid_t pid)
{
	char * path = NULL;
	process_record * record = get_record_by_pid(pid);
	
	if (NULL != record)
	{
		path = record->filename;
	}

	return path;
}
