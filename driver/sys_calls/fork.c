#include <linux/init.h>
#include <linux/module.h>
#include <linux/unistd.h>

#include "./../sys_monitor.h"
#include "./../log.h"
#include "./../trace_dog.h"
#include "./../process.h"

#define SYSMON_DEBUG
#include "./../../share/debug.h"

#include "fork.h"

//
// sys_clone
//

typedef long (*lpfn_original_sys_clone)(unsigned long clone_flags, 
										unsigned long newsp,
										int __user *parent_tidptr, 
										int tls_val,
										int __user *child_tidptr, 
										struct pt_regs *regs);
typedef void (*lpfn_original_sys_clone_wrapper)(void);

lpfn_original_sys_clone original_sys_clone = NULL;
lpfn_original_sys_clone_wrapper original_sys_clone_wrapper = NULL;
bool b_sys_clone_wrapper = false;

void fake_sys_clone_wrapper(void) __attribute__((naked));
long fake_sys_clone(unsigned long clone_flags, 
					unsigned long newsp,
					int __user *parent_tidptr, 
					int tls_val,
					int __user *child_tidptr, 
					struct pt_regs *regs);

void fake_sys_clone_wrapper(void)
{
	asm("nop");
	asm("nop");
	asm("nop");
	asm("nop");

	/*
		add ip, sp, #S_OFF;
		str ip, [sp, #4];
		b fake_sys_clone;
	*/
}

long fake_sys_clone(unsigned long clone_flags, 
					unsigned long newsp,
					int __user *parent_tidptr, 
					int tls_val,
					int __user *child_tidptr, 
					struct pt_regs *regs)
{
	bool log_ok = false;
	long result = 0;

	notify_enter();

	trace_dog_enter(api_sys_clone);

	log_ok = begin_log_system_call(op_fork_proc, api_sys_clone, NULL, 6);
	
	if (log_ok)
	{
		add_unsigned_int_param("clone_flags", clone_flags);
		add_unsigned_int_param("newsp", newsp);
		add_pointer_param("parent_tidptr", (unsigned char *)parent_tidptr);
		add_unsigned_int_param("tls_val", tls_val);
		add_pointer_param("child_tidptr", (unsigned char *)child_tidptr);
		add_pointer_param("regs", (unsigned char *)regs);
	}
	
	result = original_sys_clone(clone_flags, newsp, parent_tidptr, tls_val, child_tidptr, regs);
	
	if (log_ok)
	{
		end_log_system_call(result);
	}

	trace_dog_leave(api_sys_clone);
	
	return result;
}

//
// sys_vfork
//

typedef long (*lpfn_original_sys_vfork)(struct pt_regs * regs);
typedef void (*lpfn_original_sys_vfork_wrapper)(void);

lpfn_original_sys_vfork original_sys_vfork = NULL;
lpfn_original_sys_vfork_wrapper original_sys_vfork_wrapper = NULL;
bool b_sys_vfork_wrapper = false;

void fake_sys_vfork_wrapper(void) __attribute__((naked));
long fake_sys_vfork(struct pt_regs * regs);

void fake_sys_vfork_wrapper(void)
{
	asm("nop");
	asm("nop");
	asm("nop");
	asm("nop");

	/*
		add r0, sp, #8;
		b fake_sys_vfork;
	*/
}

long fake_sys_vfork(struct pt_regs * regs)
{
	bool log_ok = false;
	long result = 0;

	notify_enter();

	trace_dog_enter(api_sys_vfork);

	log_ok = begin_log_system_call(op_fork_proc, api_sys_vfork, NULL, 1);
	
	if (log_ok)
	{
		add_pointer_param("regs", (unsigned char *)regs);
	}
	
	result = original_sys_vfork(regs);
	
	if (log_ok)
	{
		end_log_system_call(result);
	}

	trace_dog_leave(api_sys_vfork);
	
	return result;
}

//
// sys_fork
//

typedef long (*lpfn_original_sys_fork)(struct pt_regs * regs);
typedef void (*lpfn_original_sys_fork_wrapper)(void);

lpfn_original_sys_fork original_sys_fork = NULL;
lpfn_original_sys_fork_wrapper original_sys_fork_wrapper = NULL;
bool b_sys_fork_wrapper = false;

void fake_sys_fork_wrapper(void) __attribute__((naked));
long fake_sys_fork(struct pt_regs * regs);

void fake_sys_fork_wrapper(void)
{
	asm("nop");
	asm("nop");
	asm("nop");
	asm("nop");

	/*
		add r0, sp, #8;
		b fake_sys_fork;
	*/
}

long fake_sys_fork(struct pt_regs * regs)
{
	bool log_ok = false;
	long result = 0;

	notify_enter();

	trace_dog_enter(api_sys_fork);

	log_ok = begin_log_system_call(op_fork_proc, api_sys_fork, NULL, 1);
	
	if (log_ok)
	{
		add_pointer_param("regs", (unsigned char *)regs);
	}

	result = original_sys_fork(regs);
	
	if (log_ok)
	{
		end_log_system_call(result);
	}

	trace_dog_leave(api_sys_fork);
	
	return result;
}

void fork_operation_init(unsigned long ** sys_call_table)
{
	if (NULL != sys_call_table)
	{
		unsigned int inst_a, inst_b, inst_c, addr;

		//first, get sys_call_table + offset(sys_fork, sys_vfork, sys_clone)
		original_sys_fork = (lpfn_original_sys_fork)sys_call_table[__NR_fork];
		original_sys_vfork = (lpfn_original_sys_vfork)sys_call_table[__NR_vfork];
		original_sys_clone = (lpfn_original_sys_clone)sys_call_table[__NR_clone];
		
		//check fork wrapper
		inst_a = *((unsigned int *)original_sys_fork);
		inst_b = *((unsigned int *)original_sys_fork + 1);
		if ((0xffff0000 & inst_a) == 0xe28d0000)	//add r0, sp, #xxx
		{
			if ((0xff000000 & inst_b) == 0xea000000) 	//b xxx
			{
				original_sys_fork_wrapper = (lpfn_original_sys_fork_wrapper)original_sys_fork;
				original_sys_fork = (lpfn_original_sys_fork)((unsigned int)original_sys_fork_wrapper + (inst_b & 0x00ffffff) * 4 + 12);
				b_sys_fork_wrapper = true;
			}
		}

		PDEBUG("check sys_fork wrapper result: %s, sys_fork: 0x%08x, sys_fork_wrapper: 0x%08x\n", (b_sys_fork_wrapper)? "true" : "false", original_sys_fork, original_sys_fork_wrapper);

		//check vfork wrapper
		inst_a = *((unsigned int *)original_sys_vfork);
		inst_b = *((unsigned int *)original_sys_vfork + 1);
		if ((0xffff0000 & inst_a) == 0xe28d0000)	//add r0, sp, #xxx
		{
			if ((0xff000000 & inst_b) == 0xea000000) 	//b xxx
			{
				original_sys_vfork_wrapper = (lpfn_original_sys_vfork_wrapper)original_sys_vfork;
				original_sys_vfork = (lpfn_original_sys_vfork)((unsigned int)original_sys_vfork_wrapper + (inst_b & 0x00ffffff) * 4 + 12);
				b_sys_vfork_wrapper = true;
			}
		}

		PDEBUG("check sys_vfork wrapper result: %s, sys_vfork: 0x%08x, sys_vfork_wrapper: 0x%08x\n", (b_sys_vfork_wrapper)? "true" : "false", original_sys_vfork, original_sys_vfork_wrapper);

		//check clone wrapper
		inst_a = *((unsigned int *)original_sys_clone);
		inst_b = *((unsigned int *)original_sys_clone + 1);
		inst_c = *((unsigned int *)original_sys_clone + 2);
		if ((0xffff0000 & inst_a) == 0xe28d0000)	//add r0, sp, #xxx
		{
			if ((0xff000000 & inst_c) == 0xea000000) 	//b xxx
			{
				original_sys_clone_wrapper = (lpfn_original_sys_clone_wrapper)original_sys_clone;
				original_sys_clone = (lpfn_original_sys_clone)((unsigned int)original_sys_clone_wrapper + (inst_c & 0x00ffffff) * 4 + 16);
				b_sys_clone_wrapper = true;
			}
		}

		PDEBUG("check sys_clone wrapper result: %s, sys_clone: 0x%08x, sys_clone_wrapper: 0x%08x\n", (b_sys_clone_wrapper)? "true" : "false", original_sys_clone, original_sys_clone_wrapper);

		//deal with fork
		if (b_sys_fork_wrapper)
		{
			//copy wrapper to fake_sys_wrapper, currently two ops
			memcpy(fake_sys_fork_wrapper, original_sys_fork_wrapper, 4 * 2);

			//update wrapper b fake_sys_fork op
			addr = ((unsigned int)fake_sys_fork - (unsigned int)fake_sys_fork_wrapper - 12) / 4;	
			*((unsigned int *)fake_sys_fork_wrapper + 1) = 0xea000000 + addr;

			//finally, replace system service function ptr
			sys_call_table[__NR_fork] = (unsigned long *)fake_sys_fork_wrapper;

			PDEBUG("replace sys_fork with wrapper, original wrapper: 0x%08x, fake wrapper: 0x%08x, fake: 0x%08x\n", original_sys_fork_wrapper, fake_sys_fork_wrapper, fake_sys_fork);
		}
		else
		{
			//no wrapper, directly replace
			sys_call_table[__NR_fork] = (unsigned long *)fake_sys_fork;

			PDEBUG("replace sys_fork, original: 0x%08x, fake: 0x%08x\n", original_sys_fork, fake_sys_fork);
		}

		//deal with vfork
		if (b_sys_vfork_wrapper)
		{
			//copy wrapper to fake_sys_vfork_wrapper, currently two ops
			memcpy(fake_sys_vfork_wrapper, original_sys_vfork_wrapper, 4 * 2);

			//update wrapper b fake_sys_fork op
			addr = ((unsigned int)fake_sys_vfork - (unsigned int)fake_sys_vfork_wrapper - 12) / 4;	
			*((unsigned int *)fake_sys_vfork_wrapper + 1) = 0xea000000 + addr;

			//finally, replace system service function ptr
			sys_call_table[__NR_vfork] = (unsigned long *)fake_sys_vfork_wrapper;

			PDEBUG("replace sys_vfork with wrapper, original wrapper: 0x%08x, fake wrapper: 0x%08x, fake: 0x%08x\n", original_sys_vfork_wrapper, fake_sys_vfork_wrapper, fake_sys_vfork);
		}
		else
		{
			//no wrapper, directly replace
			sys_call_table[__NR_fork] = (unsigned long *)fake_sys_vfork;

			PDEBUG("replace sys_vfork, original: 0x%08x, fake: 0x%08x\n", original_sys_vfork, fake_sys_vfork);
		}

		//deal with clone
		if (b_sys_clone_wrapper)
		{
			//copy wrapper to fake_sys_clone_wrapper, currently three ops
			memcpy(fake_sys_clone_wrapper, original_sys_clone_wrapper, 4 * 3);

			//update wrapper b fake_sys_clone op
			addr = ((unsigned int)fake_sys_clone - (unsigned int)fake_sys_clone_wrapper - 16) / 4;	
			*((unsigned int *)fake_sys_clone_wrapper + 2) = 0xea000000 + addr;

			//finally, replace system service function ptr
			sys_call_table[__NR_clone] = (unsigned long *)fake_sys_clone_wrapper;

			PDEBUG("replace sys_clone with wrapper, original wrapper: 0x%08x, fake wrapper: 0x%08x, fake: 0x%08x\n", original_sys_clone_wrapper, fake_sys_clone_wrapper, fake_sys_clone);
		}
		else
		{
			//no wrapper, directly replace
			sys_call_table[__NR_clone] = (unsigned long *)fake_sys_clone;

			PDEBUG("replace sys_clone, original: 0x%08x, fake: 0x%08x\n", original_sys_clone, fake_sys_clone);
		}
	}
}

void fork_operation_cleanup(unsigned long ** sys_call_table)
{
	if (NULL != sys_call_table)
	{
		if (b_sys_fork_wrapper)
		{
			if (NULL != original_sys_fork_wrapper)
			{
				sys_call_table[__NR_fork] = (unsigned long *)original_sys_fork_wrapper;

				PDEBUG("restore sys_fork wrapper, original: 0x%08x\n", original_sys_fork_wrapper);
			}
		}
		else
		{
			if (NULL != original_sys_fork)
			{
				sys_call_table[__NR_fork] = (unsigned long *)original_sys_fork;
		
				PDEBUG("restore sys_fork, original: 0x%08x\n", original_sys_fork);
			}
		}

		if (b_sys_vfork_wrapper)
		{
			if (NULL != original_sys_vfork_wrapper)
			{
				sys_call_table[__NR_vfork] = (unsigned long *)original_sys_vfork_wrapper;

				PDEBUG("restore sys_vfork wrapper, original: 0x%08x\n", original_sys_vfork_wrapper);
			}
		}
		else
		{
			if (NULL != original_sys_vfork)
			{
				sys_call_table[__NR_vfork] = (unsigned long *)original_sys_vfork;
		
				PDEBUG("restore sys_vfork, original: 0x%08x\n", original_sys_vfork);
			}
		}

		if (b_sys_clone_wrapper)
		{
			if (NULL != original_sys_clone_wrapper)
			{
				sys_call_table[__NR_clone] = (unsigned long *)original_sys_clone_wrapper;

				PDEBUG("restore sys_clone wrapper, original: 0x%08x\n", original_sys_clone_wrapper);
			}
		}
		else
		{
			if (NULL != original_sys_clone)
			{
				sys_call_table[__NR_clone] = (unsigned long *)original_sys_clone;
		
				PDEBUG("restore sys_clone, original: 0x%08x\n", original_sys_clone);
			}
		}
	}
}
