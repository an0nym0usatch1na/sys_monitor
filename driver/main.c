#include <linux/syscalls.h>
#include <linux/init.h>
#include <linux/module.h>

#include "sys_monitor.h"
#include "interface.h"
#include "log.h"
#include "trace_dog.h"
#include "process.h"
#include "./sys_calls/open.h"
#include "./sys_calls/read.h"
#include "./sys_calls/write.h"
#include "./sys_calls/close.h"
#include "./sys_calls/ioctl.h"
#include "./sys_calls/fork.h"
#include "./sys_calls/exit.h"
#include "./sys_calls/exec.h"

#define SYSMON_VERBOSE
#include "./../share/debug.h"

MODULE_LICENSE("GPL");

//
// global var definition
//

unsigned long ** sys_call_table = NULL;
int dev_major = 0;
int dev_minor = 0;

//
// find sys_call_table in kernel, fit arm cpu, kernel 2.4 - 3.7
//
unsigned long ** find_sys_call_table(void)
{
	int i = 0;
	unsigned long regCache[16] = { 0 };
	unsigned long * ptr = (unsigned long *)0xffff0008;
	unsigned long inst = *ptr;

	if (0x059ff000 == (inst & 0x0ffff000))	//ldr{cond} PC, [PC, #Immi] format
	{
		ptr = (unsigned long *)((inst & 0xfff) + 0xffff0008 + 8); //address = immi + pc
		ptr = (unsigned long *)*ptr;	//jump to addr

		for (i = 0; i < 100; i++)	//find in next 100 instructions
		{
			inst = *ptr;
			
			if (0xea000000 == (inst & 0xff))	//b xxx format, exit
			{			
				break;
			}
			else if (0xe28f0000 == (inst & 0xffff0000))	//add Rn, [PC, #Immi]
			{
				unsigned int rn = (inst >> 12) & 0xf;
				
				regCache[rn] = (inst & 0xfff) + 8 + (unsigned long)ptr;
			}
			else if (0x0790f000 == (inst & 0x0ff0f000))	//ldr{cond} PC, [Rn, Rm, #Immi]
			{
				unsigned int rn = (inst >> 16) & 0xf;
				
				return (unsigned long **)regCache[rn];
			}

			ptr++;
		}
	}
	
	return 0;
}

//
// init
//

static int sys_monitor_init(void)
{			
	int result = -EINVAL;
	dev_t dev = 0;

	PDEBUG("init started, module compiled at %s %s", __DATE__, __TIME__);

	sys_call_table = find_sys_call_table();
	if (NULL == sys_call_table)
	{
		PERROR("failed while get system call table\n");
	
		goto clean_final;
	}
	
	PDEBUG("system cale table address: 0x%08x!\n", (unsigned int)sys_call_table);
		
	if (!log_init())
	{
		PERROR("failed while init log\n");

		goto clean_final;
	}
	
	PDEBUG("log module init success\n");
			
	dev = MKDEV(0, 0);
	result = alloc_chrdev_region(&dev, 0, 1, "sys_monitor");
	if (result < 0) 
	{
		PERROR("failed while invoke register_chrdev_region, error: %d\n", result);
	
		goto clean_log;
	}
	
	dev_major = MAJOR(dev);
	dev_minor = MINOR(dev);
	
	PDEBUG("alloc chrdev at major %d, minor %d\n", dev_major, dev_minor);
   
	result = process_monitor_init();
	if (result < 0)
	{
		PERROR("failed while invoke process_monitor_init, error: %d\n", result);

		goto clean_alloc;
	}

	result = interface_init(dev);
	if (result < 0)
	{
		PERROR("failed while invoke interface_init, error: %d\n", result);
		
		goto clean_process;
	}

	result = trace_dog_initize();
	if (result < 0)
	{
		PERROR("failed while invoke trace_dong_initize, error: %d\n", result);
		
		goto clean_interface;
	}
		
	//open_operation_init(sys_call_table);
	//read_operation_init(sys_call_table);
	//write_operation_init(sys_call_table);
	//close_operation_init(sys_call_table);
	//ioctl_operation_init(sys_call_table);

	//fork_operation_init(sys_call_table);
	//exit_operation_init(sys_call_table);
	//exec_operation_init(sys_call_table);

	PDEBUG("replace completed, module init finish\n");

	//succ return	
	return 0;

clean_interface:
	interface_cleanup(dev);

clean_process:
	process_monitor_cleanup();

clean_alloc:
	unregister_chrdev_region(dev, 1);

clean_log:
	log_cleanup();
	
clean_final:

	//failed exit
	return result;
}

//
// clean up
//

static void sys_monitor_exit(void)
{
	dev_t dev = MKDEV(dev_major, dev_minor);

	//exec_operation_cleanup(sys_call_table);
	//exit_operation_cleanup(sys_call_table);
	//fork_operation_cleanup(sys_call_table);
	
	//ioctl_operation_cleanup(sys_call_table);
	//close_operation_cleanup(sys_call_table);
	//write_operation_cleanup(sys_call_table);
	//read_operation_cleanup(sys_call_table);
	//open_operation_cleanup(sys_call_table);
	
	trace_dog_cleanup();

	interface_cleanup(dev);

	process_monitor_cleanup();

	unregister_chrdev_region(dev, 1);
	
	log_cleanup();
	
	PDEBUG("module cleanup finish, goodbye!\n");
}

//
// export init and clean up function
//

module_init(sys_monitor_init);
module_exit(sys_monitor_exit);
