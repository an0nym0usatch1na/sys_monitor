#ifndef SYS_MONITOR_IOCTL_H
#define SYS_MONITOR_IOCTL_H

//
// ioctl file system call includes:
// sys_ioctl
//

void ioctl_operation_init(unsigned long ** sys_call_table);

void ioctl_operation_cleanup(unsigned long ** sys_call_table);

#endif
