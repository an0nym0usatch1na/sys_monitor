#ifndef SYS_MONITOR_CLOSE_H
#define SYS_MONITOR_CLOSE_H

//
// close file system call includes:
// sys_close
//

void close_operation_init(unsigned long ** sys_call_table);

void close_operation_cleanup(unsigned long ** sys_call_table);

#endif
