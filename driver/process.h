#ifndef SYS_MONITOR_PROCESS_H
#define SYS_MONITOR_PROCESS_H

//
// macro declare
//

#define HOT_FD_CACHE_SIZE 16 

//
// struct declare
//

//file fd storage struct, each instance represents an fd
typedef struct _file_fd_record
{
	struct _file_fd_record * prev;
	struct _file_fd_record * next;

	struct _file_fd_record * time_prev;
	struct _file_fd_record * time_next;

	unsigned int fd;
	int hot_index;
	char * filename;
} file_fd_record;

//
typedef struct _process_record 
{
	struct _process_record * prev;									//previous outstanding record
	struct _process_record * next;									//next outstanding record
	
	struct timespec start_time;										//process start time

	bool before_execve;												//identicate this process is just before execve states
	char * filename;												//process binary file name
	int argv_length;												//argument total length
	char * argv;													//argument(s)
	int env_length;													//environment total length
	char * env;														//environment var(s)

	//file fd record area
	struct rw_semaphore file_fd_sem;
	int fd_count;
	int hot_count;
	struct _file_fd_record * file_fd_head;							//fd link header, full link
	struct _file_fd_record * hot_fd_head;							//hot fd cycle link, not full link, head element must be the youngest
	struct _file_fd_record * hot_fd_cache[HOT_FD_CACHE_SIZE];		//hot fd cache, inc sorted by fd
} process_record;

//
// function declare
//

//do the initize 
int process_monitor_init(void);

//do the cleanup
void process_monitor_cleanup(void);

//notify monitor that an process has created(sys_fork, sys_vfork, sys_clone)
void notify_create(void);

//notify monitor that an process has exited
void notify_exit(void);

//notify monitor to update process path 
void notify_enter(void);

//notify monitor that sys_exeve is about to execute
void notify_execve(void);

int get_current_process_id(void);

//get process record by pid
process_record * get_record_by_pid(pid_t pid);

//get process binary file name by pid
char * get_process_path_by_pid(pid_t pid);

void lock_process_record(void);

void unlock_process_record(void);

//export
char * get_current_process_path(void);
char * get_current_process_arguments(int * length);
char * get_current_process_environment(int * length);

#endif
