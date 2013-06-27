#ifndef SYS_MONITOR_LOG_H
#define SYS_MONITOR_LOG_H

#define HASH_SLOT_SIZE 65535
#define STRING_MAX_SIZE 256
#define RECORD_MAX_SIZE	1024

#include "./../share/log_def.h"

void log_event(const char log_level, const char * file, int line, const char * fmt, ...);

bool log_init(void);

int get_proc_id(void);

void log_cleanup(void);

bool begin_log_system_call2(operation_name oper, api_name api, unsigned int fd, int param_count);

bool begin_log_system_call(operation_name oper, api_name api, const __user char * path, int param_count);

void add_void_param(void);

void add_int_param(char * name, int int_param);

void add_unsigned_int_param(char * name, unsigned int uint_param);

void add_pointer_param(char * name, unsigned char * pointer_param);

void add_string_param(char * name, const __user char * string_param);

void end_log_system_call(long ret);

const char * operation_to_string(operation_name operation);

const char * api_to_string(api_name api);

#endif
