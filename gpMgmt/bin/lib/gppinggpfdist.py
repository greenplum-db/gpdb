#!/usr/bin/env python3

import sys, http.client, getopt, socket

def usage(exitarg):
    print('usage: %s [-q] host:port' % sys.argv[0])
    print() 
    print('   -q: quiet mode')
    print()
    print('e.g. %s localhost:8080' % sys.argv[0])
    print()
    sys.exit(exitarg)

gpfdist = ''
quiet = 0
uri = ''

try:
    (options, args) = getopt.getopt(sys.argv[1:], 'q')
except Exception as e:
    usage('Error: ' + str(e))

for (switch, val) in options:
    if (switch == '-q'):  quiet = 1

if len(args) != 1:
    usage('Error: please specify uri.')

host_port = args[0]

try:
    conn = http.client.HTTPConnection(host_port)
    conn.request('GET', '/')
    r = conn.getresponse()
    gpfdist = r.getheader('X-GPFDIST-VERSION', '')
except socket.error:
    if not quiet:
        print('Error: gpfdist is not running (reason: socket error)')
        print('Exit: 1')
    sys.exit(1)
     
if not gpfdist:
    if not quiet:
        print('Error: gpfdist port is taken by some other programs')
        print('Exit: 2')
    sys.exit(2)

if not quiet:
    print('Okay, gpfdist version "%s" is running on %s.' % (gpfdist, host_port))
sys.exit(0)
