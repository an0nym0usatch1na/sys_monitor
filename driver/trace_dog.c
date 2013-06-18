#include <linux/syscalls.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/delay.h>

#include "sys_monitor.h"
#include "log.h"
#include "trace_dog.h"

#define SYSMON_DEBUG
#include "./../share/debug.h"

//global vars
int g_ref_count = 0;
int ref_table[api_max] = { 0 };
trace_item * ref_link = NULL;

struct semaphore trace_dog_sem;

//initize routine of trace dog
int trace_dog_initize(void)
{
	sema_init(&trace_dog_sem, 1);

	PDEBUG("trace dog initize finish\n");

	return 0;
}

//cleanup routine of trace dog
void trace_dog_cleanup(void)
{
	wait_for_unload();

	PDEBUG("trace dog cleanup success\n");
}

//wait for the time that module can unload
void wait_for_unload(void)
{
	while (true) 
	{
		int ref_count = 0;

		if (down_interruptible(&trace_dog_sem))
		{
       		break;
	    }   
	    
	    ref_count = g_ref_count;
	
	    up(&trace_dog_sem);

		if (0 != ref_count)
		{
			//well, we need to wait
			PDEBUG("trace dog now going to wait, %d ref count(s) remains\n", ref_count);

			//sleep 500 ms
			msleep(500);
		}
		else
		{
			//all ref has gone
			break;
		}
	}

	//wait another 500ms, let api exit safely
	msleep(500);

	PDEBUG("all ref to sys api has gone, exit wait\n");
}

//tell trace dog that we have enter an sys_call api
void trace_dog_enter(api_name api)
{
    if (down_interruptible(&trace_dog_sem))
	{
		return;
    }

	ref_table[api]++;
	g_ref_count++;

	PVERBOSE("trace dog enter, api: %s, global ref(s): %d\n", api_to_string(api), g_ref_count);

	up(&trace_dog_sem);
}

//tell trace dog that we have leave an sys_call api
void trace_dog_leave(api_name api)
{
    if (down_interruptible(&trace_dog_sem))
    {
	    return;
    }   
    
    ref_table[api]--;
	g_ref_count--;

	PVERBOSE("trace dog leave, api: %d, global ref(s): %d\n", api, g_ref_count);

    up(&trace_dog_sem);
}

