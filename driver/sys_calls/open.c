#include <linux/init.h>
#include <linux/module.h>
#include <linux/unistd.h>

#include "./../sys_monitor.h"
#include "./../log.h"
#include "./../trace_dog.h"
#include "./../process.h"
#include "./../fd_cache.h"

#define SYSMON_DEBUG
#include "./../../share/debug.h"

#include "open.h"

//
// sys_open
//

typedef long (*lpfn_original_sys_open)(const char __user *filename, int flags, int mode);
lpfn_original_sys_open original_sys_open = NULL;

long fake_sys_open(const char __user * filename, int flags, int mode)
{
	bool log_ok = false;
	long result = 0;

	PVERBOSE("sys_open(filename: %s, flags: %d, mode: %d) invoked\n", filename, flags, mode);

	notify_enter();

	trace_dog_enter(api_sys_open);

	log_ok = begin_log_system_call(op_create_file, api_sys_open, filename, 3);
	
	if (log_ok)
	{
		add_string_param("filename", filename);
		add_unsigned_int_param("flags", flags);
		add_unsigned_int_param("mode", mode);
	}
	
	result = original_sys_open(filename, flags, mode);
	
	if (log_ok)
	{
		end_log_system_call(result);
	}

	if (-1 != result)
	{
		//sys api success
		insert_into_cache((unsigned int)result, (char *)filename);	
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

	PVERBOSE("sys_creat(pathname: %s, mode: %d) invoked\n", pathname, mode);

	notify_enter();

	trace_dog_enter(api_sys_create);

	log_ok = begin_log_system_call(op_create_file, api_sys_create, pathname, 2);
	
	if (log_ok)
	{
		add_string_param("pathname", pathname);
		add_unsigned_int_param("mode", mode);
	}
	
	result = original_sys_creat(pathname, mode);
	
	if (log_ok)
	{
		end_log_system_call(result);
	}

	if (-1 != result)
	{
		insert_into_cache((unsigned int)result, (char *)pathname);
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
