#ifndef SYS_MONITOR_OPEN_H
#define SYS_MONITOR_OPEN_H

//
// open file system call includes:
// sys_open
// sys_creat
//

void open_operation_init(unsigned long ** sys_call_table);

void open_operation_cleanup(unsigned long ** sys_call_table);

char * get_absolute_path_by_fd(unsigned int fd);

#endif
