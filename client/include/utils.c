#include "utils.h"


/////////////////////////////
//		Function prototypes
/////////////////////////////

int writer_func(const char *fpath, const char *buf);
void signal_handler ( int signal_number );
char *read_until_term(int fd, const char term, int *rtn_flag);
ssize_t readLine(int fd, void *buffer, size_t n);

/**********************************
*   Globals
**********************************/

bool caught_signal;


/////////////////////////////
//		Function definitions
/////////////////////////////

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