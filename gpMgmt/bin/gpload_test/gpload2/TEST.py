#!/usr/bin/env pytest

import unittest
import sys
import os
import string
import time
import socket
import fileinput
import platform
import re
#import yaml
import pytest

try:
    import subprocess32 as subprocess
except:
    import subprocess
try:
    import pg
except ImportError:
    try:
        from pygresql import pg
    except Exception as e:
        pass
except Exception as e:
    print(repr(e))
    errorMsg = "gpload was unable to import The PyGreSQL Python module (pg.py) - %s\n" % str(e)
    sys.stderr.write(str(errorMsg))
    errorMsg = "Please check if you have the correct Visual Studio redistributable package installed.\n"
    sys.stderr.write(str(errorMsg))
    sys.exit(2)

def get_port_from_conf():
    file = os.environ.get('MASTER_DATA_DIRECTORY')+'/postgresql.conf'
    if os.path.isfile(file):
        with open(file) as f:
            for line in f.xreadlines():
                match = re.search(r'port=\d+',line)
                if match:
                    match1 = re.search(r'\d+', match.group())
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

def getPortMasterOnly(host = 'localhost',master_value = None,
                      user = os.environ.get('USER'),gphome = os.environ['GPHOME'],
                      mdd=os.environ['MASTER_DATA_DIRECTORY'],port = os.environ['PGPORT']):

    master_pattern = r"Context:\s*-1\s*Value:\s*\d+"
    command = "gpconfig -s %s" % ( "port" )

    cmd = "source %s/greenplum_path.sh; export MASTER_DATA_DIRECTORY=%s; export PGPORT=%s; %s" \
           % (gphome, mdd, port, command)

    (ok,out) = run(cmd)
    if not ok:
        cmd = "python %s/bin/gpconfig -s port"%(gphome)
        (ok,out) = run(cmd)
        if not ok:
            raise Exception("Unable to connect to segment server %s as user %s" % (host, user))

    for line in out:
        out = line.decode().split('\n')
    for line in out:
        if re.search(master_pattern, line):
            master_value = int(line.split()[3].strip())

    if master_value is None:
        error_msg = "".join(out)
        raise Exception(error_msg)

    return str(master_value)


def runfile(ifile, flag='', dbname=None, outputPath="", outputFile="",
            username=None,
            PGOPTIONS=None, host = None, port = None):
    '''
    run a file in psql
    '''

    if len(outputFile) == 0:
        (ok, out) = psql_run(ifile = ifile,ofile = outFile(ifile, outputPath),flag = flag,
                             dbname=dbname , username=username,
                            PGOPTIONS=PGOPTIONS, host = host, port = port)
    else:
        (ok,out) = psql_run(ifile =ifile, ofile =outFile(outputFile, outputPath), flag =flag,
                            dbname= dbname, username= username,
                            PGOPTIONS= PGOPTIONS, host = host, port = port)

    return (ok, out)


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
if PGHOST is None or PGHOST=="":
    PGHOST = HOST

d = mkpath('config')
if not os.path.exists(d):
    os.mkdir(d)


hostNameAddrs = get_ip(HOST)
masterPort = getPortMasterOnly()


def write_config_file(version='1.0.0.1', database='reuse_gptest', user=os.environ.get('USER'), host=hostNameAddrs, port=masterPort, config='config/config_file', local_host=[hostNameAddrs], file='data/external_file_01.txt', input_port='8081', port_range=None,
    ssl=None,columns=None, format='text', log_errors=None, error_limit=None, delimiter="'|'", encoding=None, escape=None, null_as=None, fill_missing_fields=None, quote=None, header=None, transform=None, transform_config=None, max_line_length=None, 
    table='texttable', mode='insert', update_columns=['n2'], update_condition=None, match_columns=['n1','s1','s2'], staging_table=None, mapping=None, externalSchema=None, preload=True, truncate=False, reuse_tables=True, fast_match=None,
    sql=False, before=None, after=None, error_table=None):

    f = open(config,'w')
    f.write("VERSION: "+version)
    if database:
        f.write("\nDATABASE: "+database)
    if user:
        f.write("\nUSER: "+user)
    f.write("\nHOST: "+host)
    f.write("\nPORT: "+str(port))

    f.write("\nGPLOAD:")
    f.write("\n   INPUT:")
    f.write("\n    - SOURCE:")
    if local_host:
        f.write("\n         LOCAL_HOSTNAME:")
        for lh in local_host:
            f.write("\n            - "+lh)
    if input_port:
        f.write("\n         PORT: "+str(input_port))
    if port_range:
        f.write("\n         PORT_RANGE: "+port_range)
    f.write("\n         FILE:")
    if(isinstance(file,str)):
        f.write("\n            - "+mkpath(file))
    if (isinstance(file,list)):
        for ff in file:
            f.write("\n            - "+mkpath(ff))
    if ssl is not None:
        f.write("\n         SSL: "+str(ssl))
    if columns:
        f.write("\n    - COLUMNS:")
        for c,ct in columns.items():
            f.write("\n           - "+c+": "+ct)
    if format:
        f.write("\n    - FORMAT: "+format)
    if log_errors:
        f.write("\n    - LOG_ERRORS: "+str(log_errors))
    if error_limit:
        f.write("\n    - ERROR_LIMIT: "+str(error_limit))
    if error_table:
        f.write("\n    - ERROR_TABLE: "+error_table)
    f.write("\n    - DELIMITER: "+delimiter)
    if encoding:
        f.write("\n    - ENCODING: "+encoding)
    if escape:
        f.write("\n    - ESCAPE: "+escape)
    if null_as:
        f.write("\n    - NULL_AS: "+null_as)
    if fill_missing_fields:
        f.write("\n    - FILL_MISSING_FIELDS: "+str(fill_missing_fields))
    if quote:
        f.write("\n    - QUOTE: "+quote)
    if header:
        f.write("\n    - HEADER: "+header)
    if transform:
        f.write("\n    - TRANSFORM: "+transform)
    if transform_config:
        f.write("\n    - TRANSFORM_CONFIG: "+transform_config)
    if max_line_length:
        f.write("\n    - MAX_LINE_LENGTH: "+str(max_line_length))
    if externalSchema:
        f.write("\n   EXTERNAL:")
        f.write("\n    - SCHEMA: "+externalSchema)

    f.write("\n   OUTPUT:")
    f.write("\n    - TABLE: "+table)
    f.write("\n    - MODE: "+mode)
    if match_columns:
        f.write("\n    - MATCH_COLUMNS:")
        for m in match_columns:
            f.write("\n           - "+m)
    if update_columns:
        f.write("\n    - UPDATE_COLUMNS:")
        for u in update_columns:
            f.write("\n           - "+u)
    if update_condition:
        f.write("\n    - UPDATE_CONDITION: "+update_condition)
    if mapping:
        f.write("\n    - MAPPING:")
        for key, val in mapping.items():
            f.write("\n           "+key+": "+val)

    if preload:
        f.write("\n   PRELOAD:" )
        if truncate:
            f.write("\n    - TRUNCSTE: "+str(truncate))
        if reuse_tables:
            f.write("\n    - REUSE_TABLES: "+str(reuse_tables))
        if fast_match:
            f.write("\n    - FAST_MATCH: "+str(fast_match))
        if staging_table:
            f.write("\n    - STAGING_TABLE: "+staging_table)

    if sql:
        f.write("\n   SQL:" )
        if before:
            f.write("\n    - BEFORE: "+before)
        if after:
            f.write("\n    - AFTER: "+after)
    f.close()

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

def alterOutFile(file,old_str,new_str):
    with open(file, "r", encoding="utf-8") as f1,open("%s.bak" % file, "w", encoding="utf-8") as f2:
        for line in f1:
            for i in range(len(old_str)):
                line = re.sub(old_str[i],new_str[i],line)
            f2.write(line)
    os.remove(file)
    os.rename("%s.bak" % file, file)

def isFileEqual( f1, f2, optionalFlags = "", outputPath = "", myinitfile = ""):
    LMYD = os.path.abspath(os.path.dirname(__file__))
    if not os.access( f1, os.R_OK ):
        raise Exception( 'Error: cannot find file %s' % f1 )
    if not os.access( f2, os.R_OK ):
        raise Exception( 'Error: cannot find file %s' % f2 )
    dfile = diffFile( f1, outputPath = outputPath )
    # Gets the suitePath name to add init_file
    suitePath = f1[0:f1.rindex( "/" )]

    pat1 = r'["|//]\d+\.\d+\.\d+\.\d+'  # host ip 
    newpat1 = lambda x : x[0][0]+'*'
    pat2 = r'[a-zA-Z0-9/\_-]*/data_file'  # file location
    newpat2 = 'pathto/data_file'
    pat3 = r', SSL off$'
    newpat3 = ''
    alterOutFile(f2, [pat1,pat2,pat3], [newpat1,newpat2,newpat3])  # some strings in outfile are different each time, such as host and file location
    # we alter the out file here to make it match the ans file

    gphome = os.environ['GPHOME']
    if os.path.exists(suitePath + "/init_file"):
        (ok, out) = run(gphome+'/lib/postgresql/pgxs/src/test/regress/gpdiff.pl -w ' + optionalFlags + \
                              ' -I NOTICE: -I HINT: -I CONTEXT: -I GP_IGNORE: -I DROP --gp_init_file=%s/global_init_file --gp_init_file=%s/init_file '
                              '%s %s > %s 2>&1' % (LMYD, suitePath, f1, f2, dfile))

    else:
        if os.path.exists(myinitfile):
            (ok, out) = run(gphome+'/lib/postgresql/pgxs/src/test/regress/gpdiff.pl -w ' + optionalFlags + \
                                  ' -I NOTICE: -I HINT: -I CONTEXT: -I GP_IGNORE: -I DROP --gp_init_file=%s/global_init_file --gp_init_file=%s '
                                  '%s %s > %s 2>&1' % (LMYD, myinitfile, f1, f2, dfile))
        else:
            (ok, out) = run( gphome+'/lib/postgresql/pgxs/src/test/regress/gpdiff.pl -w ' + optionalFlags + \
                              ' -I NOTICE: -I HINT: -I CONTEXT: -I GP_IGNORE: -I DROP --gp_init_file=%s/global_init_file '
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
            print (str(re.sub('\n','',line)))

def copy_data(source='',target=''):
    cmd = 'cp '+ mkpath('data/' + source) + ' ' + mkpath(target)
    p = subprocess.Popen(cmd,shell=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    return p.communicate()


def get_table_name():
    try:
        db = pg.DB(dbname='reuse_gptest'
                  ,host='localhost'
                  ,port=int(PGPORT)
                  )
    except Exception as e:
        errorMessage = str(e)
        print ('could not connect to database: ' + errorMessage)
    queryString = """SELECT relname
                     from pg_class
                     WHERE relname
                     like 'ext_gpload_reusable%'
                     OR relname
                     like 'staging_gpload_reusable%';"""
    resultList = db.query(queryString.encode('utf-8')).getresult()
    return resultList

def drop_tables():
    '''drop external and staging tables'''
    try:
        db = pg.DB(dbname='reuse_gptest'
                  ,host='localhost'
                  ,port=int(PGPORT)
                  )
    except Exception as e:
        errorMessage = str(e)
        print ('could not connect to database: ' + errorMessage)

    tableList = get_table_name()
    for i in tableList:
        name = i[0]
        match = re.search('ext_gpload',name)
        if match:
            queryString = "DROP EXTERNAL TABLE %s;" % name
            db.query(queryString.encode('utf-8'))

        else:
            queryString = "DROP TABLE %s;" % name
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

class AnsFile():
    def __init__(self, path):
        self.path = path
    def __eq__(self, other):
        return isFileEqual(self.path, other.path, '-U3', outputPath="")

def check_result(ifile,  optionalFlags = "-U3", outputPath = "", num=None):
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
    f1 = AnsFile(f1)
    f2 = outFile(ifile, outputPath=outputPath)
    f2 = AnsFile(f2)
    assert f1 == f2 #, read_diff(ifile, "")
    if num==54:
        assert f1==AnsFile(mkpath('54tmp.log'))
    return True

def doTest(num):
    file = mkpath('query%d.diff' % num)
    if os.path.isfile(file):
        run("rm -f" + " " + file)
    modify_sql_file(num)
    file = mkpath('query%d.sql' % num)
    runfile(file)
    check_result(file,num=num)

def write_test_file(num,cmd='',times=2):
    """
    initialize specific query#.sql for test case n

    num: number for case
    cmd: add sth after default gpload command
    times: how many times to write gpload command, dufault is 2
    """
    f = open(mkpath('query%d.sql' % num),'w')
    while times>0:
        f.write("\\! gpload -f "+mkpath('config/config_file')+ " " + cmd + "\n")
        times-=1
    f.close()


from functools import wraps

def prepare_before_test(num,cmd='',times=2):
    def prepare_decorator(func):
        @wraps(func)
        def wrapped_function(*args, **kwargs):
            write_test_file(num,cmd,times)
            retval= func(*args, **kwargs)
            doTest(num)
            return retval
        return wrapped_function
    return prepare_decorator



def test_00_gpload_formatOpts_setup():
    "0  gpload setup"
    """setup database"""
    file = mkpath('setup.sql')
    runfile(file)
    check_result(file)

@prepare_before_test(num=1)
def test_01_gpload_formatOpts_delimiter():
    "1  gpload formatOpts delimiter '|' with reuse "
    copy_data('external_file_01.txt','data_file.txt')
    write_config_file(reuse_tables=True,format='text',file='data_file.txt',table='texttable',delimiter="'|'")

@prepare_before_test(num=2)
def test_02_gpload_formatOpts_delimiter():
    "2  gpload formatOpts delimiter '\t' with reuse"
    copy_data('external_file_02.txt','data_file.txt')
    write_config_file(reuse_tables=True, format='text', file='data_file.txt',delimiter="'\\t'")

@prepare_before_test(num=3)
def test_03_gpload_formatOpts_delimiter():
    "3  gpload formatOpts delimiter E'\t' with reuse"
    copy_data('external_file_02.txt','data_file.txt')
    write_config_file(reuse_tables=True,format='text',file='data_file.txt',table='texttable',delimiter="E'\\t'")

@prepare_before_test(num=4)
def test_04_gpload_formatOpts_delimiter():
    "4  gpload formatOpts delimiter E'\u0009' with reuse"
    copy_data('external_file_02.txt','data_file.txt')
    write_config_file(reuse_tables=True,format='text',file='data_file.txt',table='texttable',delimiter=r"E'\u0009'")

@prepare_before_test(num=5)
def test_05_gpload_formatOpts_delimiter():
    "5  gpload formatOpts delimiter E'\\'' with reuse"
    copy_data('external_file_03.txt','data_file.txt')
    write_config_file(reuse_tables=True,format='text',file='data_file.txt',table='texttable',delimiter="E'\''")

@prepare_before_test(num=6)
def test_06_gpload_formatOpts_delimiter():
    "6  gpload formatOpts delimiter \"'\" with reuse"
    copy_data('external_file_03.txt','data_file.txt')
    write_config_file(reuse_tables=True,format='text',file='data_file.txt',table='texttable',delimiter="\"'\"")

@prepare_before_test(num=7)
def test_07_gpload_reuse_table_insert_mode_without_reuse():
    "7  gpload insert mode without reuse"
    runfile(mkpath('setup.sql'))
    f = open(mkpath('query7.sql'),'a')
    f.write("\\! psql -d reuse_gptest -c 'select count(*) from texttable;'")
    f.close()
    write_config_file(mode='insert',reuse_tables=False)

@prepare_before_test(num=8)
def test_08_gpload_reuse_table_update_mode_with_reuse():
    "8  gpload update mode with reuse"
    drop_tables()
    copy_data('external_file_04.txt','data_file.txt')
    write_config_file(mode='update',reuse_tables=True,file='data_file.txt')

@prepare_before_test(num=9)
def test_09_gpload_reuse_table_update_mode_without_reuse():
    "9  gpload update mode without reuse"
    f = open(mkpath('query9.sql'),'a')
    f.write("\\! psql -d reuse_gptest -c 'select count(*) from texttable;'\n"+"\\! psql -d reuse_gptest -c 'select * from texttable where n2=222;'")
    f.close()
    copy_data('external_file_05.txt','data_file.txt')
    write_config_file(mode='update',reuse_tables=False,file='data_file.txt')

@prepare_before_test(num=10)
def test_10_gpload_reuse_table_merge_mode_with_reuse():
    "10  gpload merge mode with reuse "
    drop_tables()
    copy_data('external_file_06.txt','data_file.txt')
    write_config_file(mode='merge', reuse_tables=True, file='data_file.txt')

@prepare_before_test(num=11)
def test_11_gpload_reuse_table_merge_mode_without_reuse():
    "11  gpload merge mode without reuse "
    copy_data('external_file_07.txt','data_file.txt')
    write_config_file(mode='merge', reuse_tables=False, file='data_file.txt')

@prepare_before_test(num=12)
def test_12_gpload_reuse_table_merge_mode_with_different_columns_number_in_file():
    "12 gpload merge mode with reuse (RERUN with different columns number in file) "
    psql_run(cmd="ALTER TABLE texttable ADD column n8 text",dbname='reuse_gptest')
    copy_data('external_file_08.txt','data_file.txt')
    write_config_file(mode='merge', reuse_tables=True, file='data_file.txt')

@prepare_before_test(num=13)
def test_13_gpload_reuse_table_merge_mode_with_different_columns_number_in_DB():
    "13  gpload merge mode with reuse (RERUN with different columns number in DB table) "
    preTest = mkpath('pre_test_13.sql')
    psql_run(preTest, dbname='reuse_gptest')
    copy_data('external_file_09.txt','data_file.txt')
    write_config_file(mode='merge', reuse_tables=True, file='data_file.txt')

@prepare_before_test(num=14)
def test_14_gpload_reuse_table_update_mode_with_reuse_RERUN():
    "14 gpload update mode with reuse (RERUN) "
    write_config_file(mode='update', reuse_tables=True, file='data_file.txt')

@prepare_before_test(num=15)
def test_15_gpload_reuse_table_merge_mode_with_different_columns_order():
    "15 gpload merge mode with different columns' order "
    copy_data('external_file_10.txt','data/data_file.tbl')
    input_columns = {'s_s1':'text',
        's_s2':'text',
        's_dt':'timestamp',
        's_s3':'text',
        's_n1':'smallint',
        's_n2':'integer',
        's_n3':'bigint',
        's_n4':'decimal',
        's_n5':'numeric',
        's_n6':'real',
        's_n7':'double precision',
        's_n8':'text',
        's_n9':'text'}
    output_mapping =  {"s1": "s_s1", "s2": "s_s2",  "dt": "s_dt", "s3": "s_s3", "n1": "s_n1",
    "n2": "s_n2", "n3": "s_n3", "n4": "s_n4", "n5": "s_n5", "n6": "s_n6", "n7": "s_n7", "n8": "s_n8", "n9": "s_n9"}
    write_config_file(mode='merge', reuse_tables=True, file='data/data_file.tbl',columns=input_columns,mapping=output_mapping)

@prepare_before_test(num=16)
def test_16_gpload_formatOpts_quote():
    "16  gpload formatOpts quote unspecified in CSV with reuse "
    copy_data('external_file_11.csv','data_file.csv')
    write_config_file(reuse_tables=True, format='csv', file='data_file.csv', table='csvtable',delimiter="','")

@prepare_before_test(num=17)
def test_17_gpload_formatOpts_quote():
    "17  gpload formatOpts quote '\\x26'(&) with reuse"
    copy_data('external_file_12.csv','data_file.csv')
    write_config_file(reuse_tables=True, format='csv', file='data_file.csv', table='csvtable', delimiter="','", quote=r"'\x26'")

@prepare_before_test(num=18)
def test_18_gpload_formatOpts_quote():
    "18  gpload formatOpts quote E'\\x26'(&) with reuse"
    copy_data('external_file_12.csv','data_file.csv')
    write_config_file(reuse_tables=True, format='csv', file='data_file.csv', table='csvtable', delimiter="','", quote="E'\x26'")

@prepare_before_test(num=19)
def test_19_gpload_formatOpts_escape():
    "19  gpload formatOpts escape '\\' with reuse"
    copy_data('external_file_01.txt','data_file.txt')
    file = mkpath('setup.sql')
    runfile(file)
    write_config_file(reuse_tables=True, format='text', file='data_file.txt',table='texttable',escape="'\\'")

@prepare_before_test(num=20)
def test_20_gpload_formatOpts_escape():
    "20  gpload formatOpts escape '\\' with reuse"
    copy_data('external_file_01.txt','data_file.txt')
    write_config_file(reuse_tables=True, format='text', file='data_file.txt',table='texttable',escape="'\x5C'")

@prepare_before_test(num=21)
def test_21_gpload_formatOpts_escape():
    "21  gpload formatOpts escape E'\\\\' with reuse"
    copy_data('external_file_01.txt','data_file.txt')
    write_config_file(reuse_tables=True, format='text', file='data_file.txt',table='texttable',escape="E'\\\\'")

# case 22 is flaky on concourse. It may report: Fatal Python error: GC object already tracked during testing.
# This is seldom issue. we can't reproduce it locally, so we disable it, in order to not blocking others
#def test_22_gpload_error_count(self):
#    "22  gpload error count"
#    f = open(mkpath('query22.sql'),'a')
#    f.write("\\! psql -d reuse_gptest -c 'select count(*) from csvtable;'")
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

@prepare_before_test(num=23)
def test_23_gpload_error_count():
    "23  gpload error_table"
    file = mkpath('setup.sql')
    runfile(file)
    f = open(mkpath('query23.sql'),'a')
    f.write("\\! psql -d reuse_gptest -c 'select count(*) from csvtable;'")
    f.close()
    f = open(mkpath('data/large_file.csv'),'w')
    for i in range(0, 10000):
        if i % 2 == 0:
            f.write('1997,Ford,E350,"ac, abs, moon",3000.00,a\n')
        else:
            f.write('1997,Ford,E350,"ac, abs, moon",3000.00\n')
    f.close()
    copy_data('large_file.csv','data_file.csv')
    write_config_file(reuse_tables=True, format='csv', file='data_file.csv',table='csvtable', delimiter="','", error_table="err_table", error_limit=90000000)

@prepare_before_test(num=24)
def test_24_gpload_error_count():
    "24  gpload error count with ext schema"
    file = mkpath('setup.sql')
    runfile(file)
    f = open(mkpath('query24.sql'),'a')
    f.write("\\! psql -d reuse_gptest -c 'select count(*) from csvtable;'")
    f.close()
    f = open(mkpath('data/large_file.csv'),'w')
    for i in range(0, 10000):
        if i % 2 == 0:
            f.write('1997,Ford,E350,"ac, abs, moon",3000.00,a\n')
        else:
            f.write('1997,Ford,E350,"ac, abs, moon",3000.00\n')
    f.close()
    copy_data('large_file.csv','data_file.csv')
    write_config_file(reuse_tables=True, format='csv', file='data_file.csv',table='csvtable', delimiter="','", log_errors=True, error_limit=90000000, externalSchema='test')

@prepare_before_test(num=25)
def test_25_gpload_ext_staging_table():
    "25  gpload reuse ext_staging_table if it is configured"
    file = mkpath('setup.sql')
    runfile(file)
    f = open(mkpath('query25.sql'),'a')
    f.write("\\! psql -d reuse_gptest -c 'select count(*) from csvtable;'")
    f.close()
    copy_data('external_file_13.csv','data_file.csv')
    write_config_file(reuse_tables=True, format='csv', file='data_file.csv', table='csvtable', delimiter="','", log_errors=True,error_limit=10,staging_table='staging_table')

@prepare_before_test(num=26)
def test_26_gpload_ext_staging_table_with_externalschema():
    "26  gpload reuse ext_staging_table if it is configured with externalschema"
    file = mkpath('setup.sql')
    runfile(file)
    f = open(mkpath('query26.sql'),'a')
    f.write("\\! psql -d reuse_gptest -c 'select count(*) from csvtable;'")
    f.close()
    copy_data('external_file_13.csv','data_file.csv')
    write_config_file(reuse_tables=True, format='csv', file='data_file.csv', table='csvtable', delimiter="','",log_errors=True,error_limit=10, staging_table='staging_table',externalSchema='test')

@prepare_before_test(num=27)
def test_27_gpload_ext_staging_table_with_externalschema():
    "27  gpload reuse ext_staging_table if it is configured with externalschema"
    file = mkpath('setup.sql')
    runfile(file)
    f = open(mkpath('query27.sql'),'a')
    f.write("\\! psql -d reuse_gptest -c 'select count(*) from test.csvtable;'")
    f.close()
    copy_data('external_file_13.csv','data_file.csv')
    write_config_file(reuse_tables=True, format='csv', file='data_file.csv',table='test.csvtable',delimiter="','",log_errors=True,error_limit=10,staging_table='staging_table',externalSchema="'%'")

@prepare_before_test(num=28)
def test_28_gpload_ext_staging_table_with_dot():
    "28  gpload reuse ext_staging_table if it is configured with dot"
    file = mkpath('setup.sql')
    runfile(file)
    f = open(mkpath('query28.sql'),'a')
    f.write("\\! psql -d reuse_gptest -c 'select count(*) from test.csvtable;'")
    f.close()
    copy_data('external_file_13.csv','data_file.csv')
    write_config_file(reuse_tables=True, file='data_file.csv',table='test.csvtable',format='csv',delimiter="','",log_errors=True,error_limit=10,staging_table='t.staging_table')

@prepare_before_test(num=29)
def test_29_gpload_reuse_table_insert_mode_with_reuse_and_null():
    "29  gpload insert mode with reuse and null"
    runfile(mkpath('setup.sql'))
    f = open(mkpath('query29.sql'),'a')
    f.write("\\! psql -d reuse_gptest -c 'select count(*) from texttable where n2 is null;'")
    f.close()
    copy_data('external_file_14.txt','data_file.txt')
    write_config_file(mode='insert', reuse_tables=True, file='data_file.txt',log_errors=True, error_limit=100)

@prepare_before_test(num=30)
def test_30_gpload_reuse_table_update_mode_with_fast_match():
    "30  gpload update mode with fast match"
    drop_tables()
    copy_data('external_file_04.txt','data_file.txt')
    write_config_file(mode='update',reuse_tables=True,fast_match=True,file='data_file.txt')

@prepare_before_test(num=31)
def test_31_gpload_reuse_table_update_mode_with_fast_match_and_different_columns_number():
    "31 gpload update mode with fast match and differenct columns number) "
    psql_run(cmd="ALTER TABLE texttable ADD column n8 text",dbname='reuse_gptest')
    copy_data('external_file_08.txt','data_file.txt')
    write_config_file(mode='update',reuse_tables=True,fast_match=True,file='data_file.txt')

@prepare_before_test(num=32)
def test_32_gpload_update_mode_without_reuse_table_with_fast_match():
    "32  gpload update mode when reuse table is false and fast match is true"
    drop_tables()
    copy_data('external_file_08.txt','data_file.txt')
    write_config_file(mode='update',reuse_tables=False,fast_match=True,file='data_file.txt')

@prepare_before_test(num=33)
def test_33_gpload_reuse_table_merge_mode_with_fast_match_and_external_schema():
    "33  gpload update mode with fast match and external schema"
    file = mkpath('setup.sql')
    runfile(file)
    copy_data('external_file_04.txt','data_file.txt')
    write_config_file(mode='merge',reuse_tables=True,fast_match=True,file='data_file.txt',externalSchema='test')

@prepare_before_test(num=34)
def test_34_gpload_reuse_table_merge_mode_with_fast_match_and_encoding():
    "34  gpload merge mode with fast match and encoding GBK"
    file = mkpath('setup.sql')
    runfile(file)
    copy_data('external_file_04.txt','data_file.txt')
    write_config_file(mode='merge',reuse_tables=True,fast_match=True,file='data_file.txt',encoding='GBK')

@prepare_before_test(num=35)
def test_35_gpload_reuse_table_merge_mode_with_fast_match_default_encoding():
    "35  gpload does not reuse table when encoding is setted from GBK to empty"
    write_config_file(mode='merge',reuse_tables=True,fast_match=True,file='data_file.txt')

@prepare_before_test(num=36)
def test_36_gpload_reuse_table_merge_mode_default_encoding():
    "36  gpload merge mode with encoding GBK"
    file = mkpath('setup.sql')
    runfile(file)
    copy_data('external_file_04.txt','data_file.txt')
    write_config_file(mode='merge',reuse_tables=True,fast_match=False,file='data_file.txt',encoding='GBK')

@prepare_before_test(num=37)
def test_37_gpload_reuse_table_merge_mode_invalid_encoding():
    "37  gpload merge mode with invalid encoding"
    file = mkpath('setup.sql')
    runfile(file)
    copy_data('external_file_04.txt','data_file.txt')
    write_config_file(mode='merge',reuse_tables=True,fast_match=False,file='data_file.txt',encoding='xxxx')

@prepare_before_test(num=38)
def test_38_gpload_without_preload():
    "38  gpload insert mode without preload"
    file = mkpath('setup.sql')
    runfile(file)
    copy_data('external_file_04.txt','data_file.txt')
    write_config_file(mode='insert',reuse_tables=True,fast_match=False,file='data_file.txt',error_table="err_table",error_limit=1000,preload=False)

@prepare_before_test(num=39)
def test_39_gpload_fill_missing_fields():
    "39  gpload fill missing fields"
    file = mkpath('setup.sql')
    runfile(file)
    copy_data('external_file_04.txt','data_file.txt')
    write_config_file(mode='insert',reuse_tables=False,fast_match=False,file='data_file.txt',table='texttable1', error_limit=1000, fill_missing_fields=True)

@prepare_before_test(num=40)
def test_40_gpload_merge_mode_with_multi_pk():
    "40  gpload merge mode with multiple pk"
    file = mkpath('setup.sql')
    runfile(file)
    copy_data('external_file_pk.txt','data_file.txt')
    write_config_file(mode='merge',reuse_tables=True,fast_match=False,file='data_file.txt',table='testpk')
    #write_config_file(mode='merge',reuse_flag='true',fast_match='false',file='data_file.txt',table='testpk')
    copy_data('external_file_pk2.txt','data_file2.txt')
    write_config_file(mode='merge',reuse_tables=True,fast_match=False,file='data_file2.txt',table='testpk',config='config/config_file2')
    # write_config_file(mode='merge',reuse_flag='true',fast_match='false',file='data_file2.txt',table='testpk',config='config/config_file2')
    f = open(mkpath('query40.sql'),'w')
    f.write("""\\! psql -d reuse_gptest -c "create table testpk (n1 integer, s1 integer, s2 varchar(128), n2 integer, primary key(n1,s1,s2))\
            partition by range (s1)\
            subpartition by list(s2)\
            SUBPARTITION TEMPLATE\
            ( SUBPARTITION usa VALUES ('usa'),\
                    SUBPARTITION asia VALUES ('asia'),\
                    SUBPARTITION europe VALUES ('europe'),\
                    DEFAULT SUBPARTITION other_regions)\
            (start (1) end (13) every (1),\
            default partition others)\
            ;"\n""")
    f.write("\\! gpload -f "+mkpath('config/config_file')+ " -d reuse_gptest\n")
    f.write("\\! gpload -f "+mkpath('config/config_file2')+ " -d reuse_gptest\n")
    f.write("\\! psql -d reuse_gptest -c 'drop table testpk;'\n")
    f.close()

@prepare_before_test(num=41)
def test_41_gpload_special_char():
    "41 gpload special char"
    copy_data('external_file_15.txt','data_file.txt')
    columns = {"'\"Field1\"'": 'bigint',"'\"Field#2\"'": 'text'}
    write_config_file(mode='insert',reuse_tables=True,fast_match=False, file='data_file.txt',table='testSpecialChar', columns=columns,delimiter="';'")
    copy_data('external_file_16.txt','data_file2.txt')
    update_columns=["'\"Field#2\"'"]
    match_columns = ["'\"Field1\"'", "'\"Field#2\"'"]
    write_config_file(update_columns=update_columns ,config='config/config_file2', mode='merge',reuse_tables=True,fast_match=False, file='data_file2.txt',table='testSpecialChar',columns=columns, delimiter="';'",match_columns=match_columns)
    f = open(mkpath('query41.sql'),'a')
    f.write("\\! gpload -f "+mkpath('config/config_file2')+ " -d reuse_gptest\n")
    f.close()

@prepare_before_test(num=42)
def test_42_gpload_update_condition():
    "42 gpload update condition"
    copy_data('external_file_01.txt','data_file.txt')
    copy_data('external_file_03.txt','data_file2.txt')
    f = open(mkpath('query42.sql'),'w')
    f.write("\\! gpload -f "+mkpath('config/config_file')+ " -d reuse_gptest\n")
    f.write("\\! gpload -f "+mkpath('config/config_file2')+ " -d reuse_gptest\n")
    f.close()
    write_config_file(mode='insert', format='text',file='data_file.txt',table='texttable')
    update_columns = ['s2']
    match_columns = ['n1']
    write_config_file(update_columns=update_columns,match_columns=match_columns,config='config/config_file2',mode='update', update_condition="s3='shpits'", format='text',file='data_file2.txt',table='texttable',delimiter="\"'\"")

@prepare_before_test(num=43)
def test_43_gpload_column_without_data_type():
    "43 gpload column name has capital letters and without data type"
    copy_data('external_file_15.txt','data_file.txt')
    columns = {"'\"Field1\"'": '',"'\"Field#2\"'": ''}
    write_config_file(mode='insert',reuse_tables=True,fast_match=False, file='data_file.txt',table='testSpecialChar',columns=columns, delimiter="';'")

@prepare_before_test(num=44, cmd="-h "+str(hostNameAddrs))
def test_44_gpload_config_h():
    "44 gpload command config test -h hostname"
    copy_data('external_file_01.txt', 'data_file.txt')
    write_config_file( host="",format='text',file='data_file.txt',table='texttable',delimiter="'|'")

@prepare_before_test(num=45, cmd="-p "+str(masterPort))
def test_45_gpload_config_p():
    "45 gpload command config test -p port"
    copy_data('external_file_01.txt', 'data_file.txt')
    write_config_file(port="", format='text',file='data_file.txt',table='texttable')

@prepare_before_test(num=46, cmd="-p 9999")
def test_46_gpload_config_wrong_p():
    "46 gpload command config test -p port"
    copy_data('external_file_01.txt', 'data_file.txt')
    write_config_file(port="", format='text',file='data_file.txt',table='texttable')

''' this case runs extreamly slowly, comment it here
@prepare_before_test(num=47, cmd="-h 1.2.3.4")
def test_47_gpload_config_wrong_h():
    "47 gpload command config test -h hostname"
    copy_data('external_file_01.txt', 'data_file.txt')
    write_config_file( host="",format='text',file='data_file.txt',table='texttable',delimiter="'|'")
'''

@prepare_before_test(num=48, cmd="-d reuse_gptest")
def test_48_gpload_config_d():
    "48 gpload command config test -d database"
    copy_data('external_file_01.txt', 'data_file.txt')
    write_config_file(database="", format='text',file='data_file.txt',table='texttable')

@prepare_before_test(num=49, cmd="-d notexistdb")
def test_49_gpload_config_wrong_d():
    "49 gpload command config test -d with wrong database"
    copy_data('external_file_01.txt', 'data_file.txt')
    write_config_file(database="", format='text',file='data_file.txt',table='texttable')

@prepare_before_test(num=50, cmd="-U gpadmin")
def test_50_gpload_config_U():
    "50 gpload command config test -U username"
    copy_data('external_file_01.txt', 'data_file.txt')
    write_config_file(user="", format='text',file='data_file.txt',table='texttable')

@prepare_before_test(num=51, cmd="-U notexistusr")
def test_51_gpload_config_wrong_U():
    "51 gpload command config test -U wrong username"
    copy_data('external_file_01.txt', 'data_file.txt')
    write_config_file(user="", format='text',file='data_file.txt',table='texttable')

@prepare_before_test(num=52, cmd='--gpfdist_timeout 2')
def test_52_gpload_config_gpfdist_timeout():
    "52 gpload command config test gpfdist_timeout"
    drop_tables()
    copy_data('external_file_01.txt','data_file.txt')
    write_config_file(format='text',file='data_file.txt',table='texttable')

'''  maybe some bug in gpfdist
@prepare_before_test(num=53, cmd='--gpfdist_timeout aa')
def test_53_gpload_config_gpfdist_timeout_wrong():
    "53 gpload command config test gpfdist_timeout with a string"
    runfile(mkpath('setup.sql'))
    copy_data('external_file_01.txt','data_file.txt')
    write_config_file(format='text',file='data_file.txt',table='texttable')
'''

@prepare_before_test(num=54, cmd='-l 54tmp.log')
def test_54_gpload_config_l():
    "54 gpload command config test -l logfile"
    run('rm 54tmp.log')
    copy_data('external_file_01.txt','data_file.txt')
    write_config_file(format='text',file='data_file.txt',table='texttable')

@prepare_before_test(num=55)
def test_55_gpload_yaml_version():
    "55 gpload yaml version"
    copy_data('external_file_01.txt','data_file.txt')
    write_config_file(version='1.0.0.2',format='text',file='data_file.txt',table='texttable')

@prepare_before_test(num=56)
def test_56_gpload_yaml_wrong_database():
    "56 gpload yaml writing a not exist database"
    copy_data('external_file_01.txt','data_file.txt')
    write_config_file(database='notexist',format='text',file='data_file.txt',table='texttable')

@prepare_before_test(num=57)
def test_57_gpload_yaml_wrong_user():
    "57 gpload yaml writing a not exist user"
    copy_data('external_file_01.txt','data_file.txt')
    write_config_file(user='notexist',format='text',file='data_file.txt',table='texttable')
''' wrong host runs slowly
@prepare_before_test(num=58)
def test_58_gpload_yaml_wrong_host():
    "58 gpload yaml writing a not exist host"
    copy_data('external_file_01.txt','data_file.txt')
    write_config_file(host='1.2.3.4',format='text',file='data_file.txt',table='texttable')
'''
@prepare_before_test(num=59)
def test_59_gpload_yaml_wrong_port():
    "59 gpload yaml writing a not exist port"
    copy_data('external_file_01.txt','data_file.txt')
    write_config_file(port='111111',format='text',file='data_file.txt',table='texttable')

@prepare_before_test(num=60)
def test_60_gpload_local_hostname():
    "60 gpload yaml local host with 127.0.0.1 and none and a not exist host"
    copy_data('external_file_01.txt','data_file.txt')
    write_config_file(local_host=['127.0.0.1'],format='text',file='data_file.txt',table='texttable')
    write_config_file(config='config/config_file2',local_host=None,format='text',file='data_file.txt',table='texttable')
    write_config_file(config='config/config_file3',local_host=['123.123.1.1'],format='text',file='data_file.txt',table='texttable')
    f = open('query60.sql','w')
    f.write("\\! gpload -f "+mkpath('config/config_file')+"\n")
    f.write("\\! gpload -f "+mkpath('config/config_file2')+"\n")
    f.write("\\! gpload -f "+mkpath('config/config_file3')+"\n")
    f.close()

