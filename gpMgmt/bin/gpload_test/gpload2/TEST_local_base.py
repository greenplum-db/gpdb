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

from gppylib.commands.gp import get_coordinatordatadir

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
    file = get_coordinatordatadir()+'/postgresql.conf'
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

def getPortCoordinatorOnly(host = 'localhost',coordinator_value = None,
                      user = os.environ.get('USER'),gphome = os.environ['GPHOME'],
                      cdd=get_coordinatordatadir(),port = os.environ['PGPORT']):

    coordinator_pattern = r"Context:\s*-1\s*Value:\s*\d+"
    command = "gpconfig -s %s" % ( "port" )

    cmd = "source %s/greenplum_path.sh; export COORDINATOR_DATA_DIRECTORY=%s; export PGPORT=%s; %s" \
           % (gphome, cdd, port, command)

    (ok,out) = run(cmd)
    if not ok:
        cmd = "python3 %s/bin/gpconfig -s port"%(gphome)
        (ok,out) = run(cmd)
        if not ok:
            raise Exception("Unable to connect to segment server %s as user %s" % (host, user))

    for line in out:
        out = line.decode().split('\n')
    for line in out:
        if re.search(coordinator_pattern, line):
            coordinator_value = int(line.split()[3].strip())

    if coordinator_value is None:
        error_msg = "".join(out)
        raise Exception(error_msg)

    return str(coordinator_value)


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
if PGHOST is None:
    PGHOST = HOST

d = mkpath('config')
if not os.path.exists(d):
    os.mkdir(d)

hostNameAddrs = get_ip(HOST)
coordinatorPort = getPortCoordinatorOnly()

def write_config_file(version='1.0.0.1', database='reuse_gptest', user=os.environ.get('USER'), host=hostNameAddrs, port=coordinatorPort, config='config/config_file', local_host=[hostNameAddrs], file='data/external_file_01.txt', input_port='8081', port_range=None,
    ssl=None,columns=None, format='text', force_not_null=[], log_errors=None, error_limit=None, delimiter="'|'", encoding=None, escape=None, null_as=None, fill_missing_fields=None, quote=None, header=None, transform=None, transform_config=None, max_line_length=None, 
    table='texttable', mode='insert', update_columns=['n2'], update_condition=None, match_columns=['n1','s1','s2'], staging_table=None, mapping=None, externalSchema=None, preload=True, truncate=False, reuse_tables=True, fast_match=None,
    sql=False, before=None, after=None, error_table=None, newline=None):

    f = open(config,'w', encoding="utf-8")
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
    if delimiter:
        f.write("\n    - DELIMITER: "+delimiter)
    if newline:
        f.write("\n    - NEWLINE: "+str(newline))
    if encoding:
        f.write("\n    - ENCODING: "+encoding)
    if escape:
        f.write("\n    - ESCAPE: "+escape)
    if null_as:
        f.write("\n    - NULL_AS: "+null_as)
    if force_not_null:
        f.write("\n    - FORCE_NOT_NULL:")
    for c in force_not_null:
        f.write("\n           - "+c)
    if fill_missing_fields:
        f.write("\n    - FILL_MISSING_FIELDS: "+str(fill_missing_fields))
    if quote:
        f.write("\n    - QUOTE: "+quote)
    if header != None:
        f.write("\n    - HEADER: "+str(header))
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
            f.write("\n    - TRUNCATE: "+str(truncate))
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
    f.write("\n")
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

def isFileEqual( f1, f2, optionalFlags = "", outputPath = "", myinitfile = ""):
    LMYD = os.path.abspath(os.path.dirname(__file__))
    if not os.access( f1, os.R_OK ):
        raise Exception( 'Error: cannot find file %s' % f1 )
    if not os.access( f2, os.R_OK ):
        raise Exception( 'Error: cannot find file %s' % f2 )
    dfile = diffFile( f1, outputPath = outputPath )
    # Gets the suitePath name to add init_file
    suitePath = f1[0:f1.rindex( "/" )]

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
    queryString = """SELECT sch.table_schema, cls.relname
                     FROM pg_class AS cls, information_schema.tables AS sch
                     WHERE
                     (cls.relname LIKE 'ext_gpload_reusable%'
                     OR
                     relname LIKE 'staging_gpload_reusable%')
                     AND cls.relname=sch.table_name;"""
    resultList = db.query(queryString.encode('utf-8')).getresult()
    print(resultList)
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
        schema = i[0]
        name = i[1]
        match = re.search('ext_gpload',name)
        if match:
            queryString = 'DROP EXTERNAL TABLE "%s"."%s";'%(schema, name)
            db.query(queryString.encode('utf-8'))

        else:
            queryString = 'DROP TABLE "%s"."%s";'%(schema, name)
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


def do_assert(f1, f2, num, ifile):
    # this will help print the diff message in the screen if case fail
    assert f1 == f2 , read_diff(ifile, "")
    if num==54:
        assert f1==AnsFile(mkpath('54tmp.log'))
    return True


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
    do_assert(f1, f2, num, ifile)


def ModifyOutFile(file,old_str,new_str):
    file = 'query'+file+'.out'
    with open(file, "r", encoding="utf-8") as f1,open("%s.bak" % file, "w", encoding="utf-8") as f2:
        for line in f1:
            for i in range(len(old_str)):
                line = re.sub(old_str[i],new_str[i],line)
            f2.write(line)
    os.remove(file)
    os.rename("%s.bak" % file, file)

Modify_Output_Case = [46,51,57,65,76,260,402]


def doTest(num):
    file = mkpath('query%d.diff' % num)
    if os.path.isfile(file):
        run("rm -f" + " " + file)
    modify_sql_file(num)
    file = mkpath('query%d.sql' % num)
    runfile(file)

    if num in Modify_Output_Case:  # some cases need to modify output file to avoid compareing fail with ans file
        pat1 = r'["|//]\d+\.\d+\.\d+\.\d+'  # host ip 
        newpat1 = lambda x : x[0][0]+'*'
        pat2 = r'[a-zA-Z0-9/\_-]*/data_file'  # file location
        newpat2 = 'pathto/data_file'
        pat3 = r', SSL off$'
        newpat3 = ''
        pat4 = r'LINE 1: ...[a-zA-Z0-9\_]*\('
        newpat4 = 'LINE 1: ...('
        ModifyOutFile(str(num), [pat1,pat2,pat3,pat4], [newpat1,newpat2,newpat3,newpat4])  # some strings in outfile are different each time, such as host and file location
        # we modify the out file here to make it match the ans file

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


def prepare_before_test_2(num):
    """ Similar to the prepare_before_test function, but this won't write to
        the test sql file. This gives more freedom to the test case to handle
        the test sql file.
    """
    def prepare_decorator(func):
        @wraps(func)
        def wrapped_function(*args, **kwargs):
            # Clear the file
            f = open(mkpath('query%d.sql' % num), 'w')
            f.close()
            retval = func(*args, **kwargs)
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
