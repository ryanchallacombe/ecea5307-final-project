/*
** aesd_client.c
**
** Advanced Embedded Linux Programming: assignment 5 part 1
**
**		writer: ryan challacombe
**		date:	9/22/2026
*/

/*
** Usage: >> aesd_client <server_ip> <optional -d for daemon mode>
*/

#include "include/utils.h"

/**********************************
*	Defines
**********************************/

#define MAXDATASIZE		1024						// max number of bytes
#define RECVFILE		"/var/tmp/aesd_client_data"	// file for storing received messages
#define TCP_PORT 		"4242"
#define BUF_SIZE        2048

/**********************************
*	Globals
**********************************/

extern bool caught_signal;			// defined in utils.c
char *Q_IDN = "IDN?";
char *R_IDN = "pico w";				// expected response to query
char *Q_DATA = "DATA?";
char recv_buffer[BUF_SIZE];
size_t numbytes; 

/**********************************
*	Main
**********************************/
int main(int argc, char *argv[]) {


	/* Setup syslog */
	openlog(NULL, 0, LOG_USER);

	/* Setup a return value for the program */
	int rtn_val = 0;

	/* Let user know program is starting */
	const char *prog_name = argv[0];
	syslog(LOG_DEBUG, "***** Starting program: %s\n", prog_name);


	/************************* Signal handler setup *************************/
	caught_signal = false;			// Signal handler flag
	struct sigaction new_action;	

    memset( &new_action, 0, sizeof(struct sigaction) );
    new_action.sa_handler = signal_handler;

    if( sigaction(SIGTERM, &new_action, NULL) != 0 ) {
        printf("Error %d (%s) registering for SIGTERM",errno,strerror(errno));
        rtn_val = -1;
        goto DONE;
    }
    if( sigaction(SIGINT, &new_action, NULL) ) {
        printf("Error %d (%s) registering for SIGINT",errno,strerror(errno));
        rtn_val = -1;
        goto DONE;
    }

    /************************* Remove write file if it exists *************************/
	char *wr_file_path = RECVFILE;
	errno = 0;
	FILE *fp;
	if ( (fp = fopen(wr_file_path, "r")) != NULL ) {
		printf("Removing file: %s\n", wr_file_path);
		fclose(fp);
		if ( (remove(wr_file_path)) == -1)
			perror("remove");
	}
	
	/************************* Socket *************************/
	// setup
	//const char *port = "4242";
	struct addrinfo hints;
	struct addrinfo *servinfo;				// will point to the linked list of results

	// hints setup
	memset(&hints, 0, sizeof(hints));		// make sure that the struct is cleared
	hints.ai_family = AF_UNSPEC;			// don't care if ipv4 or ipv6
	hints.ai_socktype = SOCK_STREAM; 		// TCP stream sockets
	hints.ai_flags = AI_PASSIVE;     		// fill in my localhost IP for me

    if (argc != 2) {
        printf("usage: aesd_client <server_ip>\n");
		rtn_val = -1;
		goto DONE;
    }

	// getaddrinfo call
	int ret;
	if ( (ret = getaddrinfo(argv[1], TCP_PORT, &hints, &servinfo)) != 0 ) {
		// log error
		printf("getaddrinfo() call error: %s\n", gai_strerror(ret));
		rtn_val = -1;
		goto DONE;
	}

	// socket call
	int sockfd;
	errno = 0;
	if ( (sockfd = socket(servinfo->ai_family, servinfo-> ai_socktype, servinfo->ai_protocol)) == -1 ) {
		printf("socket() call error: %s\n", strerror(errno));
		rtn_val = -1;
		goto DONE;
	}
	syslog(LOG_DEBUG, "***** Socket file set with descriptor: %d\n", sockfd);

	// set SO_REUSEADDR on a socket to true (1):
	int optval = 1;
	socklen_t optlen;
	if ( setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) != 0) {
		perror("setsockopt");
	}

	getsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, &optlen);
	if (optval != 0) {
    	printf("SO_REUSEADDR enabled on sockfd\n");
	}

	// connect() call
	errno = 0;
	if ( (connect(sockfd, servinfo->ai_addr, servinfo->ai_addrlen)) == -1 ) {
		printf("connect()) call error: %s\n", strerror(errno));
		rtn_val = -1;
		close(sockfd);
		goto DONE;
	} 

	// Print address of connected server
	char ipstr[INET_ADDRSTRLEN];  				// space to hold the IPv4 string
	inet_ntop(servinfo->ai_family, (struct sockaddr_in *)servinfo->ai_addr, ipstr, INET_ADDRSTRLEN);
	syslog(LOG_DEBUG, "Client connected to %s\n", ipstr);
	printf("Client connected to %s\n", ipstr);

	// TODO: support daemon mode or delete
	/********** Daemon mode **********/
	
	// Daemon mode is invoked by argument 1 as '-d' when this application is invoked from command line
/* 	pid_t pid;
	if (argc > 1) {			// check to ensure we got a command line argument
		if ( !strncmp("-d", argv[1], 2) ) {
			printf("Daemon mode invoked.\n");

			pid = fork();
			if ( pid == -1) {			// fork error
				perror("fork");
				rtn_val = -1;
				goto DONE;
			} else if ( pid != 0 ) {	// pid = 0 is child pid

				// exit parent process
				printf("Exiting parent process\n");
				rtn_val = 0;
				goto DONE;
			}
		}
	} */

	errno = 0;
	char *msg = Q_IDN;
	if ( send(sockfd, msg, sizeof(msg)*sizeof(char), 0) == -1 ) {
		printf("send() call error: %s\n", strerror(errno));
		rtn_val = -1;
		close(sockfd);
		goto DONE;
	}

	errno = 0;
	if ((numbytes = recv(sockfd, recv_buffer, BUF_SIZE-1, 0)) == -1) {
		printf("recv() call error: %s\n", strerror(errno));
		rtn_val = -1;
		close(sockfd);
		goto DONE;
    }

    recv_buffer[numbytes] = '\0';
	printf("Recieved from server: %s\n", recv_buffer);


	/**************** LOOP  ****************/ 

	while (!caught_signal) 
	{
		sleep(1);
	}

	/**************** Cleanup and Exit ****************/
 
	DONE:

	close(sockfd);
	freeaddrinfo(servinfo);		// free the linked list

	if ( caught_signal == true ) {
		syslog(LOG_DEBUG, "Caught signal, exiting\n");
		printf("Caught signal, exiting\n");
	}

	/* Let user know program is ending */
	syslog(LOG_DEBUG, "***** Exiting program %s with return value: %d\n", prog_name, rtn_val);
	printf("***** Exiting program %s with return value: %d\n", prog_name, rtn_val);

	return rtn_val;
}