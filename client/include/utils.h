#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <poll.h>
#include <math.h>
#include <time.h>

#ifndef UTILS_H
#define	UTILS_H

#define INITIAL_BUFFER_SIZE		128
#define RCV_BUF_SIZE 		1024
#define SEND_BUF_SIZE 		512
#define DATA_BUF_SIZE 		256
#define IP_ADDR_CHAR_COUNT	14	
#define TCP_PORT 			"4242"
#define IP_PREFIX 			"192.168.86."
#define IP_SUFFIX_START		25
#define IP_SUFFIX_END		35
#define BASELINE_REQ_COUNT 	100
#define REQ_COUNT_10 		10
#define REQ_COUNT_25 		25
#define REQ_COUNT_50 		50

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

struct AxisStats
{
	int axis; 		// per pico interface: x = 1, y = 2, z = 3
	float mean;
	float stddev;
};

void signal_handler ( int signal_number );
int exec_textbelt_call(char *script_fullpath, char *msg, char *phonenumber);
int sock_recv_chars(int fd, struct Buffers *buf);
int sock_recv_data(int fd, struct Buffers *buf);
int sock_send_chars(int fd, struct Buffers *buf, char *msg);
int sock_send_data_req(int fd, struct Buffers *buf, char *msg, int axis, int count);
void calc_stats(float *buf, int count, struct AxisStats *ax_stats);
int try_server_connect(char *server_ip, char *tcp_port, int timeout_ms);
void build_ipv4_addr_array(char *pfx, int suf_start, int suf_end, char target_ip[][IP_ADDR_CHAR_COUNT+1]);
int update_axis_vibe_stats(struct AxisStats *axis_stats, int samples, int fd, struct Buffers *buf);


#endif