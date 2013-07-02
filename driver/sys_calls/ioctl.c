#include <linux/init.h>
#include <linux/module.h>
#include <linux/unistd.h>

#include "./../sys_monitor.h"
#include "./../log.h"
#include "./../trace_dog.h"
#include "./../process.h"

#define SYSMON_DEBUG
#include "./../../share/debug.h"

#include "ioctl.h"

//
// sys_ioctl
//

typedef long (*lpfn_original_sys_ioctl)(unsigned int fd, unsigned int cmd, unsigned long arg);
lpfn_original_sys_ioctl original_sys_ioctl = NULL;

long fake_sys_ioctl(unsigned int fd, unsigned int cmd, unsigned long arg)
{
	bool log_ok = false;
	long result = 0;

	trace_dog_enter(api_sys_ioctl);

	notify_enter();

	log_ok = begin_log_system_call_by_fd(op_io_control_file, api_sys_ioctl, fd, 3);
	if (log_ok)
	{
		add_unsigned_int_param("fd", fd);
		add_unsigned_int_param("cmd", cmd);
		add_unsigned_int_param("arg", arg);
	}
	
	result = original_sys_ioctl(fd, cmd, arg);
	
	if (log_ok)
	{
		end_log_system_call(result);
	}

	trace_dog_leave(api_sys_ioctl);
	
	return result;
}

void ioctl_operation_init(unsigned long ** sys_call_table)
{
	if (NULL != sys_call_table)
	{
		//replace system service function ptr
		original_sys_ioctl = (lpfn_original_sys_ioctl)sys_call_table[__NR_ioctl];
		sys_call_table[__NR_ioctl] = (unsigned long *)fake_sys_ioctl;
		
		PDEBUG("replace sys_ioctl, original: 0x%08x, fake: 0x%08x\n", original_sys_ioctl, fake_sys_ioctl);
	}
}

void ioctl_operation_cleanup(unsigned long ** sys_call_table)
{
	if (NULL != sys_call_table)
	{
		if (NULL != original_sys_ioctl)
		{
			sys_call_table[__NR_ioctl] = (unsigned long *)original_sys_ioctl;
		
			PDEBUG("restore sys_ioctl, original: 0x%08x\n", original_sys_ioctl);
		}
	}
}
