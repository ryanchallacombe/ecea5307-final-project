#include "utils.h"

/////////////////////////////
//		Function prototypes
/////////////////////////////

int writer_func(const char *fpath, const char *buf);
void signal_handler ( int signal_number );
char *read_until_term(int fd, const char term, int *rtn_flag);
ssize_t readLine(int fd, void *buffer, size_t n);
int exec_textbelt_call(char *script_fullpath, char *msg, char *phonenumber);
int sock_recv_chars(int fd, struct Buffers *buf);
int sock_recv_data(int fd, struct Buffers *buf);
int sock_send_chars(int fd, struct Buffers *buf, char *msg);
int sock_send_data_req(int fd, struct Buffers *buf, char *msg, int axis, int count);
void calc_stats(float *buf, int count, struct AxisStats *ax_stats);
int try_server_connect(char *server_ip, char *tcp_port, int timeout_ms);
void build_ipv4_addr_array(char *pfx, int suf_start, int suf_end, char target_ip[][IP_ADDR_CHAR_COUNT+1]);
int update_axis_vibe_stats(struct AxisStats *axis_stats, int samples, int fd, struct Buffers *buf);

/**********************************
*   Globals
**********************************/

bool caught_signal;
int ip_addr_char_count = IP_ADDR_CHAR_COUNT;
extern char *Q_DATA;
extern float OOB_ths;

/////////////////////////////
//		Function definitions
/////////////////////////////

/***********
    update_axis_vibe_stats()
    inputs: 
        *axis_stats = ptr to struct AxisStats which will be updated
        samples = number of samples to get from the pico
        fd = socket file descriptor
        *buf = ptr to struct Buffers which are used to buffer data from and to the pico

    returns:
            1 = stddev of vibration measurements exceeds threshold
            0 = stddev of vibration measurements is below threshold
            -1 = error / function failure

    update_axis_vibe_stats()

    returns:
            0 = function successfully update stats for axis
            -1 = error / function failure
*/
int update_axis_vibe_stats(struct AxisStats *axis_stats, int samples, int fd, struct Buffers *buf) 
{   
    bool OOB_flag = false;
    float recv_values[samples];
    int rtn_val = 0;

    do
    {
        for (int i = 0; i < samples; i++)
        {
            OOB_flag = false;
            // query for data
            if ((rtn_val = sock_send_data_req(fd, buf, Q_DATA, axis_stats->axis, 1)) == -1)
            {
                return rtn_val;
            }

            // recieve
            if ((rtn_val = sock_recv_data(fd, buf)) == -1)
            {
                return rtn_val;
            }
            // printf("loop i = %i, received %i bytes\n", i, buffers->data_bytes);
            recv_values[i] = *buf->data_buffer;
            // printf("Recieved: %.4f\n", recv_values[i]);
            if (recv_values[i] > OOB_ths)
            {
                printf("\n***OOB value received: %.4fg\n\n", recv_values[i]);
                OOB_flag = true;
                break;
            }
        }
    } while (OOB_flag);

    calc_stats(recv_values, samples, axis_stats);

    return rtn_val;
}


/***********
 * try_server_connect())
 *      inputs: server_ip = ip address to try (char)
 *              tcp_port = port to try
 *              timeout_ms = poll timeout
 *
 *      returns:
 *              -1  = failure to connect, i.e. poll timed out or error occurred
 *              else = socket fd
 */
int try_server_connect(char *server_ip, char *tcp_port, int timeout_ms) {
    // setup
    struct addrinfo hints;
    struct addrinfo *servinfo; // will point to the linked list of results
    int ret, sockfd, optval = 1;

    // hints setup
    memset(&hints, 0, sizeof(hints)); // make sure that the struct is cleared
    hints.ai_family = AF_UNSPEC;      // don't care if ipv4 or ipv6
    hints.ai_socktype = SOCK_STREAM;  // TCP stream sockets
    hints.ai_flags = AI_PASSIVE;      // fill in my localhost IP for me

    // poll setup
    struct pollfd pfds[1];

    // getaddrinfo call
    if ((ret = getaddrinfo(server_ip, tcp_port, &hints, &servinfo)) != 0)
    {
        // log error
        printf("getaddrinfo() call error: %s\n", gai_strerror(ret));
        return -1;
    }

    // socket call
    if ((sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol)) == -1)
    {
        printf("socket() call error: %s\n", strerror(errno));
        return -1;
    }

    // set SO_REUSEADDR on a socket to true (1):
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) != 0)
    {
        perror("setsockopt");
        return -1;
    }

    // set socket to non-blocking
    int flags = fcntl(sockfd, F_GETFL);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

    // connect() call
    ret = connect(sockfd, servinfo->ai_addr, servinfo->ai_addrlen);
    if ( ret == 0 ){
        // connected immediately
        return sockfd;      
    }
    freeaddrinfo(servinfo);

    // poll
    pfds[0].fd = sockfd;
    pfds[0].events = POLLOUT;
    pfds[0].revents = 0;
    int num_events = poll(pfds, 1, timeout_ms);
    if (num_events == 0) {
        // poll timed out
        close(sockfd);
        return -1;
    } else if ( pfds[0].revents & POLLOUT ){
    } else {
        // unexpected poll() event occurred
        close(sockfd);
        return -1;
    }

    return sockfd;
}

/***********
 * build_ipv4_addr_array()
 *      inputs: 
                char *pfx = 11 digit ip /24 prefix (char array)
                    must include period at the end, i.e. "192.168.86."
                int suf_start = suffix (last octet of ip addr) to START with (lowest)
                int suf_end =  suffix (last octet of ip addr) to END with (lowest)
                char target_ip[][IP_ADDR_CHAR_COUNT+1]
 *                  pointer to array of null terminated ip address strings
 * 
 *                  e.g. pfx = "192.168.86.", suf_start = 25, suf_end = 30
                    output is a pointer to array with range of ip addresses
                    starting at "192.168.86.25" and ending with "192.168.86.30"         
 ****************/
void build_ipv4_addr_array(char *pfx, int suf_start, int suf_end, char target_ip[][IP_ADDR_CHAR_COUNT+1]) {

    int pfx_count = 11;

    int idx_str_count = 4;
    char idx_str[idx_str_count];
    for (int i = suf_start; i <= suf_end; i++) {
        
        if (i<10) {
            idx_str[0] = i + '0';
            idx_str[1] = '\0';
        } else if (i<100) {
            idx_str[0] = i/10 + '0';
            idx_str[1] = (i % 10) + '0';
            idx_str[2] = '\0';
        } else {
            idx_str[0] = (i / 100) + '0';
            idx_str[1] = (i / 10) % 10 + '0';
            idx_str[2] = (i % 10) + '0';
            idx_str[3] = '\0';
        }

        strncpy(target_ip[i - suf_start], pfx, pfx_count);
        strncpy(target_ip[i - suf_start] + pfx_count, idx_str, idx_str_count);
        //printf("target_ip[i - suf_start]: %s\n", target_ip[i - suf_start]);
    }

}

/***********
 * sock_recv_chars()
 *      inputs: fd = the socket fd
 *              *buf = ptr to struct Buffers
 *
 *      returns:
 *              0   = successful receive
 *              -1  = error in recv
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
 *      inputs: fd = the socket fd
 *              *buf = ptr to struct Buffers
 *
 *      returns:
 *              0   = successful receive
 *              -1  = error in recv
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
 *      inputs: fd = the socket fd
 *              *buf = ptr to struct Buffers
 *              * msg = ptr to message to send
 *
 *      returns:
 *              0   = successful send
 *              -1  = error in recv
 */
int sock_send_chars(int fd, struct Buffers *buf, char *msg)
{

    int sockfd = fd;
    // clear the receive buffer
    memset(buf->send_buffer, 0, SEND_BUF_SIZE);
    strncpy(buf->send_buffer, msg, strlen(msg));
    buf->send_len = sizeof(msg);

    errno = 0;
    int numbytes = send(sockfd, buf->send_buffer, buf->send_len, 0);
    if (numbytes == -1)
    {
        printf("error in sock_send_chars() in send() call: %s\n", strerror(errno));
        return -1;
    }

    return 0;
}

/***********
 * sock_send_data_req())
 *      inputs: fd = the socket fd
 *              *buf = ptr to struct Buffers
 *              * msg = ptr to message to send
 *              axis = the axis to sample (x=1, y=2, z=3)
 *              count = number of samples
 *
 *      returns:
 *              0   = successful send
 *              -1  = error in recv
 */
int sock_send_data_req(int fd, struct Buffers *buf, char *msg, int axis, int count)
{

    int sockfd = fd;
    // clear the send buffer
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
    strncpy(buf->send_buffer, full_msg, strlen(full_msg));

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
 *      inputs: *buf = array of values to be processed
 *              count = number of values in the array
 *              *ax_stats = struct that will hold stats
 *
 *      returns: nones\
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

int writer_func(const char *fpath, const char *buf)
{	
	// ASSUME SYSLOG IS OPEN

	syslog( LOG_DEBUG, "*** Starting writer_func ***\n" );
	
	int fd;
	ssize_t bytes_written;
	int buf_bytes = strlen(buf);
	
	/* Open file and error check */
	fd = open( fpath, O_RDWR | O_APPEND);
	if ( fd == -1 ) {		
		// Create file
		syslog( LOG_ERR, "Creating file %s.\n", fpath);
		errno = 0;
		fd = creat( fpath, S_IRWXU | S_IRWXG | S_IRWXO );
		
		// Check for errors
		if ( (errno != 0) || (fd == -1) ) {
			char *error_str = strerror(errno);
			syslog( LOG_ERR, "ERRNO string: %s\n", error_str );
			syslog( LOG_ERR, "Error creating file %s.\n", fpath );
			syslog( LOG_ERR, "Returning with status = 1.\n" );
			return 1;
		}
	}
	
	/* Write to file and error check */
	syslog( LOG_DEBUG, "Writing %s to %s\n", buf, fpath );
	bytes_written = write( fd, buf, buf_bytes );
	if ( bytes_written != buf_bytes ) {
		syslog( LOG_ERR, "Error writing to file %s.\n", fpath);
		syslog( LOG_ERR, "Number of bytes in buffer = %i\n", buf_bytes);
		syslog( LOG_ERR, "Bytes written (returned from write()) = %li\n", bytes_written);
		syslog( LOG_ERR, "Returning from writer_func with status = -1.\n" );
		return -1;
	}
	
	close( fd );
	
	syslog( LOG_DEBUG, "*** Returning from writer_func with status = 0 ***\n" );
	return 0;
}
// TODO
// Gracefully exits when SIGINT or SIGTERM is received, completing any open connection operations, 
// closing any open sockets, and deleting the file /var/tmp/aesdsocketdata.
// Logs message to the syslog “Caught signal, exiting” when SIGINT or SIGTERM is received.

void signal_handler ( int signal_number )
{
    //
    // Save a copy of errno so we can restore it later.  See https://pubs.opengroup.org/onlinepubs/9699919799/
    //
    int errno_saved = errno;

    if ( (signal_number == SIGINT) || (signal_number == SIGTERM) )
        caught_signal = true;

    errno = errno_saved;
}




// read_until_term()
// 
// Reads from a file descriptor until EOF or the specified terminal character is found
//
// Dynamically allocates and reallocates buffer memory until EOF or the specified terminal character is found
// Returns a pointer to the buffer
// Returns NULL on error or no characters read
//
// It's up to the caller to free() this pointer when done with it.
//
// 	Notes:
// 	1. The specified terminal character *is* included in the returned buffer
//	2. When the caller passes in a null character '\0' the function will read until EOF
//  3. Return flags can be used to determine exactly when happened after the function returns
// 
// Credits/references: The below were used as reference material in developing this function	
//						beej's 'readline' function: https://beej.us/guide/bgc/html/split/manual-memory-allocation.html
//						Michael Kerrisk's 'readLine' function: https://man7.org/tlpi/code/online/dist/sockets/read_line.c.html
//
//  Return flags
//  0 = terminal character found
//  1 = malloc failed
//  2 = realloc failed
//  3 = read() error
//  4 = reached EOF and no bytes were read
//  5 = reached EOF and some bytes were read


char *read_until_term(int fd, const char term, int *rtn_flag)
{

    ssize_t num_read;      	// # of bytes fetched by last read()
    size_t tot_read = 0;       	// Total bytes read so far
    int offset = 0;   		// Index of next char that goes in the buffer
    int bufsize = INITIAL_BUFFER_SIZE;  
    char *buf;        		
    char c;            		// The character we've read in

    buf = malloc(bufsize);  // Allocate initial buffer

    if (buf == NULL) {   		// Error check
        printf("malloc failed\n");
        *rtn_flag = 1; 
        return NULL;
    }

    do {
    	// Check if we're out of room in the buffer accounting
        // for the extra byte for the NUL terminator
    	if (offset == bufsize -1) {
    		bufsize = 2 * bufsize;			// double the size

    		char *new_buf = realloc(buf, bufsize);

    		 if (new_buf == NULL) {
                free(buf);   // On error, free and bail
                printf("realloc failed\n");
                *rtn_flag = 2; 
                return NULL;
            }

            buf = new_buf;  // Successful realloc

    	}

    	// read a single character 
    	errno = 0;
    	num_read = read(fd, &c, 1);
        // printf("read returned: %d\n", (uint)num_read);
    	if ( num_read == -1) {
    		if (errno == EINTR)         // Interrupted --> restart read()
                continue;
            else {
            	free(buf);
                *rtn_flag = 3; 
                return NULL;              // Some other error 
            }
    	}
    	else if (num_read == 0)	{		// EOF
    		if (tot_read == 0) {		// no bytes read, return null
    			free(buf);
                *rtn_flag = 4; 
                return NULL;
    		}
    		else						// some bytes have been read, break loop and add '\0'
                *rtn_flag = 5; 
    			break;	
    	}		
 
    	buf[offset] = c;  // Add the byte onto the buffer
    	//printf("buf[offset]: %c\n", buf[offset] );
    	offset++;
    	//printf("new offset: %d\n", offset);
    	tot_read++;
        *rtn_flag = 0; 

    }  while ( buf[offset-1] != term );

    // We hit newline or EOF...

    //printf("Total chars read: %d\n", (int)tot_read);

    // Shrink to fit
    if (offset < bufsize - 1) {  // If we're short of the end
        char *new_buf = realloc(buf, offset + 1); // +1 for NUL terminator

        // If successful, point buf to new_buf;
        // otherwise we'll just leave buf where it is
        if (new_buf != NULL)
            buf = new_buf;
    }

    // Add the NUL terminator
    buf[offset] = '\0';

    return buf;
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