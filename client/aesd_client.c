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
// set to 0 if debugging and don't want to send texts
#define SEND_TEXTBELT_REQ				1			
// how many samples to request from the pico	
#define LOOP_REQ_COUNT 					REQ_COUNT_10
// confirm (debounce) state changes this many times
#define STATE_CHANGE_DEBOUNCE_COUNT		3

// textbelt bash script
#ifndef TB_SCRIPT_PATH
#define TB_SCRIPT_PATH				"/aesd_client_include/tb_request.sh"
#endif

// subscriber file
#ifndef SUBSCRIBER_FILE_PATH
#define SUBSCRIBER_FILE_PATH     	"/aesd_client_include/subscribers.conf"
#endif
#define MAX_SUBSCRIBER_CT   		5
#define PHONE_NUMBER_CHAR_CT   		11

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
int g_debounce_sleep_secs = 2;
bool D_RUN = false;				   // 'dryer running' flag

// textbelt / messaging
char *textbelt_script_fullpath = TB_SCRIPT_PATH;
char *phonenumber = "9137355704";
char *dryer_start_msg = "Dryer started ";
char *dryer_stop_msg = "Dryer stopped ";
char *now_str;
char text_msg[100];
char *subscriber_file_path = SUBSCRIBER_FILE_PATH;

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
	printf("***** Current time: %s\n\n", ctime(&now));

	// show textbelt script path
	printf("textbelt_script_fullpath: %s\n\n", textbelt_script_fullpath);
	syslog(LOG_DEBUG, "textbelt_script_fullpath: %s\n\n", textbelt_script_fullpath);

	// show subscriber file path
	printf("subscriber_file_path: %s\n\n", subscriber_file_path);

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

	/************************* Get list of subscriber phone numbers *************************/
    int fd = open(subscriber_file_path, O_RDONLY, 0644);
    if (fd == -1) {
        perror("Error opening file");
        return 1;
    }

    // setup buffer for reading the subscriber phone numbers
    //int max_subscriber_ct = MAX_SUBSCRIBER_CT;
    //int phone_number_char = PHONE_NUMBER_CHAR_CT;
    //char subscriber_phone_numbers[max_subscriber_ct][phone_number_char];
    char subscriber_phone_numbers[MAX_SUBSCRIBER_CT][PHONE_NUMBER_CHAR_CT];
    int num_read = 0;
    int subscriber_count = 0;
    for (subscriber_count = 0; subscriber_count < MAX_SUBSCRIBER_CT; subscriber_count++) {
        if ( (num_read = readLine(fd, subscriber_phone_numbers[subscriber_count], PHONE_NUMBER_CHAR_CT)) == 0) {
            break;
        } 
        printf("subscriber_phone_numbers[subscriber_count]: %s\n", subscriber_phone_numbers[subscriber_count]);
    } 
    printf("subscriber_count: %i\n", subscriber_count);
    close(fd);

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
		rtn_val = -1;
		goto DONE;
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

	// report hardcoded vibration stats based on experimental data
	syslog(LOG_DEBUG, "baseline mean_val = %.4fg\n", baseline_z_mean);
	syslog(LOG_DEBUG, "baseline stddev = %.4fg\n", baseline_z_stddev);
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
	int loop_sleep_secs = g_passive_mon_sleep_secs;

	while (!caught_signal)
	{

		/**************** UPDATE VIBE STATS  ****************/
		int debounce_counter = 0;
		do 
		{
			printf("****************\n");
			printf("Querying for %i samples on axis %i\n", LOOP_REQ_COUNT, z_axis->axis);
			printf("debounce_counter = %i\n", debounce_counter);
			syslog(LOG_DEBUG, "****************\n");
			syslog(LOG_DEBUG, "Querying for %i samples on axis %i\n", LOOP_REQ_COUNT, z_axis->axis);
			syslog(LOG_DEBUG, "debounce_counter = %i\n", debounce_counter);
			now = time(NULL);
			syslog(LOG_DEBUG, "***** Current time: %s\n", ctime(&now));

			if (update_axis_vibe_stats(z_axis, LOOP_REQ_COUNT, sockfd, buffers) != 0) {
				printf("error in update_axis_vibe_stats()\n");
				rtn_val = -1;
				goto DONE;
			}

			// report values 
			printf("mean_val = %.4f\n", z_axis->mean);
			printf("stddev = %.4f\n", z_axis->stddev);
			syslog(LOG_DEBUG, "mean_val = %.4f\n", z_axis->mean);
			syslog(LOG_DEBUG, "stddev = %.4f\n", z_axis->stddev);

			// if running, but data indicates it is not
			if ( (D_RUN) & (z_stats.stddev <= baseline_stddev_ths) ) {
				debounce_counter++;
				sleep(g_debounce_sleep_secs);
			} else
			// if not running, but data inidicates it is running
			if ( (!D_RUN) & (z_stats.stddev > baseline_stddev_ths) ) {
				printf("**** stddev %.4fg exceeds threshold %.4fg ****\n\n", z_axis->stddev, baseline_stddev_ths);
				debounce_counter++;
				sleep(g_debounce_sleep_secs);
			} else { 	
				// data does not indicate a state change
				debounce_counter = 0;
			}

			

		} while ((debounce_counter > 0) & (debounce_counter < STATE_CHANGE_DEBOUNCE_COUNT));

		// Change state if needed
		if (debounce_counter == STATE_CHANGE_DEBOUNCE_COUNT) {
			
			if (D_RUN) {		// running but state needs to change to not running
				D_RUN = false;
				loop_sleep_secs = g_passive_mon_sleep_secs;

				// send textbelt call here ... dryer done
				now = time(NULL);
				now_str = ctime(&now);
				strncpy(text_msg, dryer_stop_msg, strlen(dryer_stop_msg));
				strncpy(text_msg + strlen(dryer_stop_msg), now_str, strlen(now_str));
				if (SEND_TEXTBELT_REQ) {
					for(int i = 0; i < subscriber_count; i++) {
						exec_textbelt_call(textbelt_script_fullpath, text_msg, subscriber_phone_numbers[i]);	
					}
				}

				printf("***************** Dryer stopped\n");
				syslog(LOG_DEBUG, "***************** Dryer stopped\n");
				now = time(NULL);
				syslog(LOG_DEBUG, "***** Current time: %s\n\n\n", ctime(&now));	

			} else {			// not running but state needs to change to running	
				D_RUN = true;
				loop_sleep_secs = g_active_mon_sleep_secs;

				// send textbelt call here ... dryer started
				now = time(NULL);
				now_str = ctime(&now);
				strncpy(text_msg, dryer_start_msg, strlen(dryer_start_msg));
				strncpy(text_msg + strlen(dryer_start_msg), now_str, strlen(now_str));
				if (SEND_TEXTBELT_REQ) {
					for(int i = 0; i < subscriber_count; i++) {
						exec_textbelt_call(textbelt_script_fullpath, text_msg, subscriber_phone_numbers[i]);	
					}	
				}

				printf("******* Dryer started\n");
				syslog(LOG_DEBUG, "***************** Dryer started\n");
				now = time(NULL);
				syslog(LOG_DEBUG, "***** Current time: %s\n\n\n", ctime(&now));	
			}

		} else {
			// not a stand change 
			if (D_RUN) {
				printf("Dryer is still running\n");
				syslog(LOG_DEBUG, "Dryer is still running\n");
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
