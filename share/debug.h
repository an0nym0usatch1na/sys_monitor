/*
//Output all discard file settings
#undef SYSMON_VERBOSE
#undef SYSMON_DEBUG
#undef SYSMON_WARN
#undef SYSMON_ERROR

#define SYSMON_ERROR
*/

#if defined(SYSMON_VERBOSE)			//output max level

#	define SYSMON_DEBUG
#	define SYSMON_WARN	
#	define SYSMON_ERROR

#elif defined(SYSMON_DEBUG)			//debug level

#	define SYSMON_WARN
#	define SYSMON_ERROR

#elif defined(SYSMON_WARN)			//warnning level

#	define SYSMON_ERROR

#elif defined(SYSMON_ERROR)			//only error, do nothing here

#else								//default level, erro + warn + debug

#	define SYSMON_DEBUG
#	define SYSMON_WARN
#	define SYSMON_DEBUG

#endif

//log define down here
#undef PERROR             	//undef it, every file has its own settings 
#undef PWARN
#undef PDEBUG
#undef PVERBOSE

//log function define here
#ifndef _DEBUG_LOG_OPT_
#define _DEBUG_LOG_OPT_

#define LOG_LEVEL_ERROR     'E'
#define LOG_LEVEL_WARN      'W'
#define LOG_LEVEL_DEBUG     'D'
#define LOG_LEVEL_VERBOSE   'V'
#define LOG_LEVEL_ASSERT	'A'

#ifdef _DEBUG

#	define assert(expr) \
		if (unlikely(!(expr))) {                         \
		log_event(LOG_LEVEL_ASSERT, __FILE__, __LINE__,  \
		"Assertion failed: %s\n", #expr);     			\
		}

#else

#	define assert(expr)

#endif

//function defines
void log_event(const char log_level, const char * file, int line, const char * fmt, ...);

#endif

#ifdef SYSMON_VERBOSE
#	define PVERBOSE(fmt, args...) log_event(LOG_LEVEL_VERBOSE, __FILE__, __LINE__, fmt, ## args)
#else
#	define PVERBOSE(fmt, args...)	//or, nothing 
#endif

#ifdef SYSMON_DEBUG
#   define PDEBUG(fmt, args...) log_event(LOG_LEVEL_DEBUG, __FILE__, __LINE__, fmt, ## args)
#else
#  	define PDEBUG(fmt, args...) 
#endif

#ifdef SYSMON_ERROR
#   define PERROR(fmt, args...) log_event(LOG_LEVEL_ERROR, __FILE__, __LINE__, fmt, ## args)
#else
#  	define PERROR(fmt, args...) 
#endif

#ifdef SYSMON_WARN
#   define PWARN(fmt, args...) log_event(LOG_LEVEL_WARN, __FILE__, __LINE__, fmt, ## args)
#else
#  	define PWARN(fmt, args...) 
#endif
