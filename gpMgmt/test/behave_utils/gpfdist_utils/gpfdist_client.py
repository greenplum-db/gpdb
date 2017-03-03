#!/usr/bin/env python

import socket, time

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
if not s:
    raise Exception("socket not created")

s.connect(("localhost", 8088))

print "connected"

msg = """GET /data.txt HTTP/1.1
Host: localhost"""

bytes = s.send(msg)
begin = time.time()

print "Bytes sent %d" % bytes

c = s.recv(5000)
end = time.time()
print "RECEIVE: '%s'" % c 
print "TIMEOUT PERIOD: %d" % (end - begin)

