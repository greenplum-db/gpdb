#!/usr/bin/env python

import unittest
import sys
import os
import string
import time
import socket
import fileinput
import platform
import re
try:
    import subprocess32 as subprocess
except:
    import subprocess
from pygresql import pg

def get_port_from_conf():
    file = os.environ.get('MASTER_DATA_DIRECTORY')+'/postgresql.conf'
    if os.path.isfile(file):
        with open(file) as f:
            for line in f.xreadlines():
                match = re.search('port=\d+',line)
                if match:
                    match1 = re.search('\d+', match.group())
                    if match1:
                        return match1.group()

def get_port():
    port = os.environ['PGPORT']
    if not port:
        port = get_port_from_conf()
    return port if port else 5432

def get_ip(hostname=None):
    if hostname is None:
        hostname = socket.gethostname()
    else:
        hostname = hostname
    hostinfo = socket.getaddrinfo(hostname, None)
    ipaddrlist = list(set([(ai[4][0]) for ai in hostinfo]))
    for myip in ipaddrlist:
        if myip.find(":") > 0:
            ipv6 = myip
            return ipv6
        elif myip.find(".") > 0:
            ipv4 = myip
            return ipv4

def getPortMasterOnly(host = 'localhost',master_value = None,
                      user = os.environ.get('USER'),gphome = os.environ['GPHOME'],
                      mdd=os.environ['MASTER_DATA_DIRECTORY'],port = os.environ['PGPORT']):

    master_pattern = "Context:\s*-1\s*Value:\s*\d+"
    command = "gpconfig -s %s" % ( "port" )

    cmd = "source %s/greenplum_path.sh; export MASTER_DATA_DIRECTORY=%s; export PGPORT=%s; %s" \
           % (gphome, mdd, port, command)

    (ok,out) = run(cmd)
    if not ok:
        raise Exception("Unable to connect to segment server %s as user %s" % (host, user))

    for line in out:
        out = line.split('\n')
    for line in out:
        if re.search(master_pattern, line):
            master_value = int(line.split()[3].strip())

    if master_value is None:
        error_msg = "".join(out)
        raise Exception(error_msg)

    return str(master_value)

"""
Global Values
"""
MYD = os.path.abspath(os.path.dirname(__file__))
mkpath = lambda *x: os.path.join(MYD, *x)
UPD = os.path.abspath(mkpath('..'))
if UPD not in sys.path:
    sys.path.append(UPD)

DBNAME = "postgres"
USER = os.environ.get( "LOGNAME" )
HOST = socket.gethostname()
GPHOME = os.getenv("GPHOME")
PGPORT = get_port()
PGUSER = os.environ.get("PGUSER")
if PGUSER is None:
    PGUSER = USER
PGHOST = os.environ.get("PGHOST")
if PGHOST is None:
    PGHOST = HOST

d = mkpath('config')
if not os.path.exists(d):
    os.mkdir(d)

def write_config_file(mode='insert', reuse_flag='',columns_flag='0',mapping='0',portNum='8081',database='reuse_gptest',host='localhost',formatOpts='text',file='data/external_file_01.txt',table='texttable',format='text',delimiter="'|'",escape='',quote='',truncate='False',log_errors=None, error_limit='0',error_table=None,externalSchema=None,staging_table=None,fast_match='false', encoding=None, preload=True, fill=False):

    f = open(mkpath('config/config_file'),'w')
    f.write("VERSION: 1.0.0.1")
    if database:
        f.write("\nDATABASE: "+database)
    f.write("\nUSER: "+os.environ.get('USER'))
    f.write("\nHOST: "+hostNameAddrs)
    f.write("\nPORT: "+masterPort)
    f.write("\nGPLOAD:")
    f.write("\n   INPUT:")
    f.write("\n    - SOURCE:")
    f.write("\n         LOCAL_HOSTNAME:")
    f.write("\n            - "+hostNameAddrs)
    if portNum:
        f.write("\n         PORT: "+portNum)
    f.write("\n         FILE:")
    f.write("\n            - "+mkpath(file))
    if columns_flag=='1':
        f.write("\n    - COLUMNS:")
        f.write("\n           - s_s1: text")
        f.write("\n           - s_s2: text")
        f.write("\n           - s_dt: timestamp")
        f.write("\n           - s_s3: text")
        f.write("\n           - s_n1: smallint")
        f.write("\n           - s_n2: integer")
        f.write("\n           - s_n3: bigint")
        f.write("\n           - s_n4: decimal")
        f.write("\n           - s_n5: numeric")
        f.write("\n           - s_n6: real")
        f.write("\n           - s_n7: double precision")
        f.write("\n           - s_n8: text")
        f.write("\n           - s_n9: text")
    if format:
        f.write("\n    - FORMAT: "+format)
    if log_errors:
        f.write("\n    - LOG_ERRORS: true")
        f.write("\n    - ERROR_LIMIT: " + error_limit)
    if error_table:
        f.write("\n    - ERROR_TABLE: " + error_table)
        f.write("\n    - ERROR_LIMIT: " + error_limit)
    if delimiter:
        f.write("\n    - DELIMITER: "+delimiter)
    if encoding:
        f.write("\n    - ENCODING: "+encoding)
    if escape:
        f.write("\n    - ESCAPE: "+escape)
    if quote:
        f.write("\n    - QUOTE: "+quote)
    if fill:
        f.write("\n    - FILL_MISSING_FIELDS: true")
    f.write("\n   OUTPUT:")
    f.write("\n    - TABLE: "+table)
    if mode:
        if mode == 'insert':
            f.write("\n    - MODE: "+'insert')
        if mode == 'update':
            f.write("\n    - MODE: "+'update')
        if mode == 'merge':
            f.write("\n    - MODE: "+'merge')
    f.write("\n    - UPDATE_COLUMNS:")
    f.write("\n           - n2")
    f.write("\n    - MATCH_COLUMNS:")
    f.write("\n           - n1")
    f.write("\n           - s1")
    f.write("\n           - s2")
    if mapping=='1':
        f.write("\n    - MAPPING:")
        f.write("\n           s1: s_s1")
        f.write("\n           s2: s_s2")
        f.write("\n           dt: s_dt")
        f.write("\n           s3: s_s3")
        f.write("\n           n1: s_n1")
        f.write("\n           n2: s_n2")
        f.write("\n           n3: s_n3")
        f.write("\n           n4: s_n4")
        f.write("\n           n5: s_n5")
        f.write("\n           n6: s_n6")
        f.write("\n           n7: s_n7")
        f.write("\n           n8: s_n8")
        f.write("\n           n9: s_n9")
    if externalSchema:
        f.write("\n   EXTERNAL:")
        f.write("\n    - SCHEMA: "+externalSchema)
    if preload:
        f.write("\n   PRELOAD:")
        f.write("\n    - REUSE_TABLES: "+reuse_flag)
        f.write("\n    - FAST_MATCH: "+fast_match)
        if staging_table:
            f.write("\n    - STAGING_TABLE: "+staging_table)
    f.write("\n")
    f.close()

def runfile(ifile, flag='', dbname=None, outputPath="", outputFile="",
            username=None,
            PGOPTIONS=None, host = None, port = None):

    if len(outputFile) == 0:
        (ok, out) = psql_run(ifile = ifile,ofile = outFile(ifile, outputPath),flag = flag,
                             dbname=dbname , username=username,
                            PGOPTIONS=PGOPTIONS, host = host, port = port)
    else:
        (ok,out) = psql_run(ifile =ifile, ofile =outFile(outputFile, outputPath), flag =flag,
                            dbname= dbname, username= username,
                            PGOPTIONS= PGOPTIONS, host = host, port = port)

    return (ok, out)

def psql_run(ifile = None, ofile = None, cmd = None,
            flag = '-e',dbname = None,
            username = None,
            PGOPTIONS = None, host = None, port = None):
    '''
    Run a command or file against psql. Return True if OK.
    @param dbname: database name
    @param ifile: input file
    @param cmd: command line
    @param flag: -e Run SQL with no comments (default)
                 -a Run SQL with comments and psql notice
    @param username: psql user
    @param host    : to connect to a different host
    @param port    : port where gpdb is running
    @param PGOPTIONS: connects to postgres via utility mode
    '''
    if dbname is None:
        dbname = DBNAME

    if username is None:
        username = PGUSER  # Use the default login user

    if PGOPTIONS is None:
        PGOPTIONS = ""
    else:
        PGOPTIONS = "PGOPTIONS='%s'" % PGOPTIONS

    if host is None:
        host = "-h %s" % PGHOST
    else:
        host = "-h %s" % host

    if port is None:
        port = ""
    else:
        port = "-p %s" % port

    if cmd:
        arg = '-c "%s"' % cmd
    elif ifile:
        arg = ' < ' + ifile
        if not (flag == '-q'):  # Don't echo commands sent to server
            arg = '-e < ' + ifile
        if flag == '-a':
            arg = '-f ' + ifile
    else:
        raise PSQLError('missing cmd and ifile')

    if ofile == '-':
        ofile = '2>&1'
    elif not ofile:
        ofile = '> /dev/null 2>&1'
    else:
        ofile = '> %s 2>&1' % ofile

    return run('%s psql -d %s %s %s -U %s %s %s %s' %
                             (PGOPTIONS, dbname, host, port, username, flag, arg, ofile))

def run(cmd):
    """
    Run a shell command. Return (True, [result]) if OK, or (False, []) otherwise.
    @params cmd: The command to run at the shell.
            oFile: an optional output file.
            mode: What to do if the output file already exists: 'a' = append;
            'w' = write.  Defaults to append (so that the function is
            backwards compatible).  Yes, this is passed to the open()
            function, so you can theoretically pass any value that is
            valid for the second parameter of open().
    """
    p = subprocess.Popen(cmd,shell=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    out = p.communicate()[0]
    ret = []
    ret.append(out)
    rc = False if p.wait() else True
    return (rc,ret)

def outFile(fname,outputPath = ''):
    return changeExtFile(fname, ".out", outputPath)

def diffFile( fname, outputPath = "" ):
    return changeExtFile( fname, ".diff", outputPath )

def changeExtFile( fname, ext = ".diff", outputPath = "" ):

    if len( outputPath ) == 0:
        return os.path.splitext( fname )[0] + ext
    else:
        filename = fname.split( "/" )
        fname = os.path.splitext( filename[len( filename ) - 1] )[0]
        return outputPath + "/" + fname + ext

def gpdbAnsFile(fname):
    ext = '.ans'
    return os.path.splitext(fname)[0] + ext

def isFileEqual( f1, f2, optionalFlags = "", outputPath = "", myinitfile = ""):

    LMYD = os.path.abspath(os.path.dirname(__file__))
    if not os.access( f1, os.R_OK ):
        raise Exception( 'Error: cannot find file %s' % f1 )
    if not os.access( f2, os.R_OK ):
        raise Exception( 'Error: cannot find file %s' % f2 )
    dfile = diffFile( f1, outputPath = outputPath )
    # Gets the suitePath name to add init_file
    suitePath = f1[0:f1.rindex( "/" )]
    if os.path.exists(suitePath + "/init_file"):
        (ok, out) = run('../gpdiff.pl -w ' + optionalFlags + \
                              ' -I NOTICE: -I HINT: -I CONTEXT: -I GP_IGNORE: --gp_init_file=%s/global_init_file --gp_init_file=%s/init_file '
                              '%s %s > %s 2>&1' % (LMYD, suitePath, f1, f2, dfile))

    else:
        if os.path.exists(myinitfile):
            (ok, out) = run('../gpdiff.pl -w ' + optionalFlags + \
                                  ' -I NOTICE: -I HINT: -I CONTEXT: -I GP_IGNORE: --gp_init_file=%s/global_init_file --gp_init_file=%s '
                                  '%s %s > %s 2>&1' % (LMYD, myinitfile, f1, f2, dfile))
        else:
            (ok, out) = run( '../gpdiff.pl -w ' + optionalFlags + \
                              ' -I NOTICE: -I HINT: -I CONTEXT: -I GP_IGNORE: --gp_init_file=%s/global_init_file '
                              '%s %s > %s 2>&1' % ( LMYD, f1, f2, dfile ) )


    if ok:
        os.unlink( dfile )
    return ok

def read_diff(ifile, outputPath):
    """
    Opens the diff file that is assocated with the given input file and returns
    its contents as a string.
    """
    dfile = diffFile(ifile, outputPath)
    with open(dfile, 'r') as diff:
        return diff.read()

def modify_sql_file(num):
    file = mkpath('query%d.sql' % num)
    user = os.environ.get('USER')
    if not user:
        user = os.environ.get('USER')
    if os.path.isfile(file):
        for line in fileinput.FileInput(file,inplace=1):
            line = line.replace("gpload.py ","gpload ")
            print str(re.sub('\n','',line))

def copy_data(source='',target=''):
    cmd = 'cp '+ mkpath('data/' + source) + ' ' + mkpath(target)
    p = subprocess.Popen(cmd,shell=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    return p.communicate()

hostNameAddrs = get_ip(HOST)
masterPort = getPortMasterOnly()

def get_table_name():
    try:
        db = pg.DB(dbname='reuse_gptest'
                  ,host='localhost'
                  ,port=int(PGPORT)
                  )
    except Exception,e:
        errorMessage = str(e)
        print 'could not connect to database: ' + errorMessage
    queryString = """SELECT relname
                     from pg_class
                     WHERE relname
                     like 'ext_gpload_reusable%'
                     OR relname
                     like 'staging_gpload_reusable%';"""
    resultList = db.query(queryString.encode('utf-8')).getresult()
    return resultList

def drop_tables():
    try:
        db = pg.DB(dbname='reuse_gptest'
                  ,host='localhost'
                  ,port=int(PGPORT)
                  )
    except Exception,e:
        errorMessage = str(e)
        print 'could not connect to database: ' + errorMessage

    list = get_table_name()
    for i in list:
        name = i[0]
        match = re.search('ext_gpload',name)
        if match:
            queryString = "DROP EXTERNAL TABLE %s" % name
            db.query(queryString.encode('utf-8'))

        else:
            queryString = "DROP TABLE %s" % name
            db.query(queryString.encode('utf-8'))

class PSQLError(Exception):
    '''
    PSQLError is the base class for exceptions in this module
    http://docs.python.org/tutorial/errors.html
    We want to raise an error and not a failure. The reason for an error
    might be program error, file not found, etc.
    Failure is define as test case failures, when the output is different
    from the expected result.
    '''
    pass

class GPLoad_FormatOpts_TestCase(unittest.TestCase):

    def check_result(self,ifile, optionalFlags = "-U3", outputPath = ""):
        """
        PURPOSE: compare the actual and expected output files and report an
            error if they don't match.
        PARAMETERS:
            ifile: the name of the .sql file whose actual and expected outputs
                we want to compare.  You may include the path as well as the
                filename.  This function will process this file name to
                figure out the proper names of the .out and .ans files.
            optionalFlags: command-line options (if any) for diff.
                For example, pass " -B " (with the blank spaces) to ignore
                blank lines. By default, diffs are unified with 3 lines of
                context (i.e. optionalFlags is "-U3").
        """
        f1 = gpdbAnsFile(ifile)
        f2 = outFile(ifile, outputPath=outputPath)

        result = isFileEqual(f1, f2, optionalFlags, outputPath=outputPath)
        diff = None if result else read_diff(ifile, outputPath)
        self.assertTrue(result, "query resulted in diff:\n{}".format(diff))

        return True

    def doTest(self, num):
        file = mkpath('query%d.diff' % num)
        if os.path.isfile(file):
           run("rm -f" + " " + file)
        modify_sql_file(num)
        file = mkpath('query%d.sql' % num)
        runfile(file)
        self.check_result(file)

    def test_00_gpload_formatOpts_setup(self):
        "0  gpload setup"
        for num in range(1,40):
           f = open(mkpath('query%d.sql' % num),'w')
           f.write("\! gpload -f "+mkpath('config/config_file')+ " -d reuse_gptest\n"+"\! gpload -f "+mkpath('config/config_file')+ " -d reuse_gptest\n")
           f.close()
        file = mkpath('setup.sql')
        runfile(file)
        self.check_result(file)

    def test_01_gpload_formatOpts_delimiter(self):
        "1  gpload formatOpts delimiter '|' with reuse "
        copy_data('external_file_01.txt','data_file.txt')
        write_config_file(reuse_flag='true',formatOpts='text',file='data_file.txt',table='texttable',delimiter="'|'")
        self.doTest(1)

    def test_02_gpload_formatOpts_delimiter(self):
        "2  gpload formatOpts delimiter '\t' with reuse"
        copy_data('external_file_02.txt','data_file.txt')
        write_config_file(reuse_flag='true',formatOpts='text',file='data_file.txt',table='texttable',delimiter="'\t'")
        self.doTest(2)

    def test_03_gpload_formatOpts_delimiter(self):
        "3  gpload formatOpts delimiter E'\t' with reuse"
        copy_data('external_file_02.txt','data_file.txt')
        write_config_file(reuse_flag='true',formatOpts='text',file='data_file.txt',table='texttable',delimiter="E'\\t'")
        self.doTest(3)

    def test_04_gpload_formatOpts_delimiter(self):
        "4  gpload formatOpts delimiter E'\u0009' with reuse"
        copy_data('external_file_02.txt','data_file.txt')
        write_config_file(reuse_flag='true',formatOpts='text',file='data_file.txt',table='texttable',delimiter="E'\u0009'")
        self.doTest(4)

    def test_05_gpload_formatOpts_delimiter(self):
        "5  gpload formatOpts delimiter E'\\'' with reuse"
        copy_data('external_file_03.txt','data_file.txt')
        write_config_file(reuse_flag='true',formatOpts='text',file='data_file.txt',table='texttable',delimiter="E'\''")
        self.doTest(5)

    def test_06_gpload_formatOpts_delimiter(self):
        "6  gpload formatOpts delimiter \"'\" with reuse"
        copy_data('external_file_03.txt','data_file.txt')
        write_config_file(reuse_flag='true',formatOpts='text',file='data_file.txt',table='texttable',delimiter="\"'\"")
        self.doTest(6)

    def test_07_gpload_reuse_table_insert_mode_without_reuse(self):
        "7  gpload insert mode without reuse"
        runfile(mkpath('setup.sql'))
        f = open(mkpath('query7.sql'),'a')
        f.write("\! psql -d reuse_gptest -c 'select count(*) from texttable;'")
        f.close()
        write_config_file(mode='insert',reuse_flag='false')
        self.doTest(7)

    def test_08_gpload_reuse_table_update_mode_with_reuse(self):
        "8  gpload update mode with reuse"
        drop_tables()
        copy_data('external_file_04.txt','data_file.txt')
        write_config_file(mode='update',reuse_flag='true',file='data_file.txt')
        self.doTest(8)

    def test_09_gpload_reuse_table_update_mode_without_reuse(self):
        "9  gpload update mode without reuse"
        f = open(mkpath('query9.sql'),'a')
        f.write("\! psql -d reuse_gptest -c 'select count(*) from texttable;'\n"+"\! psql -d reuse_gptest -c 'select * from texttable where n2=222;'")
        f.close()
        copy_data('external_file_05.txt','data_file.txt')
        write_config_file(mode='update',reuse_flag='false',file='data_file.txt')
        self.doTest(9)

    def test_10_gpload_reuse_table_merge_mode_with_reuse(self):
        "10  gpload merge mode with reuse "
        drop_tables()
        copy_data('external_file_06.txt','data_file.txt')
        write_config_file('merge','true',file='data_file.txt')
        self.doTest(10)

    def test_11_gpload_reuse_table_merge_mode_without_reuse(self):
        "11  gpload merge mode without reuse "
        copy_data('external_file_07.txt','data_file.txt')
        write_config_file('merge','false',file='data_file.txt')
        self.doTest(11)

    def test_12_gpload_reuse_table_merge_mode_with_different_columns_number_in_file(self):
        "12 gpload merge mode with reuse (RERUN with different columns number in file) "
        psql_run(cmd="ALTER TABLE texttable ADD column n8 text",dbname='reuse_gptest')
        copy_data('external_file_08.txt','data_file.txt')
        write_config_file('merge','true',file='data_file.txt')
        self.doTest(12)

    def test_13_gpload_reuse_table_merge_mode_with_different_columns_number_in_DB(self):
        "13  gpload merge mode with reuse (RERUN with different columns number in DB table) "
        preTest = mkpath('pre_test_13.sql')
        psql_run(preTest, dbname='reuse_gptest')
        copy_data('external_file_09.txt','data_file.txt')
        write_config_file('merge','true',file='data_file.txt')
        self.doTest(13)

    def test_14_gpload_reuse_table_update_mode_with_reuse_RERUN(self):
        "14 gpload update mode with reuse (RERUN) "
        write_config_file('update','true',file='data_file.txt')
        self.doTest(14)

    def test_15_gpload_reuse_table_merge_mode_with_different_columns_order(self):
        "15 gpload merge mode with different columns' order "
        copy_data('external_file_10.txt','data/data_file.tbl')
        write_config_file('merge','true',file='data/data_file.tbl',columns_flag='1',mapping='1')
        self.doTest(15)

    def test_16_gpload_formatOpts_quote(self):
        "16  gpload formatOpts quote unspecified in CSV with reuse "
        copy_data('external_file_11.csv','data_file.csv')
        write_config_file(reuse_flag='true',formatOpts='csv',file='data_file.csv',table='csvtable',format='csv',delimiter="','")
        self.doTest(16)

    def test_17_gpload_formatOpts_quote(self):
        "17  gpload formatOpts quote '\\x26'(&) with reuse"
        copy_data('external_file_12.csv','data_file.csv')
        write_config_file(reuse_flag='true',formatOpts='csv',file='data_file.csv',table='csvtable',format='csv',delimiter="','",quote="'\x26'")
        self.doTest(17)

    def test_18_gpload_formatOpts_quote(self):
        "18  gpload formatOpts quote E'\\x26'(&) with reuse"
        copy_data('external_file_12.csv','data_file.csv')
        write_config_file(reuse_flag='true',formatOpts='csv',file='data_file.csv',table='csvtable',format='csv',delimiter="','",quote="E'\x26'")
        self.doTest(18)

    def test_19_gpload_formatOpts_escape(self):
        "19  gpload formatOpts escape '\\' with reuse"
        copy_data('external_file_01.txt','data_file.txt')
        file = mkpath('setup.sql')
        runfile(file)
        write_config_file(reuse_flag='true',formatOpts='text',file='data_file.txt',table='texttable',escape='\\')
        self.doTest(19)

    def test_20_gpload_formatOpts_escape(self):
        "20  gpload formatOpts escape '\\' with reuse"
        copy_data('external_file_01.txt','data_file.txt')
        write_config_file(reuse_flag='true',formatOpts='text',file='data_file.txt',table='texttable',escape= '\x5C')
        self.doTest(20)

    def test_21_gpload_formatOpts_escape(self):
        "21  gpload formatOpts escape E'\\\\' with reuse"
        copy_data('external_file_01.txt','data_file.txt')
        write_config_file(reuse_flag='true',formatOpts='text',file='data_file.txt',table='texttable',escape="E'\\\\'")
        self.doTest(21)
    # case 22 is flaky on concourse. It may report: Fatal Python error: GC object already tracked during testing.
    # This is seldom issue. we can't reproduce it locally, so we disable it, in order to not blocking others
    #def test_22_gpload_error_count(self):
    #    "22  gpload error count"
    #    f = open(mkpath('query22.sql'),'a')
    #    f.write("\! psql -d reuse_gptest -c 'select count(*) from csvtable;'")
    #    f.close()
    #    f = open(mkpath('data/large_file.csv'),'w')
    #    for i in range(0, 10000):
    #        if i % 2 == 0:
    #            f.write('1997,Ford,E350,"ac, abs, moon",3000.00,a\n')
    #        else:
    #            f.write('1997,Ford,E350,"ac, abs, moon",3000.00\n')
    #    f.close()
    #    copy_data('large_file.csv','data_file.csv')
    #    write_config_file(reuse_flag='true',formatOpts='csv',file='data_file.csv',table='csvtable',format='csv',delimiter="','",log_errors=True,error_limit='90000000')
    #   self.doTest(22)
    def test_23_gpload_error_count(self):
        "23  gpload error_table"
        file = mkpath('setup.sql')
        runfile(file)
        f = open(mkpath('query23.sql'),'a')
        f.write("\! psql -d reuse_gptest -c 'select count(*) from csvtable;'")
        f.close()
        f = open(mkpath('data/large_file.csv'),'w')
        for i in range(0, 10000):
            if i % 2 == 0:
                f.write('1997,Ford,E350,"ac, abs, moon",3000.00,a\n')
            else:
                f.write('1997,Ford,E350,"ac, abs, moon",3000.00\n')
        f.close()
        copy_data('large_file.csv','data_file.csv')
        write_config_file(reuse_flag='true',formatOpts='csv',file='data_file.csv',table='csvtable',format='csv',delimiter="','",error_table="err_table",error_limit='90000000')
        self.doTest(23)
    def test_24_gpload_error_count(self):
        "24  gpload error count with ext schema"
        file = mkpath('setup.sql')
        runfile(file)
        f = open(mkpath('query24.sql'),'a')
        f.write("\! psql -d reuse_gptest -c 'select count(*) from csvtable;'")
        f.close()
        f = open(mkpath('data/large_file.csv'),'w')
        for i in range(0, 10000):
            if i % 2 == 0:
                f.write('1997,Ford,E350,"ac, abs, moon",3000.00,a\n')
            else:
                f.write('1997,Ford,E350,"ac, abs, moon",3000.00\n')
        f.close()
        copy_data('large_file.csv','data_file.csv')
        write_config_file(reuse_flag='true',formatOpts='csv',file='data_file.csv',table='csvtable',format='csv',delimiter="','",log_errors=True,error_limit='90000000',externalSchema='test')
        self.doTest(24)
    def test_25_gpload_ext_staging_table(self):
        "25  gpload reuse ext_staging_table if it is configured"
        file = mkpath('setup.sql')
        runfile(file)
        f = open(mkpath('query25.sql'),'a')
        f.write("\! psql -d reuse_gptest -c 'select count(*) from csvtable;'")
        f.close()
        copy_data('external_file_13.csv','data_file.csv')
        write_config_file(reuse_flag='true',formatOpts='csv',file='data_file.csv',table='csvtable',format='csv',delimiter="','",log_errors=True,error_limit='10',staging_table='staging_table')
        self.doTest(25)
    def test_26_gpload_ext_staging_table_with_externalschema(self):
        "26  gpload reuse ext_staging_table if it is configured with externalschema"
        file = mkpath('setup.sql')
        runfile(file)
        f = open(mkpath('query26.sql'),'a')
        f.write("\! psql -d reuse_gptest -c 'select count(*) from csvtable;'")
        f.close()
        copy_data('external_file_13.csv','data_file.csv')
        write_config_file(reuse_flag='true',formatOpts='csv',file='data_file.csv',table='csvtable',format='csv',delimiter="','",log_errors=True,error_limit='10',staging_table='staging_table',externalSchema='test')
        self.doTest(26)
    def test_27_gpload_ext_staging_table_with_externalschema(self):
        "27  gpload reuse ext_staging_table if it is configured with externalschema"
        file = mkpath('setup.sql')
        runfile(file)
        f = open(mkpath('query27.sql'),'a')
        f.write("\! psql -d reuse_gptest -c 'select count(*) from test.csvtable;'")
        f.close()
        copy_data('external_file_13.csv','data_file.csv')
        write_config_file(reuse_flag='true',formatOpts='csv',file='data_file.csv',table='test.csvtable',format='csv',delimiter="','",log_errors=True,error_limit='10',staging_table='staging_table',externalSchema="'%'")
        self.doTest(27)
    def test_28_gpload_ext_staging_table_with_dot(self):
        "28  gpload reuse ext_staging_table if it is configured with dot"
        file = mkpath('setup.sql')
        runfile(file)
        f = open(mkpath('query28.sql'),'a')
        f.write("\! psql -d reuse_gptest -c 'select count(*) from test.csvtable;'")
        f.close()
        copy_data('external_file_13.csv','data_file.csv')
        write_config_file(reuse_flag='true',formatOpts='csv',file='data_file.csv',table='test.csvtable',format='csv',delimiter="','",log_errors=True,error_limit='10',staging_table='t.staging_table')
        self.doTest(28)
    def test_29_gpload_reuse_table_insert_mode_with_reuse_and_null(self):
        "29  gpload insert mode with reuse and null"
        runfile(mkpath('setup.sql'))
        f = open(mkpath('query29.sql'),'a')
        f.write("\! psql -d reuse_gptest -c 'select count(*) from texttable where n2 is null;'")
        f.close()
        copy_data('external_file_14.txt','data_file.txt')
        write_config_file(mode='insert',reuse_flag='true',file='data_file.txt',log_errors=True, error_limit='100')
        self.doTest(29)

    def test_30_gpload_reuse_table_update_mode_with_fast_match(self):
        "30  gpload update mode with fast match"
        drop_tables()
        copy_data('external_file_04.txt','data_file.txt')
        write_config_file(mode='update',reuse_flag='true',fast_match='true',file='data_file.txt')
        self.doTest(30)

    def test_31_gpload_reuse_table_update_mode_with_fast_match_and_different_columns_number(self):
        "31 gpload update mode with fast match and differenct columns number) "
        psql_run(cmd="ALTER TABLE texttable ADD column n8 text",dbname='reuse_gptest')
        copy_data('external_file_08.txt','data_file.txt')
        write_config_file(mode='update',reuse_flag='true',fast_match='true',file='data_file.txt')
        self.doTest(31)

    def test_32_gpload_update_mode_without_reuse_table_with_fast_match(self):
        "32  gpload update mode when reuse table is false and fast match is true"
        drop_tables()
        copy_data('external_file_08.txt','data_file.txt')
        write_config_file(mode='update',reuse_flag='false',fast_match='true',file='data_file.txt')
        self.doTest(32)
    def test_33_gpload_reuse_table_merge_mode_with_fast_match_and_external_schema(self):
        "33  gpload update mode with fast match and external schema"
        file = mkpath('setup.sql')
        runfile(file)
        copy_data('external_file_04.txt','data_file.txt')
        write_config_file(mode='merge',reuse_flag='true',fast_match='true',file='data_file.txt',externalSchema='test')
        self.doTest(33)
    def test_34_gpload_reuse_table_merge_mode_with_fast_match_and_encoding(self):
        "34  gpload merge mode with fast match and encoding GBK"
        file = mkpath('setup.sql')
        runfile(file)
        copy_data('external_file_04.txt','data_file.txt')
        write_config_file(mode='merge',reuse_flag='true',fast_match='true',file='data_file.txt',encoding='GBK')
        self.doTest(34)

    def test_35_gpload_reuse_table_merge_mode_with_fast_match_default_encoding(self):
        "35  gpload does not reuse table when encoding is setted from GBK to empty"
        write_config_file(mode='merge',reuse_flag='true',fast_match='true',file='data_file.txt')
        self.doTest(35)

    def test_36_gpload_reuse_table_merge_mode_default_encoding(self):
        "36  gpload merge mode with encoding GBK"
        file = mkpath('setup.sql')
        runfile(file)
        copy_data('external_file_04.txt','data_file.txt')
        write_config_file(mode='merge',reuse_flag='true',fast_match='false',file='data_file.txt',encoding='GBK')
        self.doTest(36)

    def test_37_gpload_reuse_table_merge_mode_invalid_encoding(self):
        "37  gpload merge mode with invalid encoding"
        file = mkpath('setup.sql')
        runfile(file)
        copy_data('external_file_04.txt','data_file.txt')
        write_config_file(mode='merge',reuse_flag='true',fast_match='false',file='data_file.txt',encoding='xxxx')
        self.doTest(37)

    def test_38_gpload_without_preload(self):
        "38  gpload insert mode without preload"
        file = mkpath('setup.sql')
        runfile(file)
        copy_data('external_file_04.txt','data_file.txt')
        write_config_file(mode='insert',reuse_flag='true',fast_match='false',file='data_file.txt',error_table="err_table",error_limit='1000',preload=False)
        self.doTest(38)

    def test_39_gpload_fill_missing_fields(self):
        "39  gpload fill missing fields"
        file = mkpath('setup.sql')
        runfile(file)
        copy_data('external_file_04.txt','data_file.txt')
        write_config_file(mode='insert',reuse_flag='false',fast_match='false',file='data_file.txt',table='texttable1', error_limit='1000', fill=True)
        self.doTest(39)

if __name__ == '__main__':
    suite = unittest.TestLoader().loadTestsFromTestCase(GPLoad_FormatOpts_TestCase)
    runner = unittest.TextTestRunner(verbosity=2)
    ret = not runner.run(suite).wasSuccessful()
    sys.exit(ret)
