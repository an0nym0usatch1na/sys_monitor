#ifndef _SYS_READER_INTERFACE_H
#define _SYS_READER_INTERFACE_H

bool Initize();

const char * OperationToString(operation_name operation);

const char * ApiToString(api_name api);

bool ReadHeader(log_header * header);

bool ReadContent(int content_size, int path_offset, int details_offset, char ** path, char ** details);

void Cleanup();

int GetTime(int sec, int nsec, char * buffer, int buffer_size);

int GetProcess(int pid, char * buffer, int buffer_size);

#endif
