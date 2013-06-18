#ifndef SYS_MONITOR_FORK_H
#define SYS_MONITOR_FORK_H

//
// fork file system call includes:
// sys_fork
//

void fork_operation_init(unsigned long ** sys_call_table);

void fork_operation_cleanup(unsigned long ** sys_call_table);

#endif
