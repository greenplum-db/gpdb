import httplib, urllib

class GpfdistClient:
    def __init__(self, url, path, xid, segmentcount, sequence_num): 
        self.url = url
        self.path = path
        self.xid = xid
        self.segmentcount = segmentcount
        self.sequence_num = sequence_num
            
    def send_request(self, method, params, segmentid, isdone):
        headers = {
                "Accept":                   "*/*",
                "X-GP-XID":                 self.xid,
                "X-GP-CID":                 "0",
                "X-GP-SN":                  "0",
                "X-GP-SEGMENT-ID":          segmentid,
                "X-GP-SEGMENT-COUNT":       self.segmentcount,
                "X-GP-LINE-DELIM-LENGTH":   "-1",
                "X-GP-PROTO":               "0",
                "Content-Type":             "text/xml",
        }

        if self.sequence_num:
            headers = dict({"X-GP-SEQ": self.sequence_num}, **headers)
       
        if method == 'POST' and params != None:
            params += '\n'
            headers['Content-Length'] = str(len(params))
        
        if isdone: 
            headers["X-GP-DONE"] = 1
        
        print headers
        conn = httplib.HTTPConnection(self.url)
        conn.request(method, self.path, params, headers)
        response = conn.getresponse()
        print response.status, response.reason
        data = response.read()
        conn.close()

    def post(self, params, segmentid):
        self.send_request('POST', params, segmentid, False)

    def post_done(self, segmentid):
        self.send_request('POST', None, segmentid, True)

    def get(self, params, segmentid, isdone):
        '''
            TODO 
            gpfdist get method has different protocol
        '''
        self.send_request('GET', params, segmentid, isdone)

if __name__ == '__main__':
    client = GpfdistClient('localhost:8080', '/test', '2222', 2)
    client.post('abc 123', 1)
    client.post_done(1)

#    client.get('abc 123', 1, False)
#    client.get('abc 123', 1, True)

