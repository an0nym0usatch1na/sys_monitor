#include <linux/sched.h>
#include <linux/unistd.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/timer.h>
#include <linux/timex.h>
#include <linux/rtc.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <asm/current.h>

#include "sys_monitor.h"
#include "log.h"
#include "interface.h"

#define SYSMON_DEBUG
#include "debug.h"

//global vars
log_item ** thread_log_cache = NULL;

//do initize when start
bool log_init(void)
{
#ifdef _DEBUG
	int i = 0;
#endif

	int size = sizeof(void *) * HASH_SLOT_SIZE;

	thread_log_cache = (log_item **)kmalloc(size, GFP_KERNEL);
	if (NULL == thread_log_cache)
	{
		//alloc failed
		PERROR("allocate memory failed\n");

		return false;
	}
	
	memset(thread_log_cache, '\0', size);

#ifdef _DEBUG
	for (i = 0; i < size ; i++)
	{
		if (0 != *((char *)thread_log_cache + i))
		{
			//memset wrong?
			PERROR("memset  error????\n");
		}
	}
	
#endif


	PDEBUG("log module initize success, total %d byte(s) allocated\n", size);
	
	//success
	return true;
}

//do clean up before exit driver
void log_cleanup(void)
{
	int i = 0;
	
	if (NULL != thread_log_cache)
	{
		for (i = 0; i < HASH_SLOT_SIZE; i++)
		{
			log_item * item = thread_log_cache[i];
			
			if (NULL != item)
			{
				if (NULL != item->path)
				{
					kfree(item->path);
				}
		
				if (NULL != item->param.param_strings)
				{
					int i = 0;
					for (; i < item->param.param_count; i++)
					{
						if (NULL != item->param.param_strings[i])
						{
							kfree(item->param.param_strings[i]);
						}
					}
			
					kfree(item->param.param_strings);
				}
				
				kfree(item);
			}
		}
		 
		kfree(thread_log_cache);
	}
	
	PDEBUG("complete cleanup log caches\n");
}

void log_item_to_storage(log_header * header, char * path, char * details)
{
	int path_size = 1; 	//1 for last terminate char
	int details_size = 1;
	int full_size = 0;
	char * buff = NULL;

	//if device is not ready, just exit
	if (!is_device_ready())
	{
		return;
	}

	//calculate full size
   	if (NULL != path) 
	{
		path_size = strlen(path) + 1;
	}
	if (NULL != details)
	{
		details_size = strlen(details) + 1;
	}

	full_size = sizeof(log_header) + path_size + details_size;	
	
	PVERBOSE("prepare log to interface, full size: %d, path size: %d, details size: %d\n", full_size, path_size, details_size);
	
	buff = (char *)kmalloc(full_size, GFP_KERNEL);
	if (NULL != buff)
	{
		//copy header
		memcpy(buff, header, sizeof(log_header));
		header = (log_header *)buff;

		//format offset
		header->path_offset = sizeof(log_header);
		header->details_offset = header->path_offset + path_size;
		header->content_size = path_size + details_size;

		*((char *)header + header->path_offset) = '\0';
		*((char *)header + header->details_offset) = '\0';

		//fill buffer
		if (NULL != path)
		{
			strcpy((char *)header + header->path_offset, path);
		}
		if (NULL != details)
		{
			strcpy((char *)header + header->details_offset, details);
		}

		PVERBOSE("\theader->sec: 0x%08x, at 0x%08x\n", header->sec, &header->sec);
		PVERBOSE("\theader->nsec: 0x%08x, at 0x%08x\n", header->nsec, &header->nsec);
		PVERBOSE("\theader->pid: %d, at 0x%08x\n", header->pid, &header->pid);
		PVERBOSE("\theader->tid: %d, at 0x%08x\n", header->tid, &header->tid);
		PVERBOSE("\theader->operation: %d, at 0x%08x\n", header->operation, &header->operation);
		PVERBOSE("\theader->api: %d, at 0x%08x\n", header->api, &header->api);
		PVERBOSE("\theader->result: 0x%08x, at 0x%08x\n", header->result, &header->result);
		PVERBOSE("\theader->path_offset: 0x%08x, at 0x%08x\n", header->path_offset, &header->path_offset);
		PVERBOSE("\theader->details_offset: 0x%08x, at 0x%08x\n", header->details_offset, &header->details_offset);
		PVERBOSE("\theader->conten_size: %d, at 0x%08x\n", header->content_size, &header->content_size);
		PVERBOSE("\theader->path: %s, at 0x%08x\n", (char *)header + header->path_offset, (char *)header + header->path_offset);
		PVERBOSE("\theader->details: %s, at 0x%08x\n", (char *)header + header->details_offset, (char *)header + header->details_offset);

		if (api_sys_execve == header->api)
		{
			PDEBUG("now log sys_execve event to cache\n");
		}

		write_to_interface(buff, full_size);
  				
  		kfree(buff);
	}
}

int get_proc_id(void)
{
	return current->pid;
}

int get_thread_id(void)
{
	//it seems there is no thread inside linux kernel?
	return 0;
}

int get_slot_id(void)
{
	//use pid as slot id, it works under none muti-thread os
	int id = current->pid;
	if (current->pid < 0 || current->pid >= HASH_SLOT_SIZE)
	{
		PERROR("received an error pid %d\n", id);
		
		id = 0;
	}

	return id;
}

//get current log_item structure from current thread
log_item * get_current_cache(bool get_new)
{
	log_item * item = thread_log_cache[get_slot_id()];
	
	if (NULL == item)
	{
		//cache has not allocated
		item = kmalloc(sizeof(log_item), GFP_KERNEL);
		if (NULL != item)
		{
			thread_log_cache[get_slot_id()] = item;

			item->path = NULL;
			item->param.param_strings = NULL;

			PVERBOSE("got an new allcated cache at 0x%08x\n", item);
		}
	} 
	else if (get_new)
	{
		//cache allocated, but we need clean up some area
		PVERBOSE("reused an previous allcated cache at 0x%08x:0x%08x:0x%08x\n", item, item->path, item->param.param_strings);

		if (NULL != item->path)
		{
			kfree(item->path);

			item->path = NULL;
		}
		
		if (NULL != item->param.param_strings)
		{
			int i = 0;
			for (; i < item->param.param_count; i++)
			{
				if (NULL != item->param.param_strings[i])
				{
					kfree(item->param.param_strings[i]);
				}
			}
			
			kfree(item->param.param_strings);

			item->param.param_strings = NULL;
		}
	}
	
	//finally, just return it
	return item;
}

char * copy_string_from_param(const __user char * path)
{
	char * holder = kmalloc(STRING_MAX_SIZE, GFP_KERNEL);
	
	if (NULL != holder)
	{
		if (strncpy_from_user(holder, path, STRING_MAX_SIZE) < 0)
		{
			goto cleanup;
		}
	}
	
	return holder;
	
cleanup:
	PERROR("error while invoke strncpy_from_user\n");

	kfree(holder);
	
	return NULL;
}

bool begin_log_system_call(operation_name oper, api_name api, const __user char * path, int param_count)
{
	char * path_safe = NULL;
	log_item * item = NULL;

	if (is_our_process()) 
	{
		//operation from our ring3 reader process
		return false;
	}

	if (NULL != path) 
	{
		path_safe = copy_string_from_param(path);
	}

	item = get_current_cache(true);
	if (NULL != item)
	{
		//ok, found an place holder
		struct timespec now;

		now = current_kernel_time();

		item->header.sec = now.tv_sec;
		item->header.nsec = now.tv_nsec;
		item->header.pid = get_proc_id();
		item->header.tid = get_thread_id();
		item->header.operation = oper;
		item->header.api = api;
		item->path = path_safe;
		item->param.param_count = param_count;
		item->param.param_strings = NULL;
		item->param.current_param = 0;
		
		if (item->param.param_count > 0)
		{
			item->param.param_strings = (char **)kmalloc(item->param.param_count * sizeof(char *), GFP_KERNEL);
			if (NULL != item->param.param_strings)
			{
				//init
				memset(item->param.param_strings, '\0', item->param.param_count * sizeof(char *));
			}
			else
			{
				//mem allocate failed
				PERROR("memory allocate failed at begin_log_system_call\n");
				
				return 0;
			}
		}
		
		PVERBOSE("begin_log_system_call succ %d:%d, path: %s, param count: %d\n", oper, api, item->path, param_count);
		
		return true;
	}
	else
	{
		PERROR("get item failed at begin_log_system_call with slot id %d\n", get_slot_id());
		
		return false;
	}
}

//it will add an void param string to log item
void add_void_param(void)
{
	log_item * item = get_current_cache(false);
	int len = 11;	// 11 for string "viod: void"
	char * holder = NULL;
	
	if (NULL != item)
	{
		holder = (char *)kmalloc(len, GFP_KERNEL);
		if (NULL != holder)
		{
			strcpy(holder, "void: void");
		
			PVERBOSE("add an void as param %d at 0x%08x\n", item->param.current_param, holder);
		
			//ok, store it
			item->param.param_strings[item->param.current_param++] = holder;
		}
	}
	else
	{	
		//no item, it must be an error
		PERROR("get item failed at add_void_param with slot id %d\n", get_slot_id());
	}
}

//it will add an int param string to log item
void add_int_param(char * name, int int_param)
{
	log_item * item = get_current_cache(false);
	int len = strlen(name) + 22;	// %s: %d
	char * holder = NULL;
	
	if (NULL != item)
	{
		holder = (char *)kmalloc(len, GFP_KERNEL);
		if (NULL != holder)
		{
			sprintf(holder, "%s: %d", name, int_param);
		
			PVERBOSE("add an int %s as param %d at 0x%08x\n", holder, item->param.current_param, holder);
	
			//ok, store it
			item->param.param_strings[item->param.current_param++] = holder;
		}
	}
	else
	{	
		//no item, it must be an error
		PERROR("get item failed at add_int_param with slot id %d\n", get_slot_id());
	}
}

void add_pointer_param(char * name, unsigned char * pointer_param)
{
	add_unsigned_int_param(name, (unsigned int)pointer_param);
}

void add_unsigned_int_param(char * name, unsigned int uint_param)
{
	log_item * item = get_current_cache(false);
	int len = strlen(name) + 13;	// %s: 0x%08x
	char * holder = NULL;
	
	if (NULL != item)
	{
		holder = (char *)kmalloc(len, GFP_KERNEL);
		if (NULL != holder)
		{
			sprintf(holder, "%s: 0x%08x", name, uint_param);
		
			PVERBOSE("add an unsigned int %s as param %d at 0x%08x\n", holder, item->param.current_param, holder);
	
			//ok, store it
			item->param.param_strings[item->param.current_param++] = holder;
		}
	}
	else
	{	
		//no item, it must be an error
		PERROR("get item failed at add_unsigned_int_param with slot id %d\n", get_slot_id());
	}
}

//it will add and string param string to log item
void add_string_param(char *name, const __user char * string_param)
{
	bool need_free = true;
	log_item * item = get_current_cache(false);
	int len = 0; 
	char * holder = NULL; 
	
	if (NULL != item)
	{
		//first, get string from user space
		char * str = copy_string_from_param(string_param);
		if (NULL == str)
		{
			//copy failed, so we assume it as null
			str = "<NULL>";
			need_free = false;
		}
			
		//all ok, now construct it
		len = strlen(name) + 3 + strlen(str);	// 1 for space
		holder = (char *)kmalloc(len, GFP_KERNEL);
		if (NULL != holder)
		{
			sprintf(holder, "%s: %s", name, str);
	
			PVERBOSE("add an string %s as param %d at 0x%08x\n", holder, item->param.current_param, holder);

			//ok, store it
			item->param.param_strings[item->param.current_param++] = holder;
		}
		
		if (need_free)
		{	
			kfree(str);
		}
	}
	else
	{
		//no item, it must be an error
		PERROR("get item failed at add_string_param with slot id %d\n", get_slot_id());
	}	
}

//end of log, collect and construct all information and actually log it
void end_log_system_call(long ret)
{
	log_item * item = get_current_cache(false);
	
	if (NULL != item)
	{
		int i = 0;
		int path_size = 0;
		int detail_size = 0;
		char * details = NULL;
	
		if (NULL != item->path)
		{
			path_size = strlen(item->path);
		}
		else
		{
			path_size = 1;
		}

		detail_size = RECORD_MAX_SIZE - sizeof(log_item) - path_size - 1;	//1 for path terminate char '\0'

		PVERBOSE("begin end_log_system_call with path size %d, details size %d\n", path_size, detail_size);

		item->header.result = ret;

		if (detail_size <= 0)
		{
			PDEBUG("no enough space to store details, so pass it to interface directly\n");
			
			details = NULL;
		}
		else
		{
			details = (char *)kmalloc(detail_size, GFP_KERNEL);
		}

		if (NULL != details)
		{
			details[0] = '\0';	
			
			if (item->param.param_count > 0 && NULL != item->param.param_strings)
			{
				int left = detail_size;

				for (i = 0; i < item->param.param_count; i++)
				{
					//first cal remain buffer size
					if (NULL != item->param.param_strings[i])
					{
						left -= strlen(item->param.param_strings[i]);
					}
					else
					{	
						left -= 6;
					}

					if (left > 0)	//last space for '/0'
					{
						if (NULL != item->param.param_strings[i])
						{
							strcat(details, item->param.param_strings[i]);
						}
						else
						{
							strcat(details, "<NULL>");
						}
					}
					else
					{
						break;
					}
				
					if (item->param.param_count - 1 != i && left > 2)
					{
						//not last one
						strcat(details, ", ");

						left -= 2;
					}
				}
			}
			else
			{
				//well, no param, okay...
			}
		}

		//now, actually log it
		log_item_to_storage(&item->header, item->path, details);

		PVERBOSE("finish log %d:%d(details: %s) at 0x%08x\n", item->header.operation, item->header.api, details, item);

		//do some cleanup
		if (NULL != details)
		{
			kfree(details);
		}
	}
	else
	{
		//no item, it must be an error
		PERROR("get item failed at end_log_system_call with slot id %d\n", get_slot_id());
	}
}

const char * operation_to_string(operation_name operation) {
	switch (operation) {
		case op_null:
			return "<Null operation>";

		case op_create_file:
			return "Create file";

		case op_read_file:
			return "Read file";

		case op_write_file:
			return "Write file";

		case op_close_file:
			return "Close file";

		case op_io_control_file:
			return "IOCTL file";

		case op_load_library:
			return "Load library";

		case op_exit_proc:
			return "Exit process";

		case op_fork_proc:
			return "Fork process";

		case op_execve_proc:
			return "Exec process";

		case op_kill_proc:
			return "Kill process";

		default:
			return "<Unknown>";
	}
}

const char * api_to_string(api_name api) {
	switch (api) {
		case api_null:
			return "<Null api>";

		case api_sys_create:
			return "sys_creat";

		case api_sys_open:
			return "sys_open";

		case api_sys_read:
			return "sys_read";

		case api_sys_write:
			return "sys_write";

		case api_sys_close:
			return "sys_close";

		case api_sys_ioctl:
			return "sys_ioctl";

		case api_sys_uselib:
			return "sys_uselib";

		case api_sys_exit:
			return "sys_exit";

		case api_sys_fork:
			return "sys_fork";

		case api_sys_execve:
			return "sys_execve";

		default:
			return "<Unknown>";
	}
}
