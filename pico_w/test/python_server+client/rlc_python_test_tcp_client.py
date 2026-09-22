#!/usr/bin/python

import socket
import sys
import time
import struct

# queries
Q_IDN = b'IDN?\n'
R_IDN = 'pico w'


Q_STAT = b'STAT?\n'
R_STAT = 'OK'

Q_DATA = b'DATA?\n'

# Check server ip address set
if len(sys.argv) < 2:
    raise RuntimeError('pass IP address of the server')

# Set the server address here like 1.2.3.4
SERVER_ADDR = sys.argv[1]

# These constants should match the server
BUF_SIZE = 2048
SERVER_PORT = 4242

# Open socket to the server
sock = socket.socket()
addr = (SERVER_ADDR, SERVER_PORT)
sock.connect(addr)

###################
# query ID
###################
msg = Q_IDN
write_len = sock.send(msg)
print('wrote %d bytes to server' % write_len)
if write_len != len(msg):
    raise RuntimeError('wrong amount of data written')

time.sleep(5)

# receive response
response_len = 30
buf = sock.recv(response_len)
print('read %d bytes from client' % len(buf))

# display and handle the message
buf_str = buf.decode('utf-8').strip()
print('Received message: %s' % buf_str )

# check for a correct value here
if buf_str != R_IDN:
    print('Unexpected response received')

###################
# query data
###################
msg = Q_DATA
write_len = sock.send(msg)
print('wrote %d bytes to server' % write_len)
if write_len != len(msg):
    raise RuntimeError('wrong amount of data written')

time.sleep(5)

# receive response
response_len = 5
buf = sock.recv(response_len)
print('read %d bytes from client' % len(buf))

# display and handle the message
#buf_str = buf.decode('utf-8').strip()
buf_str = buf.decode()
print('Received message: %s' % buf_str )




###################
# loop
###################

while True:
    # loop infinitely
    time.sleep(1)


# All done
sock.close()
print("test completed")
