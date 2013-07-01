#ifndef SYS_MONITOR_INTERFACE_H
#define SYS_MONITOR_INTERFACE_H

//page size always be 4096(2^PAGE_SHIFT)
//4096 * 2 ^ 7 = 512KB
#define PAGE_INCREASE 7

//
// structs
//

//
// function defines
//

//check if the device is ready
bool is_device_ready(void);

//check if current process is our reader process
bool is_our_process(void);

//init the interface with dev
long interface_init(dev_t dev);

//cleanup previous initized interface
void interface_cleanup(dev_t dev);

//write data into interface
void write_to_interface(char * buffer, int buffer_size);

#endif
