#ifndef SYS_MONITOR_EXEC_H
#define SYS_MONITOR_EXEC_H

//
// exec file system call includes:
// sys_exec
//

void exec_operation_init(unsigned long ** sys_call_table);

void exec_operation_cleanup(unsigned long ** sys_call_table);

#endif
