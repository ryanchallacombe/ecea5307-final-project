#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>



#ifndef UTILS_H
#define	UTILS_H

#define INITIAL_BUFFER_SIZE		128

// bool caught_signal;

int writer_func(const char *fpath, const char *buf);
void signal_handler ( int signal_number );
char *read_until_term(int fd, const char term, int *rtn_flag);
ssize_t readLine(int fd, void *buffer, size_t n);


#endif