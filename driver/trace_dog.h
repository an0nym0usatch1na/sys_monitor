#ifndef _SYS_MONITOR_TRACE_DOG_H
#define _SYS_MONITOR_TRACE_DOG_H

typedef struct _trace_item {
	struct _trace_item * next;
	struct _trace_item * prev;
	int ref_index;
} trace_item;

//initize routine of trace dog
int trace_dog_initize(void);

//cleanup routine of trace dog
void trace_dog_cleanup(void);

//wait for the time that module can unload
void wait_for_unload(void);

//tell trace dog that we have enter an sys_call api
void trace_dog_enter(api_name api);

//tell trace dog that we have leave an sys_call api
void trace_dog_leave(api_name api);

#endif
