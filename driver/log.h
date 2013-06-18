#ifndef SYS_MONITOR_LOG_H
#define SYS_MONITOR_LOG_H

#define HASH_SLOT_SIZE 65535
#define STRING_MAX_SIZE 256
#define RECORD_MAX_SIZE	1024

#include <linux/rtc.h>

typedef enum _operation_name
{
	op_null = 0,
	
	/* file system event */
	op_create_file,
	op_read_file,
	op_write_file,
	op_close_file,
	op_io_control_file,
	op_stat_file,
	op_rename_file,
	op_make_directory,
	op_remane_directory,
	op_sync_file,
	op_open_pipe,
	op_symbol_link,
	op_read_symbol_link,

	/* process event */
	op_load_library,
	op_exit_proc,
	op_fork_proc,
	op_execve_proc,
	op_kill_proc

} operation_name;

typedef enum _api_name
{
	api_null = 0,

	/* file system event */
	api_sys_create,
	api_sys_open,
	api_sys_read,
	api_sys_write,
	api_sys_close,
	api_sys_ioctl,
	api_sys_stat,
	api_sys_lstat,
	api_sys_fstat,
	api_sys_rename,
	api_sys_mkdir,
	api_sys_rmdir,
	api_sys_sync,
	api_sys_pipe,
	api_sys_symlink,
	api_sys_readlink,

	/* process event */
	api_sys_uselib,
	api_sys_exit,
	api_sys_fork,
	api_sys_vfork,
	api_sys_clone,
	api_sys_execve,
	api_sys_kill,

	api_max
} api_name;

typedef struct _log_param_connector
{
	int param_count;			//total param count at this system call
	int current_param;			//offset of param that current deal with
	char ** param_strings;		//formatted param string buffer for each param
} log_param_connector;


typedef struct _log_header
{
	int sec;					//call time of system call
	int nsec;
	int pid;					//caller`s process id
	int tid;					//caller`s thread id
	operation_name operation;	//caller`s operation type, enum type
	api_name api;				//which system call does caller invoked, enum type
	long result;				//system call execute result
	int path_offset;			//offset of attached path string var
	int details_offset;			//offset of attached details string vat, currently on param info
	int content_size;			//this log`s full size
} log_header;

typedef struct _log_item
{
	struct _log_header header;
	struct _log_param_connector param;	
	char * path;
} log_item;

void log_event(const char log_level, const char * file, int line, const char * fmt, ...);

bool log_init(void);

int get_proc_id(void);

void log_cleanup(void);

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
