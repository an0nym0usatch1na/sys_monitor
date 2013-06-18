#ifndef SYS_MONITOR_READ_H
#define SYS_MONITOR_READ_H

//
// read file system call includes:
// sys_read
//

void read_operation_init(unsigned long ** sys_call_table);

void read_operation_cleanup(unsigned long ** sys_call_table);

#endif
