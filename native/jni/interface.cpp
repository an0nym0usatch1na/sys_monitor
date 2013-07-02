#include <stdio.h>  
#include <stdlib.h>  
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#define SYSMON_DEBUG
extern "C" {
	#include "./../../share/log_def.h"
	#include "./../../share/debug.h"
}

#include "sys_reader.h"
#include "interface.h"
#include "filter.h"

//#define DEBUG_BUFFER

int gReaderFd = -1;

#ifdef DEBUG_BUFFER
int gDebugBufferFd = -1;
#endif

char * nexttoksep(char ** strp, char * sep)
{
	char * p = strsep(strp, sep);
  	return (NULL == p) ? (char *)("") : p;
}

char * nexttok(char ** strp)
{
	return nexttoksep(strp, (char *)" ");
}

int GetProcessFromDriver(int pid, char * buffer, int buffer_size) {
	return 0;
}

int GetProcessFromNative(int pid, char * buffer, int buffer_size) {
	int fd = 0;
	int r = 0;
	char cmdline[1024];
	char name[1024];
	char * ptr = NULL;
	char * ptr_name = NULL;

	snprintf(cmdline, sizeof(cmdline), "/proc/%d/cmdline", pid);
  	
	fd = open(cmdline, O_RDONLY);
    if(0 == fd) {
		r = 0;
	} else {
		r = read(fd, cmdline, 1023);
		close(fd);
		if (r < 0) {
			r = 0;
		}
	}
	cmdline[r] = '\0';

	if (0 == r) {
		//cmdline is empty, so get name from stat
		snprintf(name, sizeof(name), "/proc/%d/stat", pid);

		fd = open(name, O_RDONLY);
		if (0 == fd) {
		   r = 0;
		} else {
			r = read(fd, name, 1023);
			close(fd);
			if (r < 0) {
				r = 0;
			} 
		}

		if (0 != r) {
			name[r] = '\0';
			ptr = name;
			nexttok(&ptr); 
			ptr++;  
			ptr_name = ptr;
			ptr = strrchr(ptr, ')'); 
			*ptr++ = '\0';
		}	
	}

	if ('\0' != cmdline[0]) {
		r = snprintf(buffer, buffer_size, "%s", cmdline);
	} else if (NULL != ptr_name) {
		r = snprintf(buffer, buffer_size, "%s", ptr_name);
	} else {
		//get information failed, process may has gone
		r = 0;
	}

	return r;
}

int GetTime(int sec, int nsec, char * buffer, int buffer_size) {
	int len = 0;

	if (NULL != buffer) {
		buffer[0] = '\0';
	}

	struct tm * local = localtime((time_t *)&sec);
	len = snprintf(buffer, 
					buffer_size, 
					"%04d-%02d-%02d %02d:%02d:%02d.%03d", 
					local->tm_year + 1900, 
					local->tm_mon + 1,
					local->tm_mday,
					local->tm_hour,
					local->tm_min,
					local->tm_sec,
					nsec / 1000000);
	
	return len;
}	

bool Initize() {
	gReaderFd = open("/dev/sys_monitor", O_RDONLY);
	if (-1 == gReaderFd) {
		PERROR("open /dev/sys_monitor failed, error: %s\n", strerror(errno));

		return false;
	}

#ifdef DEBUG_BUFFER
	gDebugBufferFd = open("/sdcard/sys_reader.buffer", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	if (-1 == gDebugBufferFd) {
		PERROR("create /sdcard/sys_reader.buffer failed, error: %s\n", strerror(errno));

		close(gReaderFd);

		return false;
	}
#endif

	PDEBUG("interface initize success\n");

	return true;
}

const char * OperationToString(operation_name operation) {
	switch (operation) {
		case op_null:
			return "<Null operation>";

		case op_create_file:
			return "Create file";

		case op_read_file:
			return "Read file";

		case op_write_file:
			return "Write file";

		case op_close_file:
			return "Close file";

		case op_io_control_file:
			return "Ioctl file";

  		case op_stat_file:
			return "Stat file";

		case op_rename_file:
			return "Rename file";

		case op_make_directory:
			return "Make directory";

		case op_rename_directory:
			return "Rename directory";

		case op_sync_file:
			return "Sync file";

		case op_open_pipe:
			return "Open pipe";

		case op_symbol_link:
			return "Symbol link";

		case op_read_symbol_link:
			return "Read symbol link";

		case op_load_library:
			return "Load library";

		case op_exit_proc:
			return "Exit process";

		case op_fork_proc:
			return "Fork process";

		case op_execve_proc:
			return "Exec process";

		case op_kill_proc:
			return "Kill process";

		default:
			return "<Unknown>";
	}
}

const char * ApiToString(api_name api) {
	switch (api) {
		case api_null:
			return "<Null api>";

		case api_sys_create:
			return "sys_creat";

		case api_sys_open:
			return "sys_open";

		case api_sys_read:
			return "sys_read";

		case api_sys_write:
			return "sys_write";

		case api_sys_close:
			return "sys_close";

		case api_sys_ioctl:
			return "sys_ioctl";

        case api_sys_stat:
			return "sys_stat";

		case api_sys_lstat:
			return "sys_lstat";

		case api_sys_fstat:
			return "sys_fstat";

		case api_sys_rename:
			return "sys_rename";

		case api_sys_mkdir:
			return "sys_mkdir";

		case api_sys_rmdir:
			return "sys_rmdir";

		case api_sys_sync:
			return "sys_sync";

		case api_sys_pipe:
			return "sys_pipe";

		case api_sys_symlink:
			return "sys_symlink";

		case api_sys_readlink:
			return "sys_readlink";

		case api_sys_uselib:
			return "sys_uselib";

		case api_sys_exit:
			return "sys_exit";

		case api_sys_fork:
			return "sys_fork";

		case api_sys_vfork:
			return "sys_vfork";

		case api_sys_clone:
			return "sys_clone";

		case api_sys_execve:
			return "sys_execve";

		default:
			return "<Unknown>";
	}
}

bool ReadHeader(log_header * header) {
	while (true) {
		int size = read(gReaderFd, header, sizeof(log_header));
		if (0 == size) {
			//no data to read right now, wait a while
			PVERBOSE("no data to read, now sleep 500ms\n");
			
			usleep(500 * 1000);	//500ms
			
			continue;
		}

#ifdef DEBUG_BUFFER
		write(gDebugBufferFd, header, size);
		fsync(gDebugBufferFd);
#endif

		if (size != sizeof(log_header)) {
			PDEBUG("read size %d do not match log_header size %d, something goes wrong, exit\n", size, sizeof(log_header));
			
			return false;
		}

		PVERBOSE("read log header result:\n");
		
		PVERBOSE("\theader->sec: %d\n", header->sec);
		PVERBOSE("\theader->nsec: %d\n", header->nsec);
		PVERBOSE("\theader->pid: %d\n", header->pid);
		PVERBOSE("\theader->tid: %d\n", header->tid);
		PVERBOSE("\theader->opertaion: %s\n", OperationToString(header->operation));
		PVERBOSE("\theader->api: %s\n", ApiToString(header->api));
		PVERBOSE("\theader->result: 0x%08x\n", header->result);
		PVERBOSE("\theader->content_size: %d\n", header->content_size);

		//once go to here, everything is fine
		if (0 == header->content_size) {
			//content size is zero? it seems something goes wrong
			PWARN("header->content_size is zero\n");
		}

		return true;
	}

	//never reach here
	return false;
}

bool ReadContent(int content_size, int path_offset, int details_offset, char ** path, char ** details) {
	int readed = 0;
	int path_size = 0, details_size = 0;
	char * inner_content = NULL;

	PVERBOSE("now read log content, size: %d\n", content_size);

	if (content_size <= 0) {
		goto cleanup;
	}

	inner_content = (char *)malloc(content_size);
	if (NULL == inner_content) {
		PWARN("malloc allocate memory failed\n");

		goto cleanup;
	}

	readed = read(gReaderFd, inner_content, content_size);
	
#ifdef DEBUG_BUFFER	
	if (readed > 0) {
		write(gDebugBufferFd, inner_content, readed);
		fsync(gDebugBufferFd);
	}
#endif

	if (readed != content_size) {
		PWARN("read size %d not equal to readed size %d\n", readed, content_size);
		
		goto cleanup;
	}

	*path = inner_content;
	path_size = strlen(*path);
	*details = inner_content + path_size + 1;
	details_size = strlen(*details);


	PVERBOSE("read log content completed, %d byte(s) readed, path: %s, details: %s\n", readed, *path, *details);

	return true;

cleanup:
	if (NULL != inner_content) {
		free(inner_content);
	}

	return false;	
}

void SkipCurrentContent(int content_size) {
	lseek(gReaderFd, content_size, SEEK_CUR);
}

void Cleanup() {
	if (-1 != gReaderFd) {
		close(gReaderFd);
	}

#ifdef DEBUG_BUFFER
	if (-1 != gDebugBufferFd) {
		close(gDebugBufferFd);
	}
#endif

	PDEBUG("interface cleanup completed\n");
}
