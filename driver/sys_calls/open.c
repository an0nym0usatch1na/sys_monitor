#include <linux/init.h>
#include <linux/module.h>
#include <linux/unistd.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/highmem.h>
#include <linux/file.h>

#include "./../sys_monitor.h"
#include "./../log.h"
#include "./../trace_dog.h"
#include "./../process.h"
#include "./../fd_cache.h"

#define SYSMON_ERROR
#include "./../../share/debug.h"

#include "open.h"

char * get_absolute_path_by_fd(unsigned int fd)
{
	char * path = NULL;
	char * t_path = NULL;
	struct file * f = NULL;

	f = fget(fd);
	if (NULL != f)
	{
		int len = 2048;
		t_path = (char *)kmalloc(len, GFP_KERNEL);
		if (NULL != t_path)
		{
			char * ptr = NULL;
			
			ptr = d_path(&(f->f_path), t_path, len);
			if (!IS_ERR(t_path))
			{
				len = strlen(ptr);

				path = (char *)kmalloc(len + 1, GFP_KERNEL);
				if (NULL != path)
				{
					strcpy(path, ptr);
				}
			}

			kfree(t_path);
		}

		fput(f);
	}

	return path;
}

//
// sys_open
//

typedef long (*lpfn_original_sys_open)(const char __user *filename, int flags, int mode);
lpfn_original_sys_open original_sys_open = NULL;

long fake_sys_open(const char __user * filename, int flags, int mode)
{
	bool log_ok = false;
	long result = 0;

	trace_dog_enter(api_sys_open);

	notify_enter();

	PVERBOSE("sys_open(filename: %s, flags: %d, mode: %d) invoked\n", filename, flags, mode);

	//call real function at first because we need its result
	result = original_sys_open(filename, flags, mode);
	if (-1 != result)
	{
		char * full_name = NULL;

		//sys_open api success
		full_name = get_absolute_path_by_fd((unsigned int)result);
		if (NULL == full_name)
		{
			full_name = copy_string_from_user(filename);
			if (NULL == full_name)
			{
				full_name = copy_string("<NULL>");
			}
		}

		PVERBOSE("fd 0x%08x full path: %s\n", result, full_name);
		
		insert_into_cache((unsigned int)result, (char *) full_name);

		if (NULL != full_name)
		{
			kfree(full_name);
		}
	}

	//log event
	log_ok = begin_log_system_call_by_user_path(op_create_file, api_sys_open, filename, 3);
	if (log_ok)
	{
		add_string_param("filename", filename);
		add_unsigned_int_param("flags", flags);
		add_unsigned_int_param("mode", mode);
		
		end_log_system_call(result);
	}

	trace_dog_leave(api_sys_open);
	
	return result;
}

//
// sys_creat
//

typedef long (*lpfn_original_sys_creat)(const char __user *pathname, int mode);
lpfn_original_sys_creat original_sys_creat = NULL;

long fake_sys_creat(const char __user * pathname, int mode)
{
	bool log_ok = false;
	long result = 0;

	trace_dog_enter(api_sys_create);
	
	notify_enter();

	PVERBOSE("sys_creat(pathname: %s, mode: %d) invoked\n", pathname, mode);

	//call real function first
	result = original_sys_creat(pathname, mode);
	if (-1 != result)
	{
		char * full_name = NULL;

		//sys_open api success
		full_name = get_absolute_path_by_fd((unsigned int)result);
		if (NULL == full_name)
		{
			full_name = copy_string_from_user(pathname);
			if (NULL == full_name)
			{
				full_name = copy_string("<NULL>");
			}
		}

		PVERBOSE("fd 0x%08x full path: %s\n", result, full_name);
		
		insert_into_cache((unsigned int)result, (char *) full_name);

		if (NULL != full_name)
		{
			kfree(full_name);
		}
	}

	//log event
	log_ok = begin_log_system_call_by_user_path(op_create_file, api_sys_create, pathname, 2);
	if (log_ok)
	{
		add_string_param("pathname", pathname);
		add_unsigned_int_param("mode", mode);
		
		end_log_system_call(result);
	}
	
	trace_dog_leave(api_sys_create);
	
	return result;
}

void open_operation_init(unsigned long ** sys_call_table)
{
	if (NULL != sys_call_table)
	{
		//replace system service function ptr
		original_sys_open = (lpfn_original_sys_open)sys_call_table[__NR_open];
		sys_call_table[__NR_open] = (unsigned long *)fake_sys_open;
		
		PDEBUG("replace sys_open, original: 0x%08x, fake: 0x%08x\n", original_sys_open, fake_sys_open);
		
		original_sys_creat = (lpfn_original_sys_creat)sys_call_table[__NR_creat];
		sys_call_table[__NR_creat] = (unsigned long *)fake_sys_creat;
		
		PDEBUG("replace sys_creat, original: 0x%08x, fake: 0x%08x\n", original_sys_creat, fake_sys_creat);
	}
}

void open_operation_cleanup(unsigned long ** sys_call_table)
{
	if (NULL != sys_call_table)
	{
		if (NULL != original_sys_creat)
		{
			sys_call_table[__NR_creat] = (unsigned long *)original_sys_creat;
		
			PDEBUG("restore sys_creat, original: 0x%08x\n", original_sys_creat);
		}
	
		if (NULL != original_sys_open)
		{
			sys_call_table[__NR_open] = (unsigned long *)original_sys_open;
		
			PDEBUG("restore sys_open, original: 0x%08x\n", original_sys_open);
		}
	}
}
