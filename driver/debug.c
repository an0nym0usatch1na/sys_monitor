#include "debug.h"

#ifdef __KERNEL__

#include <linux/sched.h>
#include <linux/unistd.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/timer.h>
#include <linux/timex.h>
#include <linux/rtc.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <asm/current.h>

//kernel version of log_event
void log_event(const char log_level, const char * file, int line, const char * fmt, ...)
{
	    char * this_file;
	    char buffer[512];
	    va_list args;
	    struct timex txc;
	    struct rtc_time tm;

	    do_gettimeofday(&(txc.time));
	    rtc_time_to_tm(txc.time.tv_sec, &tm);

	    buffer[0] = '\0';

	    va_start(args, fmt);
	    vsnprintf(buffer, sizeof(buffer), fmt, args);
	    va_end(args);

	    this_file = strrchr(file, '/');
	    if (NULL != this_file)
	    {
	        //skip '/'
	        this_file = this_file + 1;
	    }
	    else
	    {
	        //failed, then use default log file name
	        this_file = (char *)file;
	    }

	    printk("%s[SYSMON][%c][%s:%d][%02d:%02d:%02d]%s",
	            KERN_INFO,
	            log_level,
	            this_file,
	            line,
	            tm.tm_hour,
	            tm.tm_min,
	            tm.tm_sec,
	            buffer);
}

#else

#include <stdio.h>
#include <stdarg.h>

//user version of log_event
void log_event(const char log_level, const char * file, int line, const char * fmt, ...)
{
	char * this_file;
	char buffer[512];
	va_list args;

	buffer[0] = '\0';
	
	va_start(args, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, args);
	va_end(args);
	
	this_file = strrchr(file, '/');
	if (NULL != this_file)
	{
		this_file = this_file + 1;
	}
	else
	{
		this_file = (char *)file;
	}

	fprintf(stderr, "[SYSMON][%c][%s:%d]%s",
			log_level,
			this_file,
			line,
			buffer);
}

#endif
