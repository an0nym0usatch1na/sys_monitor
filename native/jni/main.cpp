#include <stdio.h>  
#include <stdlib.h>  
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#define SYSMON_DEBUG
extern "C" {
	#include "./../../share/log_def.h"
	#include "./../../share/debug.h"
}

#include "sys_reader.h"
#include "filter.h"
#include "interface.h"

int main(int argc, char ** argv) {
	char buffer[10240];
	char time[64];
	char proc[256];
	action_result result = result_none;

	//AddFilter((char *)"api is sys_fork then include");
	//AddFilter((char *)"api is sys_vfork then include");
	//AddFilter((char *)"api is sys_execve then include");
	//AddFilter((char *)"api is sys_clone then include");
	//AddFilter((char *)"api is sys_exit then include");
	//dFilter((char *)"api is sys_open the include");
	//AddFilter((char *)"api is sys_creat then include");
	//AddFilter((char *)"api is sys_read then include");
	AddFilter((char *)"path contains a.txt then include");
	//AddFilter((char *)"api is sys_close then include");

	if (Initize()) {
		while (true) {
			log_header header;
			bool suc = false;
		
			//read header	
			suc = ReadHeader(&header);
			if (!suc) {
				break;
			}
			
			char * path = NULL;
			char * details = NULL;

			//read content
			suc = ReadContent(header.content_size, header.path_offset, header.details_offset, &path, &details);
			if (!suc) {
				break;
			}

			//do filter	
			result = FilterLog(&header, path, details);
			PVERBOSE("Log %s:%s filter result: %d\n", path, details, result);

			if (result_include == result) {
				GetTime(header.sec, header.nsec, time, 64);
				GetProcess(header.pid, proc, 256);

				snprintf(buffer,
							sizeof(buffer),
							"[%s][%d][%s][%s][%s][%s][0x%08x][%s]\n",
							time,
							header.pid,
							proc,
							OperationToString(header.operation),
							ApiToString(header.api),
							path,
							header.result,
							details);

				printf(buffer);
			}

			//do cleanup here
			if (NULL != path) {
				free(path);
			}
		}	

		Cleanup();
	}

	return 0;
}
