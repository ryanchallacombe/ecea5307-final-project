#ifndef TEXTBELT_UTILS_H
#define TEXTBELT_UTILS_H

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <libgen.h>
#include <fcntl.h>
#include <errno.h>

int exec_textbelt_call(char *script_fullpath, char *msg, char *phonenumber);
ssize_t readLine(int fd, void *buffer, size_t n);

#endif