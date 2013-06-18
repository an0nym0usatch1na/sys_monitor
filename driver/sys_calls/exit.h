#ifndef SYS_MONITOR_EXIT_H
#define SYS_MONITOR_EXIT_H

//
// exit file system call includes:
// sys_exit
//

void exit_operation_init(unsigned long ** sys_call_table);

void exit_operation_cleanup(unsigned long ** sys_call_table);

#endif
