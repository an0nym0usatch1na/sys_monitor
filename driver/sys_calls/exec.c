#include <linux/init.h>
#include <linux/module.h>
#include <linux/unistd.h>

#include "./../sys_monitor.h"
#include "./../log.h"
#include "./../trace_dog.h"
#include "./../process.h"

#define SYSMON_DEBUG
#include "./../debug.h"

#include "exec.h"

//
// sys_exec
//

typedef long (*lpfn_original_sys_exec)(const char __user * filenamei,
									   const char __user * const __user * argv, 
									   const char __user * const __user * envp,
									   struct pt_regs * regs);
typedef void (*lpfn_original_sys_exec_wrapper)(void);

lpfn_original_sys_exec original_sys_exec = NULL;
lpfn_original_sys_exec_wrapper original_sys_exec_wrapper = NULL;
bool b_sys_exec_wrapper = false;

long fake_sys_exec_wrapper(void) __attribute__((naked));
long fake_sys_exec(const char __user * filenamei,
				   const char __user * const __user * argv,
				   const char __user * const __user * envp,
				   struct pt_regs * regs);

long fake_sys_exec_wrapper(void)
{
	asm("nop");
	asm("nop");
	
	/*
		add r0, sp, #8;
		b fake_sys_exec;
	*/
}

long fake_sys_exec(const char __user * filenamei,
				   const char __user * const __user * argv,
				   const char __user * const __user * envp,
				   struct pt_regs * regs)
{
	bool log_ok = false;

	notify_enter();

	notify_execve();

	trace_dog_enter(api_sys_execve);

	log_ok = begin_log_system_call(op_execve_proc, api_sys_execve, NULL, 4);
	
	if (log_ok)
	{
		add_string_param("filenamei", filenamei);
		add_pointer_param("argv", argv);
		add_pointer_param("envp", envp);
		add_pointer_param("regs", regs);

		end_log_system_call(0);
	}

	trace_dog_leave(api_sys_execve);

	PDEBUG("sys_execve(\"%s\")\n", filenamei);

	// this api will not return on success, so we can not get its result
	return original_sys_exec(filenamei, argv, envp, regs);
}

void exec_operation_init(unsigned long ** sys_call_table)
{
	if (NULL != sys_call_table)
	{
		unsigned int inst_a, inst_b, addr;

		//first, get sys_call_table + offset_sys_execve
		original_sys_exec = (lpfn_original_sys_exec)sys_call_table[__NR_execve];
		
		//check wrapper
		inst_a = *((unsigned int *)original_sys_exec);
		inst_b = *((unsigned int *)original_sys_exec + 1);
		if ((0xffff0000 & inst_a) == 0xe28d0000)	//add r0, sp, #xxx
		{
			if ((0xff000000 & inst_b) == 0xea000000) 	//b xxx
			{
				original_sys_exec_wrapper = original_sys_exec;
				original_sys_exec = ((unsigned int)original_sys_exec_wrapper + (inst_b & 0x00ffffff) * 4 + 12);
				b_sys_exec_wrapper = true;
			}
		}

		PDEBUG("check sys_execve wrapper result: %s, sys_execve: 0x%08x, sys_execve_wrapper: 0x%08x\n", (b_sys_exec_wrapper)? "true" : "false", original_sys_exec, original_sys_exec_wrapper);

		if (b_sys_exec_wrapper)
		{
			//copy wrapper to fake_sys_wrapper, currently two ops
			memcpy(fake_sys_exec_wrapper, original_sys_exec_wrapper, 4 * 2);

			//update wrapper b fake_sys_exec op
			addr = ((unsigned int)fake_sys_exec - (unsigned int)fake_sys_exec_wrapper - 12) / 4;	
			*((unsigned int *)fake_sys_exec_wrapper + 1) = 0xea000000 + addr;

			//finally, replace system service function ptr
			sys_call_table[__NR_execve] = (unsigned long *)fake_sys_exec_wrapper;

			PDEBUG("replace sys_execve with wrapper, original wrapper: 0x%08x, fake wrapper: 0x%08x, fake: 0x%08x\n", original_sys_exec_wrapper, fake_sys_exec_wrapper, fake_sys_exec);
		}
		else
		{
			//no wrapper, directly replace
			sys_call_table[__NR_execve] = (unsigned long *)fake_sys_exec;

			PDEBUG("replace sys_execve, original: 0x%08x, fake: 0x%08x\n", original_sys_exec, fake_sys_exec);
		}
	}
}

void exec_operation_cleanup(unsigned long ** sys_call_table)
{
	if (NULL != sys_call_table)
	{
		if (b_sys_exec_wrapper)
		{
			if (NULL != original_sys_exec_wrapper)
			{
				sys_call_table[__NR_execve] = original_sys_exec_wrapper;

				PDEBUG("restore sys_execve wrapper, original: 0x%08x\n", original_sys_exec_wrapper);
			}
		}
		else
		{
			if (NULL != original_sys_exec)
			{
				sys_call_table[__NR_execve] = original_sys_exec;
		
				PDEBUG("restore sys_execve, original: 0x%08x\n", original_sys_exec);
			}
		}
	}
}
