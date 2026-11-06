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
#include "textbelt_utils.h"

char *script_fullpath = "/home/ryan/projects/ecea5307-final-project/textbelt/tb_request.sh";
char *msg = "Hello World from C wrapper!!!";
char *phonenumber = "9137355704";

int main(int argc, char *argv[])
{
    exec_textbelt_call(script_fullpath, msg, phonenumber);

    return 0;
}