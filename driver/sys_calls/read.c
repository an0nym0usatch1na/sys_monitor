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

#include "read.h"

//
// sys_read
//

typedef long (*lpfn_original_sys_read)(unsigned int fd, char __user * buf, size_t count);
lpfn_original_sys_read original_sys_read = NULL;

long fake_sys_read(unsigned int fd, char __user * buf, size_t count)
{
	bool log_ok = false;
	long result = 0;

	notify_enter();

	trace_dog_enter(api_sys_read);

	log_ok = begin_log_system_call(op_read_file, api_sys_read, get_cache_by_fd(fd), 3);
	
	if (log_ok)
	{
		add_unsigned_int_param("fd", fd);
		add_pointer_param("buf", buf);
		add_int_param("count", count);
	}

	result = original_sys_read(fd, buf, count);
	
	if (log_ok)
	{
		end_log_system_call(result);
	}

	trace_dog_leave(api_sys_read);

	return result;
}

void read_operation_init(unsigned long ** sys_call_table)
{
	if (NULL != sys_call_table)
	{
		//replace system service function ptr
		original_sys_read = (lpfn_original_sys_read)sys_call_table[__NR_read];
		sys_call_table[__NR_read] = (unsigned long *)fake_sys_read;
		
		PDEBUG("replace sys_read, original: 0x%08x, fake: 0x%08x\n", original_sys_read, fake_sys_read);
	}
}

void read_operation_cleanup(unsigned long ** sys_call_table)
{
	if (NULL != sys_call_table)
	{
		if (NULL != original_sys_read)
		{
			sys_call_table[__NR_read] = (unsigned long *)original_sys_read;
		
			PDEBUG("restore sys_read, original: 0x%08x\n", original_sys_read);
		}
	}
}
