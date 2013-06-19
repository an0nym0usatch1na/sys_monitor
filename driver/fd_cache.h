#ifndef SYS_MONITOR_FD_CACHE_H
#define SYS_MONITOR_FD_CACHE_H

struct process_record;

void fd_cache_initize(process_record * record);

void fd_cache_cleanup(process_record * record);

char * get_cache_by_fd(unsigned int fd);

bool delete_cache_by_fd(unsigned int fd);

void insert_into_cache(unsigned int fd, char * path);

#endif
