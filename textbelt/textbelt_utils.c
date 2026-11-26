/**
textbelt_utils.c
This is a C wrapper for a bash script which sends a request to the textbelt api
*/

#include "textbelt_utils.h"

/*
*   exec_textbelt_call()
*   
*   inputs:
*       script_fullpath = path to bash script the executes the textbelt call
*       msg = message string
*       phonenumber = phone number string
*
*   return value: 0 if fork/execv executed / returend normally, 1 otherwise             
*/

int exec_textbelt_call(char *script_fullpath, char *msg, char *phonenumber) {

    char *cmd[4];
    cmd[0] = script_fullpath;
    cmd[1] = msg;
    cmd[2] = phonenumber;
    cmd[3] = NULL;

    pid_t pid;
    int status; 
    pid = fork();
    if (pid == -1) {		                            // fork error
		perror("****** fork");
		return 1;
	} else if (pid == 0) {                              // code executes in child pid
        if (execv(script_fullpath, cmd) == -1) {		// execv error
            perror("EXECV error"); 
            exit(EXIT_FAILURE);
        }
    }

    waitpid(pid, &status, 0) ;
    if (! WIFEXITED(status)) {
        //printf("Parent: Child terminated abnormally (e.g., crashed or killed by signal).\n");
        return 1;
    }

    return 0;
}


/* Read characters from 'fd' until a newline is encountered. If a newline
  character is not encountered in the first (n - 1) bytes, then the excess
  characters are discarded. The returned string placed in 'buf' is
  null-terminated and includes the newline character if it was read in the
  first (n - 1) bytes. The function return value is the number of bytes
  placed in buffer (which includes the newline character if encountered,
  but excludes the terminating null byte). */

// Credit: https://man7.org/tlpi/code/online/dist/sockets/read_line.c.html

ssize_t readLine(int fd, void *buffer, size_t n)
{
    ssize_t numRead;                    // # of bytes fetched by last read()
    size_t totRead;                     // Total bytes read so far
    char *buf;
    char ch;

    if (n <= 0 || buffer == NULL) {
        errno = EINVAL;
        return -1;
    }

    buf = buffer;                       // No pointer arithmetic on "void *" 

    totRead = 0;
    for (;;) {
        numRead = read(fd, &ch, 1);

        if (numRead == -1) {
            if (errno == EINTR)         // Interrupted --> restart read()
                continue;
            else
                return -1;              // Some other error 

        } else if (numRead == 0) {      // EOF
            if (totRead == 0)           // No bytes read; return 0
                return 0;
            else                        // Some bytes read; add '\0' 
                break;

        } else {                        // 'numRead' must be 1 if we get here 
            if (totRead < n - 1) {     // Discard > (n - 1) bytes 
                totRead++;
                *buf++ = ch;
            }

            if (ch == '\n')
                break;
        }
    }

    *buf = '\0';
    return totRead;
} 