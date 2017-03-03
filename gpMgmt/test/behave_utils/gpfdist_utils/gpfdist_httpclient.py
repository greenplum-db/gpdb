#! /usr/bin/env python

import sys
import os
import optparse
import logging

from gpfdist_utils import GpfdistClient

logger = logging.getLogger('gpfdist_httpclient')

def init_log(logname = None):
    formatter = logging.Formatter('[%(levelname)s] %(asctime)s %(message)s', datefmt='%m/%d/%Y %I:%M:%S')
    if logname:
        logh = logging.FileHandler(logname + '.log')
    else:
        logh = logging.StreamHandler()
    logh.setFormatter(formatter)
    logger.addHandler(logh)
    logger.setLevel(logging.DEBUG)

def parse_commandline(parser):
    parser.add_option("-u", "--url", dest="http_url",
            help="http url to connect", action="store")
    parser.add_option("-m", "--method", dest="http_method", default='POST',
            help="http method, only POST supported", action="store")
    parser.add_option("-d", "--data", dest="http_data", default='',
            help="http post params", action="store")
    parser.add_option("-p", "--path", dest="http_path", 
            help="http path", action="store")
    parser.add_option("-t", "--isdone", dest="b_done", default=False,
            help="whether DONE package", action="store_true")
    parser.add_option("-s", "--session_id", dest="session_id", type='int', default=1, 
            help="gpfdist session id", action="store")
    parser.add_option("-c", "--segment_count", dest="segment_count", type='int', default=1,
            help="gpdb segment count", action="store")
    parser.add_option("-i", "--segment_index", dest="segment_idx", type='int', default=0,
            help="gpdb segment index", action="store")
    parser.add_option("-S", "--sequence_num", dest="sequence_num", type='int', default=None,
            help="gpdb sequence number", action="store")


    (options, args) = parser.parse_args()
    
    if not options.http_url or not options.http_path or options.http_method != 'POST':
        parser.print_help()
        return (False, options)
    return (True, options)

if __name__ == '__main__':
    init_log()

    parser = optparse.OptionParser()
    (ret, options) = parse_commandline(parser)
    if not ret:
        logger.error('command line params parse error') 
        sys.exit(1)
    else:
        logger.debug('option params : %s', str(options))

    client = GpfdistClient(options.http_url, options.http_path, options.session_id,
                           options.segment_count, options.sequence_num)
    if options.b_done:
        client.post_done(options.segment_idx)
    else:
        client.post(options.http_data, options.segment_idx)
    sys.exit(0)

