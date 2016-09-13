import os, sys, time

GPMONDIR = 'gpperfmon/data'


# for each line in queries_now.dat 
#	open the q{tmid}-{xid}-{cid}.txt file to retrieve query text/ query plan
#	append to line
#	print the line

for line in open(os.path.join(GPMONDIR, "queries_now.dat")):
    line = line.split('|')
    (tmid, xid, cid) = line[1:4]
    qrytxt = ''
    appname = ''
    rsqname = ''
    priority = ''
    fp = None
    try:
        fp = open(os.path.join(GPMONDIR, "q%s-%s-%s.txt" % (tmid, xid, cid)), 'r')
        meta = fp.readline().split(' ')
        qrytxt = fp.read(int(meta[0])).strip()

        newline = fp.readline()
        meta = fp.readline().split(' ')
        appname = fp.read(int(meta[0])).strip()
        
        newline = fp.readline()
        meta = fp.readline().split(' ')
        rsqname = fp.read(int(meta[0])).strip()
        
        newline = fp.readline()
        meta = fp.readline().split(' ')
        priority = fp.read(int(meta[0])).strip()

        fp.close()
    except:
        qrytxt = "Query text unavailable"
        if fp: fp.close()

    # escape all " with ""
    if qrytxt:
        qrytxt = '""'.join(qrytxt.split('"'))
        line[-5] = '"' + qrytxt + '"'
        line[-3] = '"' + appname + '"'
        line[-2] = '"' + rsqname + '"'
        line[-1] = '"' + priority + '"'
    print '|'.join(line).strip()
