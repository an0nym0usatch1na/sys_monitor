#include <linux/init.h>
#include <linux/module.h>
#include <linux/unistd.h>
#include <linux/semaphore.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/cdev.h>
#include <asm/page.h>
#include <asm/current.h>
#include <asm/uaccess.h>

#include "sys_monitor.h"
#include "log.h"
#include "interface.h"

#define SYSMON_DEBUG
#include "./../share/debug.h"

//
// global var definition
//

struct cdev cdev;
struct semaphore sem;
struct class * monitor_class = NULL;

bool device_ready = false;
long cycle_buffer_size = 0;
char * read_ptr = NULL;
char * write_ptr = NULL;
char * cycle_buffer = NULL;

int owner_pid = 0;

//return non zero if the device is ready
bool is_device_ready(void)
{
	return device_ready;
}

bool is_our_process(void) 
{
	return owner_pid == current->pid;
}

//return how many byte(s) used in cache
long get_used_size(void)
{
	if (read_ptr == write_ptr)
	{
		return 0;
	}
	else if (read_ptr > write_ptr)
	{
		return cycle_buffer_size - (read_ptr - write_ptr);
	}
	else
	{
		return write_ptr - read_ptr;
	}
}

//copy selected interface buffer to another buffer, deal with ring buffer cut down
char * copy_to_buffer(char * start_pos, char * buffer, int size)
{
	//check if two times(over boundary) is needed
	if (start_pos + size >= cycle_buffer + cycle_buffer_size)
	{
		//specially deal needed
		int first_size = cycle_buffer_size + cycle_buffer - start_pos;
		
		memcpy(start_pos, buffer, first_size);
		
		//fixing up
		start_pos = cycle_buffer;
		buffer += first_size;
		size -= first_size;

		PDEBUG("write cross boundary, all write size: %d, first write size: %d, left: %d\n", size + first_size, first_size, size);
	}

	if (size > 0)
	{	
		memcpy(start_pos, buffer, size);
		start_pos += size;
	}

	return start_pos;
}

//write data into interface
void write_to_interface(char * buffer, int buffer_size)
{
	if (!is_device_ready())
	{
		return;
	}
	
	if (0 == buffer_size)
	{
		return;
	}

	PVERBOSE("write %d byte(s) buffer to cache now\n", buffer_size);
	
	if (down_interruptible(&sem))
	{
		return;
	}

	//we will never let our buffer be full
	//note when cycle_buffer_size - get_used_size() = buffer_size, all buffer will be filled, then read_ptr = write_ptr
	//but we have no idea buffer size is 0 or full when read_ptr = write_ptr, so avoid this states
	if ((cycle_buffer_size - get_used_size()) > buffer_size)
	{
		//buffer is enough
		write_ptr = copy_to_buffer(write_ptr, buffer, buffer_size);
		
		PVERBOSE("write complete, read_ptr: 0x%08x, write_ptr: 0x%08x, used size: %d\n", read_ptr, write_ptr, get_used_size());
	}
	else
	{
		PWARN("failed while write to interface, buffer is full, used size: %d, need: %d\n", get_used_size(), buffer_size);
	}
	
	up(&sem);
}

//create/open sys_call will invoke this, as we only support one process
//to read at the same time, so once device is opened, other process
//can not open it anymore
int interface_open(struct inode *inode, struct file *filp)
{
	int result = 0;
    if (down_interruptible(&sem))
  	{
        result = -EINTR;
		goto exit;
    }

	if (0 != owner_pid) 
	{
		//if someone has already taken this place, just fail others
		PDEBUG("interface_open invoked, but process %d has already taken place, so fail this\n", owner_pid);

		result = -EACCES;
		goto cleanup;
	}

	if (FMODE_WRITE & filp->f_mode)
	{
		//do not support write
		PDEBUG("interface_open invoked file mode 0x%08x with write access, fail it\n", filp->f_mode);
		
		result = -EACCES;
		goto cleanup;
	}

	owner_pid = current->pid;
	device_ready = true;
	
	PDEBUG("interface_open invoked, file mode: 0x%08x, update owner_pid to %d and wake up logger\n", filp->f_mode, owner_pid);

cleanup:	
	up(&sem);	

exit:
	return 0;
}

//read sys_call will invoke this, do read something
ssize_t interface_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	int used = 0;
	int min = 0;
	int left = 0;
	int copyed = 0;
	int first_size = 0;
	
	if (down_interruptible(&sem))
	{
		copyed = -EINTR;
		
		goto cleanup;
	}
	
	PVERBOSE("read begin, read_ptr: 0x%08x, write_ptr: 0x%08x, used size: %d, want to read %d byte(s)\n", read_ptr, write_ptr, get_used_size(), count);
	
	used = get_used_size();
	if (count > used)
	{
		min = used;
	}
	else
	{
		min = count;
	}	

	first_size = cycle_buffer_size - (read_ptr - cycle_buffer);
	
	if (read_ptr == write_ptr)
	{
		copyed = 0;
		
		goto cleanup;
	}
	else if (read_ptr > write_ptr && first_size <= min)
	{
		//deal with read cross boundary
		int first_copy = 0;
		int left = 0;

		ASSERT(first_size > 0);

		PDEBUG("read cross boundary, left %d byte(s), need %d byte(s)\n", first_size, min);

		//read buffer is bigger than this part, so read all left out 
		first_copy = first_size;
			
		left = copy_to_user(buf, read_ptr, first_copy);
		if (0 == left)
		{
			copyed = first_copy;
		}
		else
		{
			//copy not completed, go cleanup
			copyed = first_copy - left;

			PWARN("copy_to_user not completed (%d, %d) inside interface_read\n", left, first_copy);
			
			goto cleanup;
		}
			
		read_ptr = cycle_buffer;
		buf += copyed;
		min -= copyed;
	}

	if (min > 0) 
	{
		//read second part or read normally
		PVERBOSE("read secondly or normally %d byte(s) at 0x%08x\n", min, read_ptr);
		
		left = copy_to_user(buf, read_ptr, min);
		
		if (0 == left)
		{
			copyed += min;
			read_ptr += min;
		}
		else
		{
			copyed += min - left;
			read_ptr += min - left;

			PWARN("copy_to_user not completed (%d, %d) inside interface_read\n", left, min);
		}
	}
	
cleanup:
	up(&sem);
	
	PVERBOSE("read complete, read_ptr: 0x%08x, write_ptr: 0x%08x, used size: %d, copyed: %d\n", read_ptr, write_ptr, get_used_size(), copyed);
	
	return copyed;	
}

//write sys_call will invoke this, our device is read only device, so fail all write request
ssize_t interface_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	//write is forbidden
	PDEBUG("interface_write invoked, but we do not support write operation, so fail it\n");

	return -EROFS;
}

//close device will invoke this, clean up owner process and prepare for next open
int interface_release(struct inode *inode, struct file *filp)
{
    if (down_interruptible(&sem))
    {
	    return -EINTR;
    }

	PDEBUG("interface_release invoke, now going to release owner_pid");

	if (current->pid == owner_pid)
	{
		PDEBUG("owner pid %d has already released, set device to sleep\n", owner_pid);

		owner_pid = 0;
		device_ready = false;
	}

	up(&sem);

	return 0;
}

//we just cover read/write/open/close right now
struct file_operations interface_fops = {
	.owner =    THIS_MODULE,
	.read =     interface_read,
	.write =    interface_write,
	.open =     interface_open,
	.release =  interface_release,
};


int begin_interface(void)
{
	int i = 0;
	int size = 1;
	int result = 0;
	
	PDEBUG("begin_interface invoked, do the initize\n");
	
	if (down_interruptible(&sem))
	{
		result = -EINTR;
		
		goto cleanup_exit;
	}
	
	if (is_device_ready())
	{
		result = -ETXTBSY;
		
		goto cleanup;
	}
	
	//get page count
	for (i = 0; i < PAGE_INCREASE; i++)
	{
		size = size * 2;
	}
	
	PDEBUG("now trying to allocate %d pages, total %d bytes\n", size, size << PAGE_SHIFT);
	
	//get real size
	size = size << PAGE_SHIFT;
	cycle_buffer_size = size; 	

	//allocate
	cycle_buffer = (char *)__get_free_pages(GFP_KERNEL, PAGE_INCREASE);
	if (NULL == cycle_buffer)
	{
		result = -ENOMEM;
		
		goto cleanup;
	}
	
	PDEBUG("allocatd %d bytes at 0x%08x\n", size, cycle_buffer);
	
	read_ptr = cycle_buffer;
	write_ptr = cycle_buffer;
	
cleanup:
	up(&sem);
	
cleanup_exit:
	return result;
}

int end_interface(void)
{
	int i = 0;
	int size = 1;
	
	if (down_interruptible(&sem))
	{
		return -EINTR;
	}
	
	PDEBUG("end_interface invoked, do the cleanup, buffer: 0x%08x\n", cycle_buffer);

	if (NULL == cycle_buffer)
	{
		goto cleanup;
	}
	
	for (i = 0; i < PAGE_INCREASE; i++)
	{
		size = size * 2;
	}
	
	PDEBUG("now free %d pages\n", size);
	free_pages((unsigned long)cycle_buffer, PAGE_INCREASE);
	
	read_ptr = NULL;
	write_ptr = NULL;
	cycle_buffer = NULL;
	
	PDEBUG("cleanup completed\n");
	
cleanup:
	up(&sem);
	
	return 0;
}

long interface_init(dev_t dev)
{
	int result = 0;
	struct device * tmp = NULL;
	
	sema_init(&sem, 1);

	monitor_class = class_create(THIS_MODULE, "sys_monitor");
    if (NULL == monitor_class) 
	{
        PERROR("failed to create class\n");
		result = -EINVAL;
		goto cleanup;
    }

	tmp = device_create(monitor_class, NULL, dev, "%s", "sys_monitor");
	if (NULL == tmp)
	{
		PERROR("failed to create device /dev/sys_monitor\n");
		result = -EINVAL;
		goto cleanup_class_create;
	}

	cdev_init(&cdev, &interface_fops);
	cdev.owner = THIS_MODULE;
	cdev.ops = &interface_fops;
	
	result = cdev_add(&cdev, dev, 1);
	if (result)
	{
		//cdev_add failed
		PERROR("failed while adding device, error %d\n", result);
		result = -EINVAL;
		goto cleanup_device_create;
	}
	
	if (0 != begin_interface())
	{
		//failed
		PERROR("failed while begin interface\n");
		result = -EINVAL;
		goto cleanup_dev_add;
	}	
	
	PDEBUG("device added, interface init success\n");
	
	return 0;

cleanup_dev_add:
	cdev_del(&cdev);

cleanup_device_create:
	device_destroy(monitor_class, dev);

cleanup_class_create:
	class_destroy(monitor_class);

cleanup:
	return result;
}

void interface_cleanup(dev_t dev)
{
	end_interface();

	cdev_del(&cdev);

	device_destroy(monitor_class, dev);

	class_destroy(monitor_class);

	PDEBUG("interface cleanup completed\n");
}
