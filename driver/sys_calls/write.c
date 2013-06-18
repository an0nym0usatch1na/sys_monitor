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

#include "write.h"

//
// sys_write
//

typedef long (*lpfn_original_sys_write)(unsigned int fd, const char __user * buf, size_t count);
lpfn_original_sys_write original_sys_write = NULL;

long fake_sys_write(unsigned int fd, const char __user * buf, size_t count)
{
	bool log_ok = false;
	long result = 0;

	notify_enter();

	trace_dog_enter(api_sys_write);

	log_ok = begin_log_system_call(op_write_file, api_sys_write, get_cache_by_fd(fd), 3);
	
	if (log_ok)
	{
		add_unsigned_int_param("fd", fd);
		add_pointer_param("buf", buf);
		add_unsigned_int_param("count", count);
	}
	
	result = original_sys_write(fd, buf, count);
	
	if (log_ok)
	{
		end_log_system_call(result);
	}

	trace_dog_leave(api_sys_write);
	
	return result;
}

void write_operation_init(unsigned long ** sys_call_table)
{
	if (NULL != sys_call_table)
	{
		//replace system service function ptr
		original_sys_write = (lpfn_original_sys_write)sys_call_table[__NR_write];
		sys_call_table[__NR_write] = (unsigned long *)fake_sys_write;
		
		PDEBUG("replace sys_write, original: 0x%08x, fake: 0x%08x\n", original_sys_write, fake_sys_write);
	}
}

void write_operation_cleanup(unsigned long ** sys_call_table)
{
	if (NULL != sys_call_table)
	{
		if (NULL != original_sys_write)
		{
			sys_call_table[__NR_write] = (unsigned long *)original_sys_write;
		
			PDEBUG("restore sys_write, original: 0x%08x\n", original_sys_write);
		}
	}
}
