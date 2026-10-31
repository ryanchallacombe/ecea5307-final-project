/**

Text_text.c
This is a C wrapper for a bash script which sends a request to the textbelt api

*/

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <libgen.h>
#include <fcntl.h>
#include <errno.h>


char *script_fullpath = "/home/ryan/projects/ecea5307-final-project/textbelt/tb_request.sh";
char *msg = "Hello World from C wrapper!!!";
char *phone = "9137355704";

int main(int argc, char *argv[])
{

    char *cmd[4];
    cmd[0] = script_fullpath;
    cmd[1] = msg;
    cmd[2] = phone;
    cmd[3] = NULL;

    printf("cmd[0] = %s\n", cmd[0]);
    printf("cmd[1] = %s\n", cmd[1]);
    printf("cmd[2] = %s\n", cmd[2]);

    pid_t pid;
    int status; 
    pid = fork();
    if (pid == -1) {		                            // fork error
		perror("****** fork");
		return -1;
	} else if (pid == 0) {                              // code executes in child pid
        printf("Successful fork()\n");
        if (execv(script_fullpath, cmd) == -1) {		// execv error
            perror("EXECV error"); 
            exit(EXIT_FAILURE);
        }
    }

/* 	if (waitpid(pid, &status, 0) == -1) {	            // wait error
		perror("***** waitpid");
		return -1;
	}
	else if (WEXITSTATUS(status) != 0) {
        printf("WEXITSTATUS(status)");	
		return -1;
	}
	else {
		// if we get to this point, no errors have occured, so set return value to true

		return true;
	} */

    waitpid(pid, &status, 0) ;
    if (WIFEXITED(status)) {
        int exit_code = WEXITSTATUS(status);
        printf("Parent: Child ended normally with exit code: %d\n", exit_code);
    } else {
        printf("Parent: Child terminated abnormally (e.g., crashed or killed by signal).\n");
    }


    return 0;
}