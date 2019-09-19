#!/usr/bin/env python

import os, sys

PATH, EXECNAME = os.path.split(os.path.abspath(sys.argv[0]))
MYD = os.path.abspath(os.path.dirname(__file__))
mkpath = lambda *x: os.path.join(MYD, *x)

#globals
SAMPLE_QUERY="(select count(*) from (select o0.o_orderkey from (heap_orders o0 left outer join heap_orders o1 on o0.o_orderkey = o1.o_orderkey left outer join heap_orders o2 on o2.o_orderkey = o1.o_orderkey left outer join heap_orders o3 on o3.o_orderkey = o2.o_orderkey left outer join heap_orders o4 on o4.o_orderkey = o3.o_orderkey) order by o0.o_orderkey) as foo);"

import shutil, time, re
try:
    import subprocess32 as subprocess
except:
    import subprocess
from optparse import OptionParser, OptionGroup

try:
    from gppylib import gplog
    from multiprocessing import Process
    from gppylib.commands import unix
except Exception, e:
    sys.exit('Cannot import modules. Please check that you have sourced greenplum_path.sh. Detail: ' + str(e))

def parseargs( help=False ):
    parser = OptionParser()
    parser.remove_option('-h')

    parser.add_option( '-h', '--help', '-?', action='help', help='show this message and exit')
    # --complexity
    parser.add_option( '--complexity', metavar="<Number>", type=int, default=1,
        help='The complexity number determines the number of operators in the query. With complexity 1, you get ~10 memory guzzling operators. With 2, the number of operators ~20 and so on.')
    # --concurrency
    parser.add_option( '--concurrency', metavar='<Number>', type=int, default=1,
        help='Number of concurrent sessions from which to execute the queries')
    # --dbname
    parser.add_option( '--dbname', metavar='<dbname>', default='gptest',
        help='Database where mpph heap_ tables are created and data loaded.')
    # --username
    parser.add_option( '--username', metavar='<username>', default='gpadmin',
        help='Database username to use while executing the query')
    options, args = parser.parse_args()

    try:
        if sys.argv[1:].__len__() == 0:
            parser.print_help()
            raise Exception( "\nERROR: You did not provide any arguments\n\n" )
        if options.complexity <= 0:
            parser.print_help()
            raise Exception("\nERROR: You must provide a positive integer for complexity\n\n")
        if options.concurrency <= 0:
            parser.print_help()
            raise Exception("\nERROR: You must provide a positive integer for concurrency\n\n")
    except Exception, e:
        print e
        sys.exit(-1)

    if help:
        parser.print_help()
    else:
        return options, args

def execute_sql_file(sqlfile,outfile,dbname,user=None):
    '''
       Executes the sqlfile by feeding it to psql
    '''
    cmdstr = 'psql %s ' % (dbname)

    if user:
       cmdstr += '-U %s' % (user)

    cmdstr += ' -a -f %s > %s 2>&1' % (sqlfile,outfile)
    p = subprocess.Popen(cmdstr,shell=True)
    p.wait()

def wait( proclist, msg=None ):
    '''
    Takes a process object or a list of process objects and waits for
        them to complete.  The process objects will be started (if they
        have not been started already.
    Also accepts a message to output while waiting for process' to finish.
    If on a tty, a progress bar is twirled
    '''
    
    start = time.time()

    # Convert single process object to a list (if a list was not passed)
    procs = list()
    if not isinstance( proclist, list ):
        procs.append( proclist )
    else:
        procs = proclist

    for proc in procs:
        proc.start()

    for proc in procs:
        proc.join()

    end = time.time()

    return (end - start)

###### main()
if __name__ == "__main__":

    options, args = parseargs()
    
    # run tests 
    proc_list = list()

    ddlfile = mkpath('/tmp/ddl.sql')
    fd = open(ddlfile,'w')
    fd.write('\\timing\n')
    str = SAMPLE_QUERY
    fd.write(str)
    fd.close()

    #cleanup the output files
    cmdstr = 'rm -rf /tmp/concur_*'
    p = subprocess.Popen(cmdstr,shell=True)
    p.wait()

    for i in range(0,options.concurrency):
        outfile = mkpath('/tmp/concur_%d_out' % (i)) 
        proc_list.append( Process( target=execute_sql_file, args=(ddlfile, outfile, options.dbname, options.username),
            name='Query %d' % i))

    # Kick off the tests
    wait( proc_list, 'Running concurrent queries: ' )

