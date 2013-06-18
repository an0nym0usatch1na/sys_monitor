#include <linux/init.h>
#include <linux/module.h>
#include <linux/unistd.h>

#include "./../sys_monitor.h"
#include "./../log.h"
#include "./../trace_dog.h"
#include "./../process.h"

#define SYSMON_DEBUG
#include "./../debug.h"

#include "exit.h"

//
// sys_exit
//

typedef long (*lpfn_original_sys_exit)(int error_code);
lpfn_original_sys_exit original_sys_exit = NULL;

long fake_sys_exit(int error_code)
{
	bool log_ok = false;

	notify_enter();

	trace_dog_enter(api_sys_exit);

	log_ok = begin_log_system_call(op_exit_proc, api_sys_exit, NULL, 1);
	
	if (log_ok)
	{
		add_unsigned_int_param("error_code", error_code);
		
		//normally, sys_exit will never return, so we have to do everything before call real sys_exit
		end_log_system_call(0);
	}

	trace_dog_leave(api_sys_exit);

	return original_sys_exit(error_code);
}

void exit_operation_init(unsigned long ** sys_call_table)
{
	if (NULL != sys_call_table)
	{
		//replace system service function ptr
		original_sys_exit = (lpfn_original_sys_exit)sys_call_table[__NR_exit];
		sys_call_table[__NR_exit] = (unsigned long *)fake_sys_exit;
		
		PDEBUG("replace sys_exit, original: 0x%08x, fake: 0x%08x\n", original_sys_exit, fake_sys_exit);
	}
}

void exit_operation_cleanup(unsigned long ** sys_call_table)
{
	if (NULL != sys_call_table)
	{
		if (NULL != original_sys_exit)
		{
			sys_call_table[__NR_exit] = (unsigned long *)original_sys_exit;
		
			PDEBUG("restore sys_exit, original: 0x%08x\n", original_sys_exit);
		}
	}
}
