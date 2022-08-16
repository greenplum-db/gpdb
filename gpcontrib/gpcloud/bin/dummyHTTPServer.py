#!/usr/bin/env python

"""
Very simple HTTP server in python.
Usage::
    ./dummyHTTPServer.py [<port>]
Send a GET request::
    curl http://localhost
Send a HEAD request::
    curl -I http://localhost
Send a POST request::
    curl -d "foo=bar&bin=baz" http://localhost
"""

from BaseHTTPServer import BaseHTTPRequestHandler, HTTPServer
import SocketServer
import getopt
import sys
import os
import ssl

help_msg = '''./dummyHTTPServer.py 
[-h] | [-p (--port=) <port>][-f (--filename=) <filename>][-s][-t(--type=)<S|PARAM_S>]
Options:
    -h : print this help info
    -p --port=: http listen port
    -f --filename=: content filename path
    -t --type=: Parameter_Server server or Common_Server
    -s: use ssl connection
    '''
filename = "data/s3httptest.conf"
class S(BaseHTTPRequestHandler):

    def _set_headers(self,length = 0):
        self.send_response(200)
        self.send_header('Content-type', 'text/plain')
        self.send_header('Content-Length', length)
        self.end_headers()

    def do_GET(self):
        self._set_headers(11)
        self.wfile.write('Pong to GET')

    def do_HEAD(self):
        self._set_headers()

    def do_PUT(self):
        # Just bounce the request back
        print "----- SOMETHING WAS PUT ------"
        print self.headers
        length = int(self.headers['Content-Length'])
        content = self.rfile.read(length)
        self._set_headers(length)
        self.wfile.write(content)

    def do_POST(self):
        # Just bounce the request back
        print "----- SOMETHING WAS POST ------"
        print self.headers
        length = int(self.headers['Content-Length'])
        content = self.rfile.read(length)
        self._set_headers(length)
        self.wfile.write(content)

    def do_DELETE(self):
        # Just bounce the request back
        print "----- SOMETHING WAS DELETED ------"
        print self.headers
        length = int(self.headers['Content-Length'])
        # content = self.rfile.read(length)
        self._set_headers(length)
        self.wfile.write("")

class PARAM_S(BaseHTTPRequestHandler):
    def _set_headers(self,length = 0):
        self.send_response(200)
        self.send_header('Content-type', 'text/plain')
        self.send_header('Content-Length', length)
        self.end_headers()

    def _getcontent(self):
        content = ""
        if filename != "":
            try:
                with open(filename, 'r') as f:
                    content = f.read()
            except Exception:
                print "Can not open file:%s" % filename
        return content

    def do_GET(self):
        try:
            if self.headers['S3_Param_Req']:
                content = self._getcontent()
                self._set_headers(len(content))
                self.wfile.write(content)
            else:
                self._set_headers(0)
        except KeyError:
            print "Header missing S3_Param_Req"
            self._set_headers(0)

def run(server_class=HTTPServer, handler_class=S, port=8553, https=False):
    server_address = ('', port)
    handler_class.protocol_version = 'HTTP/1.1'
    httpd = server_class(server_address, handler_class)
    if https:
        datadir = os.getcwd()
        curdir = "./"
        while(curdir != "gpdb_src" and curdir != "gpdb"):
            datadir, curdir = os.path.split(datadir)
        datadir = os.path.join(datadir, curdir)
        datadir = os.path.join(datadir, "gpAux/gpdemo/datadirs/qddir/demoDataDir-1/")
        datadir += "/gpfdists/"
        keyfile = datadir + "server.key"
        certfile = datadir + "server.crt"
        httpd.socket = ssl.wrap_socket (httpd.socket,
            keyfile=keyfile,
            certfile=certfile, server_side=True)
    print 'Starting http server...'
    httpd.serve_forever()

if __name__ == "__main__":
    from sys import argv
    port = 8553
    servertype = "S"
    use_ssl = False
    try:
        opts, args = getopt.getopt(argv[1:],"hsp:f:t:",["--port=","--filename=", "--type="])
    except getopt.GetoptError:
        print help_msg
        sys.exit(2)
    for opt, arg in opts:
        if opt == '-h':
            print help_msg
            sys.exit(0)
        elif opt == '-s':
            use_ssl = True
        elif opt in ("-p", "--port"):
            port = int(arg)
        elif opt in ("-f", "--filename"):
            filename = arg
        elif opt in ("-t", "--type"):
            servertype = arg
    if servertype == "Common_Server":
        run(handler_class=S, port=port)
    else:
        run(handler_class=PARAM_S, port=port, https=use_ssl)

