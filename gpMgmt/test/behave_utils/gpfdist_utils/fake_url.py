#!/data/home/gpadmin/greenplum-db-devel/ext/python/bin/python
import httplib
import optparse
import string
import sys
import time

class HTTPCli:
    @staticmethod
    def send_http_request(url_path, data, **headers):
        parts = url_path.strip().split('/')
        if len(parts) < 2 or parts[0] == '':
            raise 'Bad url format:%s' % url_path
        host = parts[0]
        local_path = '/' + string.join(parts[1:], '/')
        headers = dict({ 'Content-Length': len(data) }, **headers)
        conn = httplib.HTTPConnection(host)
        conn.request('POST', local_path, data, headers)
        time.sleep(2)
        result = conn.getresponse()
        status = result.status
        content = result.read()
        print status, content
        conn.close()


parser = optparse.OptionParser()


def parse_commandline():
    parser.add_option("-u", "--url",
                  action="store", dest="url",
                  help="gpfdist url path")
    parser.add_option("-H", "--header_file",
                  action="store", dest="header_file",
                  help="HTTP header file used by gpfdist")
    parser.add_option("-D", "--data",
                  action="store", dest="data", default="",
                  help="HTTP data used by gpfdist")

    return parser.parse_args()

if __name__ == '__main__':
    (options, args) = parse_commandline()
    
    url = options.url
    header_file = options.header_file
    print options.data
    data = options.data + '\n'

    if not url or not header_file:
        parser.print_help() 
        sys.exit(1)        
    header_str = file(header_file).read()
    header_str = header_str.replace(':', '\':\'').replace('\n', '\',\'')
    header_str = '{\'' + header_str[:-1] + '}'
    headers = eval(header_str)
    HTTPCli.send_http_request(url, data, **headers)
    sys.exit(0) 
