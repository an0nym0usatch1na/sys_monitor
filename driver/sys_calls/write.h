#ifndef SYS_MONITOR_WRITE_H
#define SYS_MONITOR_WRITE_H

//
// write file system call includes:
// sys_write
//

void write_operation_init(unsigned long ** sys_call_table);

void write_operation_cleanup(unsigned long ** sys_call_table);

#endif
