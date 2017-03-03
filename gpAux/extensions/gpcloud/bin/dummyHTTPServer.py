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

def run(server_class=HTTPServer, handler_class=S, port=8553):
	server_address = ('', port)
	handler_class.protocol_version = 'HTTP/1.1'
	httpd = server_class(server_address, handler_class)
	print 'Starting http server...'
	httpd.serve_forever()

if __name__ == "__main__":
    from sys import argv

    if len(argv) == 2:
        run(port=int(argv[1]))
    else:
        run()
