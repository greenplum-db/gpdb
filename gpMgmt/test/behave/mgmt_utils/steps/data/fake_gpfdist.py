import os, sys
import optparse
import socket
import threading
import time
from BaseHTTPServer import HTTPServer, BaseHTTPRequestHandler

no_reponse_handler_key = 'NoResponse'
test_sequence_number_handler_key = 'TestSequenceNumber'
test_bad_response_handler_key = 'TestBadResponse'

def response_with_code(handler, ret_code):
    handler.protocal_version = 'HTTP/1.1' 
    handler.send_response(ret_code)
    handler.send_header("Content-Length", "0")       
    handler.send_header("Content-type", "text/html")
    handler.end_headers()

def simple_exit():
    os._exit(0)

class TimerHandler(BaseHTTPRequestHandler):
    timer = None

    @staticmethod
    def start_timer():
        TimerHandler.timer = threading.Timer(100, simple_exit)
        TimerHandler.timer.start()

    @staticmethod
    def stop_timer():
        TimerHandler.timer.cancel()

    def do_GET(self):
        TimerHandler.stop_timer()
        self.do_GET_internal()
        TimerHandler.start_timer()

    def do_POST(self):
        TimerHandler.stop_timer()
        self.do_POST_internal()
        TimerHandler.start_timer()

    def do_POST_internal():
        pass

    def do_GET_internal():
        pass


class NoResponseHandler(TimerHandler):
    def do_GET_internal(self):
        time.sleep(360)
        simple_exit()

    def do_POST_internal(self):
        time.sleep(360)
        simple_exit()
 
class TestSequenceHandler(TimerHandler):
    expected_seq = 1

    def handle_GP_DONE(self):
        if 'X-GP-DONE' in self.headers:
            response_with_code(self, 404)
            self.log_message('TestSequence succeeded.')
            simple_exit()

    def handle_GP_SEQ(self):
        if 'X-GP-SEQ' not in self.headers:
            self.log_message('X-GP-SEQ expected.')
            simple_exit()
        current_seq = int(self.headers['X-GP-SEQ'])
        self.log_message('incoming X-GP-SEQ:%d' % current_seq)
        if current_seq != TestSequenceHandler.expected_seq:
            self.log_message('X-GP-SEQ:%d expected, but %d found.' %
                        (TestSequenceHandler.expected_seq, current_seq))
            simple_exit()
        response_with_code(self, 200)
        TestSequenceHandler.expected_seq = current_seq + 1

    def do_GET_internal(self):
        reponse_with_code(self, 200)

    def do_POST_internal(self):
        self.handle_GP_DONE()
        self.handle_GP_SEQ()

class TestBadResponseHandler(TimerHandler):
    first_req = True
    prev_seq = None


    def do_GET_internal(self):
        reponse_with_code(self, 200)

    def do_POST_internal(self):
        if 'X-GP-SEQ' not in self.headers:
            self.log_message('X-GP-SEQ expected.')
            simple_exit()
        current_seq = int(self.headers['X-GP-SEQ'])
        self.log_message('incoming X-GP-SEQ:%d' % current_seq)
        if TestBadResponseHandler.first_req:
            TestBadResponseHandler.first_req = False
            TestBadResponseHandler.prev_seq = current_seq
            response_with_code(self, 408)
            return

        if current_seq != TestBadResponseHandler.prev_seq:
            self.log_message('X-GP-SEQ:%d expected, but %d found.' %
                        (TestBadResponseHandler.prev_seq, current_seq))
            simple_exit()
        response_with_code(self, 404)
        self.log_message('TestBadResponse succeeded.')
        simple_exit()

parser = optparse.OptionParser()

gpfdist_modes = {
    no_reponse_handler_key : NoResponseHandler,
    test_sequence_number_handler_key: TestSequenceHandler,
    test_bad_response_handler_key: TestBadResponseHandler,
}


def start_server(port, mode):
    if not gpfdist_modes.has_key(mode):
        print 'mode : %s is not support' % (mode)
        sys.exit(1)
    handler = gpfdist_modes[mode]
    http_server = HTTPServer((socket.gethostname(), int(port)), handler)
    TimerHandler.start_timer()
    http_server.serve_forever()

def parse_commandline():
    parser.add_option("-p", "--port",
                  action="store", dest="gpfdist_port", default=8000,
                  help="gpfdist port number")
    parser.add_option("-m", "--mode",
                  action="store", dest="gpfdist_mode",
                  help="gpfdist run mode\r\n"
                       "%s" % (gpfdist_modes.keys()))

    return parser.parse_args()

if __name__ == '__main__':
    (options, args) = parse_commandline()
    
    port = options.gpfdist_port
    mode = options.gpfdist_mode

    if not port or not mode:
        parser.print_help() 
        sys.exit(1)        

    start_server(options.gpfdist_port, options.gpfdist_mode)
    sys.exit(0) 
