/**

Text_text.c
This is a C wrapper for a bash script which sends a request to the textbelt api

*/

#include "textbelt_utils.h"

#define SUBSCRIBER_FILE     "subscribers.conf"
#define MAX_SUBSCRIBER_CT   5
#define PHONE_NUMBER_CHAR   11

char *script_fullpath = "tb_request.sh";
char *msg = "Hello World from C wrapper!!!";
char *phonenumber = "9137355704";

int main(int argc, char *argv[])
{

    // open file
    char *subscriber_file_path = SUBSCRIBER_FILE;
    int fd = open(subscriber_file_path, O_RDONLY, 0644);
    if (fd == -1) {
        perror("Error opening file");
        return 1;
    }

    printf("File opened successfully. File descriptor: %d\n", fd);

    // setup buffer for reading the subscriber phone numbers
    int max_subscriber_ct = MAX_SUBSCRIBER_CT;
    int phone_number_char = PHONE_NUMBER_CHAR;
    char subscriber_phone_numbers[max_subscriber_ct][phone_number_char];

    int num_read = 0;
    int i = 0;
    for (i = 0; i<MAX_SUBSCRIBER_CT; i++) {
        if ( (num_read = readLine(fd, subscriber_phone_numbers[i], phone_number_char)) == 0) {
            break;
        } 
        printf("subscriber_phone_numbers[i]: %s\n", subscriber_phone_numbers[i]);
    }
    printf("%i subscriber numbers read from subscriber file\n", i);

    for (int j = 0; j<i; j++) {
        exec_textbelt_call(script_fullpath, msg, subscriber_phone_numbers[j]);
    }

    // close the file descriptor
    close(fd);

    return 0;
}