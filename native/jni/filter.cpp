#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include "deelx.h"

#define SYSMON_DEBUG

extern "C" {
	#include "./../../share/log_def.h"
	#include "./../../share/debug.h"
}

#include "sys_reader.h"
#include "interface.h"
#include "filter.h"

//global vars
filter_item * filter_header = NULL;
filter_item * filter_tail = NULL;

action_result FilterValueWithFilter(char * value, filter_item * filter) {
	bool matched = false;
	action_result result = result_none;
	int len = 0;

	switch (filter->operate) {
		case op_is:
			//directly equal
			if (0 == strcasecmp(value, filter->value)) {
				matched = true;
			}
			break;

		case op_is_not:
			//directly not equal
			if (0 != strcasecmp(value, filter->value)) {
				matched = true;
			}
			break;

		case op_excludes:
			//not contains any in string
			if (NULL == strstr(value, filter->value)) {
				matched = true;
			}
			break;

		case op_contains:
			//contains any in string
			if (NULL != strstr(value, filter->value)) {
				matched = true;
			}
			break;

		case op_begins_with:
			//begin with
			if (0 == strncasecmp(value, filter->value, filter->value_length)) {
				matched = true;
			}
			break;

		case op_ends_with:
			//end with
			len = strlen(value);
			if (len >= filter->value_length && 0 == strncasecmp((char *)&value[len - filter->value_length], filter->value, filter->value_length)) {
				matched = true;
			}
			break;
	}

	if (action_include == filter->action) {
		//action_include
		if (matched) {
			result = result_include;
		} else {
			result = result_none;
		}
	} else {
		//action_exclude
		if (matched) {
			result = result_exclude;
		} else {
			result = result_none;
		}
	}

	PVERBOSE("value \"%s\" filtered by filter #%08x(%d, %d, \"%s\", %d) result: %d\n", value, filter, filter->column, filter->operate, filter->value, filter->action, result);

	return result;
}

column_item GetColumn(char * col) {
	if (0 == strcasecmp(col, "time")) {
		return column_time;
	} else if (0 == strcasecmp(col, "pid")) {
		return column_pid;
	} else if (0 == strcasecmp(col, "process")) {
		return column_process;
	} else if (0 == strcasecmp(col, "operation")) {
		return column_operation;
	} else if (0 == strcasecmp(col, "api")) {
		return column_api;
	} else if (0 == strcasecmp(col, "path")) {
		return column_path;
	} else if (0 == strcasecmp(col, "result")) {
		return column_result;
	} else if (0 == strcasecmp(col, "details")) {
		return column_details;
	}

	return column_none;
}

operate_type GetOperate(char * op) {
	if (0 == strcasecmp(op, "is")) {
		return op_is;
	} else if (0 == strcasecmp(op, "is not")) {
		return op_is_not;
	} else if (0 == strcasecmp(op, "contain") || 0 == strcasecmp(op, "contains")) {
		return op_contains;
	} else if (0 == strcasecmp(op, "exclude") || 0 == strcasecmp(op, "excludes")) {
		return op_excludes;
	} else if (0 == strcasecmp(op, "begin with") || 0 == strcasecmp(op, "begins with") || 0 == strcasecmp(op, "start with") || 0 == strcasecmp(op, "starts with")) {
	   	return op_begins_with;
	} else if (0 == strcasecmp(op, "end with") || 0 == strcasecmp(op, "ends with")) {
		return op_ends_with;
	}

	return op_none;	
}

action_item GetAction(char * action) {
	if (0 == strcasecmp(action, "include")) {
		return action_include;
	} else if (0 == strcasecmp(action, "exclude")) {
		return action_exclude;
	}

	return action_none;
}

void ListFilter(filter_item * item) {
	printf("filter #%08x(%d, %d, %s, %d)\n", item, item->column, item->operate, item->value, item->action);
}

void ListAllFilter() {
	filter_item * p = filter_header;
		
	while (NULL != p) {
		ListFilter(p);

		p = p->next;
	}
}

bool AddFilter(char * filter) {
	CRegexpT<char> regexp("^(time|pid|process|operation|api|path|result|details)\\s(begins?\\swith|starts?\\swith|ends?\\swith|contains?|excludes?|is\\snot|is)\\s(.+)\\sthen\\s(include|exclude)$");
	bool ret = false;
	char * column = NULL;
	char * op = NULL;
	char * value = NULL;
	char * action = NULL;
	filter_item * item = NULL;

	MatchResult result = regexp.Match(filter);
	if (result.IsMatched() && 4 == result.MaxGroupNumber()) {
		char * tmp = (char *)malloc(strlen(filter) + 1);
		if (NULL != tmp) {
			strcpy(tmp, filter);
			
			column = &tmp[result.GetGroupStart(1)];
			tmp[result.GetGroupEnd(1)] = '\0';

			op = &tmp[result.GetGroupStart(2)];
			tmp[result.GetGroupEnd(2)] = '\0';

			value = &tmp[result.GetGroupStart(3)];
			tmp[result.GetGroupEnd(3)] = '\0';

			action = &tmp[result.GetGroupStart(4)];
			tmp[result.GetGroupEnd(4)] = '\0';

			PDEBUG("filter \"%s %s %s then %s\" parse success\n", column, op, value, action);

			item = (filter_item *)malloc(sizeof(filter_item));
			if (NULL != item) {
				item->next = NULL;
				item->column = GetColumn(column);
				item->operate = GetOperate(op);
				item->action = GetAction(action);
				item->value_length = strlen(value);

				assert(item->value_length > 0);

				item->value = (char *)malloc(item->value_length + 1);
				if (NULL != item->value) {
					strcpy(item->value, value);

					if (NULL == filter_header) {
						//first one inside link
						filter_header = item;
						filter_tail = item;
					} else {
						//normal one
						filter_tail->next = item;
						filter_tail = item;	
					}	
				
					ret = true;

					PVERBOSE("filter #%08x(%d, %d, %s, %d) added to link\n", item, item->column, item->operate, item->value, item->action);

					ListAllFilter();
				} else {
					PWARN("allocate memory failed at AddFilter");

					free(item);
				}
			} else {
				PWARN("allocate memory failed at AddFilter\n");
			}

			free(tmp);
		} else {
			PWARN("allcate memory failed at AddFilter\n");
		}
	} else {
		PWARN("regular expression %s match failed\n", filter);
	}

	return ret;
}

action_result FilterOneLog(filter_item * filter, log_header * header, char * path, char * details) {
	action_result result = result_none;
	char buffer[256];

	switch (filter->column) {
		case column_time:
			//filter time
			GetTime(header->sec, header->nsec, buffer, 256);
			result = FilterValueWithFilter(buffer, filter);
			break;
		
		case column_pid:
			//filter pid
			snprintf(buffer, sizeof(buffer), "%d", header->pid);
			result = FilterValueWithFilter(buffer, filter); 
			break;
		
		case column_process:
			//filter process name
			GetProcess(header->pid, buffer, 256);
			result = FilterValueWithFilter(buffer, filter);
			break;

		case column_operation:
			//filter operation name
			result = FilterValueWithFilter((char *)OperationToString(header->operation), filter);
			break;

		case column_api:
			//filter api name
			result = FilterValueWithFilter((char *)ApiToString(header->api), filter);
			break;

		case column_result:
			//filter result
			snprintf(buffer, sizeof(buffer), "0x%08x", header->result);
			result = FilterValueWithFilter(buffer, filter);
			break;

		case column_path:
			//filter log path
			result = FilterValueWithFilter(path, filter);
			break;
		
		case column_details:
			result = FilterValueWithFilter(details, filter);
			break;
	}

	return result;
}

action_result FilterLog(log_header * header, char * path, char * details) {
	filter_item * p = NULL;
	action_result result = result_include;	//default include

	for (p = filter_header; NULL != p; p = p->next) {
		//find in the sequence of input
		result = FilterOneLog(p, header, path, details);

		if (action_include == p->action) {
			if (result_include == result) {
				//worked as include, and include success, so abort it
				break;
			}
		}

		if (action_exclude == p->action) {
			if (result_exclude == result)
			{
				//worked as exclude and success, return as soon as possible
				break;
			} else {
				//exclude but failed, so we still need it
				result = result_include;
			}
		}
			
	}

	return result;
}
