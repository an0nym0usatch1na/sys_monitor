#ifndef _SYS_READER_FILTER_H
#define _SYS_READER_FILTER_H

//define of item filter
typedef enum _column_item {
	column_none = 0,
	column_time = 1,
	column_pid = 2,
	column_process = 3,
	column_operation = 4,
	column_api = 5,
	column_path = 6,
	column_result = 7,
	column_details = 8
} column_item;

//define of operation filter
typedef enum _operate_type {
	op_none = 0,
	op_is = 1,
	op_is_not = 2,
	op_contains = 3,
	op_begins_with = 4,
	op_ends_with = 5,
	op_excludes = 6 
} operate_type;

//define of action filter
typedef enum _action_item {
	action_none = 0,
	action_include = 1,
	action_exclude = 2
} action_item;

//define of filter result
typedef enum _action_result {
	result_none = 0,
	result_include = 1,
	result_exclude = 2
} action_result;

//time begins with 2012 then include
//process is com.tencent.token then include
typedef struct _filter_item {
	struct _filter_item * next;		//next record in this link
	column_item column;				//column type
	operate_type operate;			//operate type
	char * value;					//operate value
	int value_length;				//operate value`s length
	action_item action;				//action type
} filter_item;

//export function
bool AddFilter(char * filter); 

action_result FilterLog(log_header * header, char * path, char * details); 

#endif
