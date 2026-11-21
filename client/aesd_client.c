/*
** aesd_client.c
**
** Advanced Embedded Linux Programming: Final Project
**
**		writer: ryan challacombe
**		date:	9/22/2026
*/

/*
** Usage: >> aesd_client <optional -d for daemon mode>
*/

#include "include/utils.h"

/**********************************
 *	Defines
 **********************************/
#define SEND_TEXTBELT_REQ	0 		// set to 0 if debugging and don't want to send texts

/**********************************
 *	Globals
 **********************************/
extern int ip_addr_char_count;
extern bool caught_signal; // defined in utils.c
char *Q_IDN = "IDN?";
char *R_IDN = "pico w"; // expected response to query
char *Q_DATA = "DATA?";
size_t numbytes;
int g_passive_mon_sleep_secs = 10; // while loop sleep time for when machine is OFF
int g_active_mon_sleep_secs = 10;  // while loop sleep time for when machine is ON
bool D_RUN = false;				   // 'dryer running' flag

// textbelt / messaging
char *textbelt_script_fullpath = "/home/ryan/projects/ecea5307-final-project/textbelt/tb_request.sh";
char *phonenumber = "9137355704";
char *dryer_start_msg = "Dryer started ";
char *dryer_stop_msg = "Dryer stopped ";
char *now_str;
char text_msg[100];		

// buffers for send and receive
struct Buffers buf = {0};
struct Buffers *buffers = &buf;

// setup baseline stats and thresholds
// alternatively could collect data with system at rest and compute / assign these as part of the application
float baseline_z_mean = 1.035;
float baseline_z_stddev = 0.02;
float baseline_stddev_ths = 0.06;			// 3 * baseline_z_stddev;
float OOB_ths = 2.5;					// baseline_z_mean + 100 * baseline_z_stddev;

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

	/************************* Signal handler setup *************************/
	caught_signal = false; // Signal handler flag
	struct sigaction new_action;

	memset(&new_action, 0, sizeof(struct sigaction));
	new_action.sa_handler = signal_handler;

	if (sigaction(SIGTERM, &new_action, NULL) != 0)
	{
		printf("Error %d (%s) registering for SIGTERM", errno, strerror(errno));
		return -1;
	}
	if (sigaction(SIGINT, &new_action, NULL))
	{
		printf("Error %d (%s) registering for SIGINT", errno, strerror(errno));
		return -1;
	}

	/********** Daemon mode **********/

	// Daemon mode is invoked by argument 2 as '-d' when this application is invoked from command line
	pid_t pid;
	if (argc > 1) { // check to ensure we got a command line argument
		if (!strncmp("-d", argv[1], 2))
		{
			printf("Daemon mode invoked.\n");

			pid = fork();
			if (pid == -1)
			{ // fork error
				perror("fork");
				return -1;
			}
			else if (pid != 0)
			{ // pid = 0 is child pid

				// exit parent process
				printf("Exiting parent process\n");
				return 0;
			}
		}
	}	

	/************************* Build list of addresses *************************/
	char *slash_24 = IP_PREFIX;
	int suf_start = IP_SUFFIX_START;
	int suf_end = IP_SUFFIX_END;
	char target_ip[suf_end - suf_start + 1][ip_addr_char_count+1];			// include room for null terminator

	build_ipv4_addr_array(slash_24, suf_start, suf_end, target_ip);

	/************************* Socket *************************/
	int poll_timeout_ms = 1000;
	bool connection_accepted = false;
	int loop_count = 0;
	int sockfd;

	do {
		printf("attempting connection to %s on port %s\n", target_ip[loop_count], TCP_PORT);

		if ( (sockfd = try_server_connect(target_ip[loop_count], TCP_PORT, poll_timeout_ms)) != -1) {
			printf("*** IP addr %s accepted connection on port %s ***\n", target_ip[loop_count], TCP_PORT);	
			connection_accepted = true;
		}

		loop_count++;

	} while ( (!connection_accepted) & ( loop_count <= (suf_end - suf_start) ));

	sleep(2);

	if ( !connection_accepted ) {
		printf("failed to connect to any of the ip address / tcp ports in the list\n");
	} else {
		printf("connected. sockfd = %i\n", sockfd);
	}

	// set socket back to blocking
    int flags = fcntl(sockfd, F_GETFL);
    fcntl(sockfd, F_SETFL, flags & ~O_NONBLOCK);

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

	printf("Recieved from server: %s\n", buffers->recv_buffer);

	/**************** SET INITIAL VIBE THRESHOLD FOR SYSTEM ****************/

	// setup axis stats for baseline here
	// todo - do i even need this base_z_stats??
	struct AxisStats base_z_stats = {0};
	struct AxisStats *base_z_axis = &base_z_stats;
	//int axis = 3;		// z-axis

	// assign hardcoded vibration stats based on experimental data
	base_z_axis->mean = baseline_z_mean;
	base_z_axis->stddev = baseline_z_stddev;
	syslog(LOG_DEBUG, "baseline mean_val = %.4fg\n", base_z_axis->mean);
	syslog(LOG_DEBUG, "baseline stddev = %.4fg\n", base_z_axis->stddev);
	syslog(LOG_DEBUG, "stddev threshold =  %.4fg\n", baseline_stddev_ths);
	/* establish an out-of-bounds threshold to use for filtering out extreme events */
	/* This filters out the closing of the dryer door for example */
	syslog(LOG_DEBUG, "OOB stddev threshold set to: %.4fg\n", OOB_ths);
	syslog(LOG_DEBUG, "*** Baseline setup completed ****\n\n\n");

	/**************** LOOP  ****************/

	// setup axis stats for loop here
	struct AxisStats z_stats = {0};
	struct AxisStats *z_axis = &z_stats;
	z_axis->axis = 3;


	// setup vars for loop

	//float recv_values[LOOP_REQ_COUNT]; // buffer for storing received vibe data
	int loop_sleep_secs = g_passive_mon_sleep_secs;
	//bool OOB_flag = false;

	while (!caught_signal)
	{
		/*******
		 * Get data and process stats
		 * check against baseline stddev threshold
		 */
		printf("****************\n");
		printf("Querying for %i samples on axis %i\n", LOOP_REQ_COUNT, z_axis->axis);
		syslog(LOG_DEBUG, "****************\n");
		syslog(LOG_DEBUG, "Querying for %i samples on axis %i\n", LOOP_REQ_COUNT, z_axis->axis);
		now = time(NULL);
		syslog(LOG_DEBUG, "***** Current time: %s\n", ctime(&now));
		/*
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
		*/

		if (update_axis_vibe_stats(z_axis, LOOP_REQ_COUNT, sockfd, buffers) != 0) {
			printf("error in update_axis_vibe_stats()\n");
			rtn_val = -1;
			goto DONE;
		}
		// calc_stats(recv_values, LOOP_REQ_COUNT, z_axis);
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
				// send textbelt call here ... dryer started
				now = time(NULL);
				now_str = ctime(&now);
				strncpy(text_msg, dryer_start_msg, strlen(dryer_start_msg));
				strncpy(text_msg + strlen(dryer_start_msg), now_str, strlen(now_str));
				if (SEND_TEXTBELT_REQ) {
					exec_textbelt_call(textbelt_script_fullpath, text_msg, phonenumber);	
				}
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
				printf("***************** Dryer stopped\n");
				syslog(LOG_DEBUG, "Dryer stopped\n");
				now = time(NULL);
				syslog(LOG_DEBUG, "***** Current time: %s\n\n\n", ctime(&now));
				loop_sleep_secs = g_passive_mon_sleep_secs;
				// send textbelt call here ... dryer done
				now = time(NULL);
				now_str = ctime(&now);
				strncpy(text_msg, dryer_stop_msg, strlen(dryer_stop_msg));
				strncpy(text_msg + strlen(dryer_stop_msg), now_str, strlen(now_str));
				if (SEND_TEXTBELT_REQ) {
					exec_textbelt_call(textbelt_script_fullpath, text_msg, phonenumber);
				}
			}
		}

		/**************** SLEEP ****************/
		sleep(loop_sleep_secs);

	} // while

	/**************** Cleanup and Exit ****************/

DONE:

	close(sockfd);

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
