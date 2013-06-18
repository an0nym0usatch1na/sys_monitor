#include <linux/init.h>
#include <linux/module.h>
#include <linux/unistd.h>

#include "./../sys_monitor.h"
#include "./../log.h"
#include "./../trace_dog.h"
#include "./../process.h"
#include "./../fd_cache.h"

#define SYSMON_DEBUG
#include "./../debug.h"

#include "close.h"

//
// sys_close
//

typedef long (*lpfn_original_sys_close)(unsigned int fd);
lpfn_original_sys_close original_sys_close = NULL;

long fake_sys_close(unsigned int fd)
{
	bool log_ok = false;
	long result = 0;

	notify_enter();
	
	trace_dog_enter(api_sys_close);

	log_ok = begin_log_system_call(op_close_file, api_sys_close, get_cache_by_fd(fd), 1);
	
	if (log_ok)
	{
		add_unsigned_int_param("fd", fd);
	}
	
	result = original_sys_close(fd);
	
	if (log_ok)
	{
		end_log_system_call(result);
	}

	trace_dog_leave(api_sys_close);
	
	return result;
}

void close_operation_init(unsigned long ** sys_call_table)
{
	if (NULL != sys_call_table)
	{
		//replace system service function ptr
		original_sys_close = (lpfn_original_sys_close)sys_call_table[__NR_close];
		sys_call_table[__NR_close] = (unsigned long *)fake_sys_close;
		
		PDEBUG("replace sys_close, original: 0x%08x, fake: 0x%08x\n", original_sys_close, fake_sys_close);
	}
}

void close_operation_cleanup(unsigned long ** sys_call_table)
{
	if (NULL != sys_call_table)
	{
		if (NULL != original_sys_close)
		{
			sys_call_table[__NR_close] = (unsigned long *)original_sys_close;
		
			PDEBUG("restore sys_close, original: 0x%08x\n", original_sys_close);
		}
	}
}
