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

