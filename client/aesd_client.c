/*
** aesd_client.c
**
** Advanced Embedded Linux Programming: Final Project
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

// #define MAXDATASIZE		1024						// max number of bytes
#define RECVFILE "/var/tmp/aesd_client_data" // file for storing received messages
#define TCP_PORT "4242"
#define RCV_BUF_SIZE 1024
#define SEND_BUF_SIZE 512
#define DATA_BUF_SIZE 256
#define BASELINE_REQ_COUNT 100
#define REQ_COUNT_10 10
#define REQ_COUNT_25 25
#define REQ_COUNT_50 50
#define LOOP_REQ_COUNT REQ_COUNT_25

/**********************************
 *	Globals
 **********************************/

extern bool caught_signal; // defined in utils.c
char *Q_IDN = "IDN?";
char *R_IDN = "pico w"; // expected response to query
char *Q_DATA = "DATA?";
/* char recv_buffer[RCV_BUF_SIZE];
char send_buffer[SEND_BUF_SIZE];
float data_buffer[DATA_BUF_SIZE]; */
size_t numbytes;
int g_passive_mon_sleep_secs = 10; // while loop sleep time for when machine is OFF
int g_active_mon_sleep_secs = 10;  // while loop sleep time for when machine is ON
bool D_RUN = false;				   // 'dryer running' flag

struct Buffers
{
	char recv_buffer[RCV_BUF_SIZE];
	char send_buffer[SEND_BUF_SIZE];
	float data_buffer[DATA_BUF_SIZE];

	/* 	lengths are updated when data is put into the respective buffer
		note that recv and send lengths are the number of bytes because
		their data type is char which is 1 byte
		however, the data number is a count because a float is generally 4 bytes
	*/
	int recv_len;
	int send_len;
	int data_count;
	int data_bytes;
};

struct Buffers buf = {0};
struct Buffers *buffers = &buf;

struct AxisStats
{
	float mean;
	float stddev;
};

/**********************************
 *	Function protos
 **********************************/
int sock_recv_chars(int fd, struct Buffers *buf);
int sock_recv_data(int fd, struct Buffers *buf);
int sock_send_chars(int fd, struct Buffers *buf, char *msg);
int sock_send_data_req(int fd, struct Buffers *buf, char *msg, int axis, int count);
void calc_stats(float *buf, int count, struct AxisStats *ax_stats);

/**********************************
 *	Main
 **********************************/
int main(int argc, char *argv[])
{

	/* Setup syslog */
	openlog(NULL, 0, LOG_USER);

	/* Setup a return value for the program */
	int rtn_val = 0;

	/* Let user know program is starting */
	const char *prog_name = argv[0];
	syslog(LOG_DEBUG, "***** Starting program: %s\n", prog_name);
	printf("***** Starting program: %s\n", prog_name);

	// show wall clock time
	time_t now = time(NULL);
	syslog(LOG_DEBUG, "***** Current time: %s\n", ctime(&now));
	printf("***** Current time: %s\n", ctime(&now));

	// check to make sure the server ip was passed in as an argument
	if (argc < 2)
	{
		printf("usage: aesd_client <server_ip>\n");
		rtn_val = -1;
		goto DONE;
	}

	/************************* Signal handler setup *************************/
	caught_signal = false; // Signal handler flag
	struct sigaction new_action;

	memset(&new_action, 0, sizeof(struct sigaction));
	new_action.sa_handler = signal_handler;

	if (sigaction(SIGTERM, &new_action, NULL) != 0)
	{
		printf("Error %d (%s) registering for SIGTERM", errno, strerror(errno));
		rtn_val = -1;
		goto DONE;
	}
	if (sigaction(SIGINT, &new_action, NULL))
	{
		printf("Error %d (%s) registering for SIGINT", errno, strerror(errno));
		rtn_val = -1;
		goto DONE;
	}

	/************************* Remove write file if it exists *************************/
	char *wr_file_path = RECVFILE;
	errno = 0;
	FILE *fp;
	if ((fp = fopen(wr_file_path, "r")) != NULL)
	{
		printf("Removing file: %s\n", wr_file_path);
		fclose(fp);
		if ((remove(wr_file_path)) == -1)
			perror("remove");
	}

	/********** Daemon mode **********/

	// Daemon mode is invoked by argument 2 as '-d' when this application is invoked from command line
	pid_t pid;
	if (argc > 2) { // check to ensure we got a command line argument
		if (!strncmp("-d", argv[2], 2))
		{
			printf("Daemon mode invoked.\n");

			pid = fork();
			if (pid == -1)
			{ // fork error
				perror("fork");
				rtn_val = -1;
				goto PARENT_DONE;
			}
			else if (pid != 0)
			{ // pid = 0 is child pid

				// exit parent process
				printf("Exiting parent process\n");
				rtn_val = 0;
				goto PARENT_DONE;
			}
		}
	}

	/************************* Socket *************************/
	// setup
	// const char *port = "4242";
	struct addrinfo hints;
	struct addrinfo *servinfo; // will point to the linked list of results

	// hints setup
	memset(&hints, 0, sizeof(hints)); // make sure that the struct is cleared
	hints.ai_family = AF_UNSPEC;	  // don't care if ipv4 or ipv6
	hints.ai_socktype = SOCK_STREAM;  // TCP stream sockets
	hints.ai_flags = AI_PASSIVE;	  // fill in my localhost IP for me



	// getaddrinfo call
	int ret;
	if ((ret = getaddrinfo(argv[1], TCP_PORT, &hints, &servinfo)) != 0)
	{
		// log error
		printf("getaddrinfo() call error: %s\n", gai_strerror(ret));
		rtn_val = -1;
		goto DONE;
	}

	// socket call
	int sockfd;
	errno = 0;
	if ((sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol)) == -1)
	{
		printf("socket() call error: %s\n", strerror(errno));
		rtn_val = -1;
		goto DONE;
	}
	syslog(LOG_DEBUG, "***** Socket file set with descriptor: %d\n", sockfd);

	// set SO_REUSEADDR on a socket to true (1):
	int optval = 1;
	socklen_t optlen;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) != 0)
	{
		perror("setsockopt");
	}

	getsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, &optlen);
	if (optval != 0)
	{
		printf("SO_REUSEADDR enabled on sockfd\n");
	}

	// connect() call
	errno = 0;
	if ((connect(sockfd, servinfo->ai_addr, servinfo->ai_addrlen)) == -1)
	{
		printf("connect()) call error: %s\n", strerror(errno));
		rtn_val = -1;
		close(sockfd);
		goto DONE;
	}

	// Print address of connected server
	char ipstr[INET_ADDRSTRLEN]; // space to hold the IPv4 string
	inet_ntop(servinfo->ai_family, (struct sockaddr_in *)servinfo->ai_addr, ipstr, INET_ADDRSTRLEN);
	syslog(LOG_DEBUG, "Client connected to %s\n", ipstr);
	printf("Client connected to %s\n", ipstr);

	/********** Daemon mode **********/

/* 	// Daemon mode is invoked by argument 2 as '-d' when this application is invoked from command line
	pid_t pid;
	if (argc > 2) { // check to ensure we got a command line argument
		if (!strncmp("-d", argv[2], 2))
		{
			printf("Daemon mode invoked.\n");

			pid = fork();
			if (pid == -1)
			{ // fork error
				perror("fork");
				rtn_val = -1;
				goto DONE;
			}
			else if (pid != 0)
			{ // pid = 0 is child pid

				// exit parent process
				printf("Exiting parent process\n");
				rtn_val = 0;
				goto DONE;
			}
		}
	} */

	/************************* INITIAL COMMS *************************/

	// query for ID
	if ((rtn_val = sock_send_chars(sockfd, buffers, Q_IDN)) == -1)
	{
		goto DONE;
	}

	if ((rtn_val = sock_recv_chars(sockfd, buffers)) == -1)
	{
		goto DONE;
	}

	// todo: check for correct response to Q_IDN
	//		if wrong goto done

	printf("Recieved from server: %s\n", buffers->recv_buffer);

	/**************** SET INITIAL VIBE THRESHOLD FOR SYSTEM OFF STATE  ****************/

	/****
	 * todo: consider hardcoded threshold to verify that system
	 * is off before proceeding with initialization
	 */

	// setup axis stats for baseline here
	struct AxisStats base_z_stats = {0};
	struct AxisStats *base_z_axis = &base_z_stats;

	// notify user of init process, don't touch system
	printf("*** Establishing baseline stats ****\n");
	printf("*** Set system on unmoving vibration target\n");
	int axis = 3;
	float baseline_recv_values[BASELINE_REQ_COUNT];
	for (int i = 0; i < BASELINE_REQ_COUNT; i++)
	{
		// query for data
		if ((rtn_val = sock_send_data_req(sockfd, buffers, Q_DATA, axis, 1)) == -1)
		{
			goto DONE;
		}

		// recieve
		if ((rtn_val = sock_recv_data(sockfd, buffers)) == -1)
		{
			goto DONE;
		}
		baseline_recv_values[i] = *buffers->data_buffer;
		// printf("Recieved: %.4f\n", baseline_recv_values[i]);
	}

	calc_stats(baseline_recv_values, BASELINE_REQ_COUNT, base_z_axis);
	printf("baseline mean_val = %.4fg\n", base_z_axis->mean);
	printf("baseline stddev = %.4fg\n", base_z_axis->stddev);
	syslog(LOG_DEBUG, "baseline mean_val = %.4fg\n", base_z_axis->mean);
	syslog(LOG_DEBUG, "baseline stddev = %.4fg\n", base_z_axis->stddev);

	// establish 3 sigma stddev threshold here
	float baseline_stddev_ths = 3 * base_z_axis->stddev;
	printf("stddev threshold set to %.4fg\n", baseline_stddev_ths);
	syslog(LOG_DEBUG, "stddev threshold set to %.4fg\n", baseline_stddev_ths);

	/***
	 * establish an out-of-bounds threshold to use for filtering out extreme events
	 * any data captures with 1 value above this threshold must be redone
	 * This filters out the closing of the dryer door
	 */
	int oob_sigma_scalar = 100;
	float OOB_ths = base_z_axis->mean + oob_sigma_scalar * base_z_axis->stddev;
	printf("OOB stddev threshold set to %i std deviations: %.4fg\n", oob_sigma_scalar, OOB_ths);

	printf("*** Baseline setup completed ****\n\n\n");

	/**************** LOOP  ****************/

	// setup axis stats for loop here
	struct AxisStats z_stats = {0};
	struct AxisStats *z_axis = &z_stats;

	// setup vars for loop

	float recv_values[LOOP_REQ_COUNT]; // buffer for storing received vibe data
	int loop_sleep_secs = g_passive_mon_sleep_secs;
	bool OOB_flag = false;

	while (!caught_signal)
	{
		/*******
		 * Get data and process stats
		 * check against baseline stddev threshold
		 */

		do
		{
			printf("****************\n");
			printf("Querying for %i samples on axis %i\n", LOOP_REQ_COUNT, axis);
			syslog(LOG_DEBUG, "****************\n");
			syslog(LOG_DEBUG, "Querying for %i samples on axis %i\n", LOOP_REQ_COUNT, axis);
			now = time(NULL);
			syslog(LOG_DEBUG, "***** Current time: %s\n", ctime(&now));
			for (int i = 0; i < LOOP_REQ_COUNT; i++)
			{
				OOB_flag = false;

				// query for data
				if ((rtn_val = sock_send_data_req(sockfd, buffers, Q_DATA, axis, 1)) == -1)
				{
					goto DONE;
				}

				// recieve
				if ((rtn_val = sock_recv_data(sockfd, buffers)) == -1)
				{
					goto DONE;
				}
				// printf("loop i = %i, received %i bytes\n", i, buffers->data_bytes);
				recv_values[i] = *buffers->data_buffer;
				// printf("Recieved: %.4f\n", recv_values[i]);

				if (recv_values[i] > OOB_ths)
				{
					printf("\n***OOB value received: %.4fg\n\n", recv_values[i]);
					OOB_flag = true;
					break;
				}
			}
		} while (OOB_flag);

		calc_stats(recv_values, LOOP_REQ_COUNT, z_axis);
		printf("mean_val = %.4f\n", z_axis->mean);
		printf("stddev = %.4f\n", z_axis->stddev);
		syslog(LOG_DEBUG, "mean_val = %.4f\n", z_axis->mean);
		syslog(LOG_DEBUG, "stddev = %.4f\n", z_axis->stddev);

		/**************** THRESHOLD CHECK  AND ACTIONS ****************/
		if (z_stats.stddev > baseline_stddev_ths)
		{
			printf("**** stddev %.4fg exceeds threshold %.4fg ****\n\n", z_axis->stddev, baseline_stddev_ths);

			if (!D_RUN)
			{
				D_RUN = true;
				printf("******* D_RUN flag set to true\n");
				loop_sleep_secs = g_active_mon_sleep_secs;
				// todo send textbelt call here ... dryer started
			}
			else
			{
				printf("Dryer is still running\n");
				syslog(LOG_DEBUG, "Dryer is still running\n");
			}
		}
		else
		{
			if (D_RUN)
			{
				D_RUN = false;
				printf("******* D_RUN flag set to false\n\\");
				printf("***************** Dryer stopped\n");
				syslog(LOG_DEBUG, "Dryer stopped\n");
				now = time(NULL);
				syslog(LOG_DEBUG, "***** Current time: %s\n\n\n", ctime(&now));
				loop_sleep_secs = g_passive_mon_sleep_secs;
				// todo send textbelt call here ... dryer done
			}
		}

		/**************** SLEEP ****************/
		sleep(loop_sleep_secs);

	} // while

	/**************** Cleanup and Exit ****************/

DONE:

	close(sockfd);
	freeaddrinfo(servinfo); // free the linked list

PARENT_DONE:		// DONE section for -d daemon mode

	if (caught_signal == true)
	{
		syslog(LOG_DEBUG, "Caught signal, exiting\n");
		printf("Caught signal, exiting\n");
	}

	/* Let user know program is ending */
	syslog(LOG_DEBUG, "***** Exiting program %s with return value: %d\n", prog_name, rtn_val);
	printf("***** Exiting program %s with return value: %d\n", prog_name, rtn_val);

	return rtn_val;
}

/**************** Functions ****************/
// TODO move these to utils file

/***********
 * sock_recv_chars()
 * 		inputs: fd = the socket fd
 * 				*buf = ptr to struct Buffers
 *
 * 		returns:
 * 				0 	= successful receive
 * 				-1 	= error in recv
 */
int sock_recv_chars(int fd, struct Buffers *buf)
{

	int sockfd = fd;
	// clear the receive buffer and length
	memset(buf->recv_buffer, 0, RCV_BUF_SIZE);
	buf->recv_len = 0;

	errno = 0;
	int numbytes = recv(sockfd, buf->recv_buffer, RCV_BUF_SIZE, 0);
	if (numbytes == -1)
	{
		printf("error in sock_recv_chars() in recv() call: %s\n", strerror(errno));
		return -1;
	}

	buf->recv_len = numbytes;
	buf->recv_buffer[numbytes] = '\0'; // null termination for string
	return 0;
}

/***********
 * sock_recv_data())
 * 		inputs: fd = the socket fd
 * 				*buf = ptr to struct Buffers
 *
 * 		returns:
 * 				0 	= successful receive
 * 				-1 	= error in recv
 */
// todo: handle 'connection reset by peer
int sock_recv_data(int fd, struct Buffers *buf)
{

	int sockfd = fd;
	// clear the receive buffer and length
	memset(buf->data_buffer, 0, DATA_BUF_SIZE * sizeof(float));
	buf->data_count = 0;
	buf->data_bytes = 0;

	errno = 0;
	int numbytes = recv(sockfd, buf->data_buffer, DATA_BUF_SIZE, 0);
	if (numbytes == -1)
	{
		printf("error in sock_recv_data() in recv() call: %s\n", strerror(errno));
		return -1;
	}

	buf->data_bytes = numbytes;
	return 0;
}

/***********
 * sock_send_chars())
 * 		inputs: fd = the socket fd
 * 				*buf = ptr to struct Buffers
 * 				* msg = ptr to message to send
 *
 * 		returns:
 * 				0 	= successful send
 * 				-1 	= error in recv
 */
int sock_send_chars(int fd, struct Buffers *buf, char *msg)
{

	int sockfd = fd;
	// clear the receive buffer
	memset(buf->send_buffer, 0, SEND_BUF_SIZE);
	strncpy(buffers->send_buffer, msg, strlen(msg));
	buf->send_len = sizeof(msg);

	errno = 0;
	int numbytes = send(sockfd, buf->send_buffer, buf->send_len, 0);
	if (numbytes == -1)
	{
		printf("error in sock_esnd_chars() in send() call: %s\n", strerror(errno));
		return -1;
	}

	return 0;
}

/***********
 * sock_send_data_req())
 * 		inputs: fd = the socket fd
 * 				*buf = ptr to struct Buffers
 * 				* msg = ptr to message to send
 * 				axis = the axis to sample (x=1, y=2, z=3)
 * 				count = number of samples
 *
 * 		returns:
 * 				0 	= successful send
 * 				-1 	= error in recv
 */
int sock_send_data_req(int fd, struct Buffers *buf, char *msg, int axis, int count)
{

	int sockfd = fd;
	// clear the receive buffer
	memset(buf->send_buffer, 0, SEND_BUF_SIZE);

	/*********
	 * build full message
	 * start with msg and add the suffix to it
	 * suffix is <axis>,<count>
	 *  Only 1 digit count supported
	 *
	 * Note that msg is a string literal
	 * so it has '\0'
	 * but strlen("DATA?") does not count it
	 *
	 * be careful using strlen in this section
	 */

	// build the suffix
	int suf_count = 3; // Only 1 digit count supported
	char suf[suf_count];
	suf[0] = axis + '0';
	suf[1] = ',';
	suf[2] = count + '0';
	// printf("Requesting <axis> #%i with <count> %i\n", axis, count);

	// now build the full message
	int msg_count = strlen(msg);
	int full_msg_count = msg_count + suf_count;
	char full_msg[full_msg_count];
	strncpy(full_msg, msg, msg_count); // does not include null terminator when copied
	strncpy(full_msg + msg_count, suf, suf_count);
	char str_full_msg[full_msg_count + 1];
	strncpy(str_full_msg, full_msg, full_msg_count);
	str_full_msg[full_msg_count] = '\0';
	// printf("str_full_msg: %s\n", str_full_msg);

	// build message
	strncpy(buffers->send_buffer, full_msg, strlen(full_msg));

	// compute length
	buf->send_len = sizeof(full_msg);

	errno = 0;
	int numbytes = send(sockfd, buf->send_buffer, buf->send_len, 0);
	if (numbytes == -1)
	{
		printf("error in sock_esnd_chars() in send() call: %s\n", strerror(errno));
		return -1;
	}

	return 0;
}

/***********
 * calc_stats()
 * 		inputs: *buf = array of values to be processed
 * 				count = number of values in the array
 * 				*ax_stats = struct that will hold stats
 *
 * 		returns: nones\
 */
void calc_stats(float *buf, int count, struct AxisStats *ax_stats)
{

	// calculate mean
	float mean_val = 0, sum = 0;
	for (int i = 0; i < count; i++)
	{
		sum += buf[i];
	}
	mean_val = sum / count;
	ax_stats->mean = mean_val;
	// printf("mean_val = %.4f\n", ax_stats->mean);

	// calculate standard deviation
	float stddev = 0;
	float temp[count];
	float temp_sum = 0;
	for (int i = 0; i < count; i++)
	{
		temp[i] = buf[i] - mean_val;
		temp[i] = temp[i] * temp[i];
		temp_sum += temp[i];
	}
	stddev = temp_sum / (count - 1);
	stddev = sqrtf(stddev);
	ax_stats->stddev = stddev;
	// printf("stddev = %.4f\n", stddev);

	return;
}