#!/usr/bin/env python

# This script will send whatever request body to gpfdist to test
# gpfdist stability.
# Usage: python gpfdist_specialrequest.py "messagebody"


import socket, time, sys

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
if not s:
    raise Exception("socket not created")

s.connect(("localhost", 8088))

print "connected"

msg = """

"""

bytes = s.send(msg)
begin = time.time()

print "Bytes sent %d" % bytes

c = s.recv(5000)
end = time.time()
print "RECEIVE: '%s'" % c
print "TIMEOUT PERIOD: %d" % (end - begin)

