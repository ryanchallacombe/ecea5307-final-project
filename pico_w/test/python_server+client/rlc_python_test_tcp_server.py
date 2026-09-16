#!/usr/bin/python

import random
import socket


def message_handler(msg):
    # print('message_handler msg = %s' % msg)
    match msg:
        case "IDN?":
            return b"surrogate for pico w"
        case _:
            return "unknown message"
    

# Set server address to machines IP
SERVER_ADDR = "192.168.86.32"

# These constants should match the client
BUF_SIZE = 2048
TEST_ITERATIONS = 10
SERVER_PORT = 4242

# Open socket to the server
sock = socket.socket()
sock.bind((SERVER_ADDR, SERVER_PORT))
sock.listen(1)
print("server listening on", SERVER_ADDR, SERVER_PORT)

# Wait for the client
con = None
con, addr = sock.accept()
print("client connected from", addr)

# receive first query from client
query_size = 6
buf = con.recv(query_size)
print('read %d bytes from client' % len(buf))

# display and handle the message
buf_str = buf.decode('utf-8').strip()
print('Received message: %s' % buf_str )
response = message_handler(buf_str)
print( 'message_handler returns: %s' %  response)

# respond
con.send(response)

# All done
con.close()
sock.close()
print("test completed")
