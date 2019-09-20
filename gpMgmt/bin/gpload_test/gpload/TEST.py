#!/usr/bin/env python

import unittest
import sys
import os
import string
import time
import datetime
import socket
import fileinput
import platform
import re
try:
    import subprocess32 as subprocess
except:
    import subprocess
from pygresql import pg

"""
Global Values
"""
DBNAME = "postgres"
USER = os.environ.get( "LOGNAME" )
HOST = socket.gethostname()
GPHOME = os.getenv("GPHOME")
PGUSER = os.environ.get("PGUSER")
if PGUSER is None:
    PGUSER = USER
PGHOST = os.environ.get("PGHOST")
if PGHOST is None:
    PGHOST = HOST
MYD = os.path.abspath(os.path.dirname(__file__))
mkpath = lambda *x: os.path.join(MYD, *x)
UPD = os.path.abspath(mkpath('..'))
dataPath = MYD + "/data"
configPath = MYD + "/config"
if not os.path.exists(configPath):
    os.mkdir(configPath)
if UPD not in sys.path:
    sys.path.append(UPD)

if platform.system() in ['Windows', 'Microsoft']:
    remove_command = "del"
    copy_command = 'copy'
    rename_command = 'rename'
    gpload_command = 'gpload.py'
else:
    remove_command = "rm -f"
    copy_command = 'cp'
    rename_command = 'mv'
    gpload_command = 'gpload'

def initialize_env_vars(name):
    if(name == 'username'):
        os.environ['PGUSER'] = ''
    if(name == 'port'):
        os.environ['PGPORT'] = ''
    if(name == 'host'):
        os.environ['PGHOST'] = ''
    if(name == 'database'):
        os.environ['PGDATABASE'] = ''
    if(name == 'password'):
        os.environ['PGPASSWORD'] = ''
        os.environ['PGPASSFILE'] = ''

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

def write_config_file(database='gptest',user='',host='localhost',port='',table='lineitem',file='lineitem.tbl.small'):
    if (not user or user == '') and (not os.environ.get('PGUSER') or os.environ.get('PGUSER') == ''):
        user = os.environ.get('USER')

    f = open(configPath + '/config_file','w')

    f.write("version:     1.0.0.1")
    if database:
        f.write("\ndatabase:  "+database)
    if user:
        f.write("\nuser:  "+user)
    if host:
        if host == 'localhost':
            f.write("\nhost:      "+get_hostname())
        else:
            f.write("\nhost:      "+host)
    if port:
        f.write("\nport:      "+port)

    f.write("\n\ngpload:")

    f.write("\n\n    input:")
    f.write("\n          - source:")
    f.write("\n              local_hostname:")
    f.write("\n                    -  "+get_hostname('true'))
    if file:
        f.write("\n              file: [ " + dataPath + "/" + file + " ]")
    f.write("\n\n          - delimiter: '|'")

    f.write("\n\n    output:")
    if table:
        f.write("\n          - table: "+table)
    f.write("\n")

    f.close()


def initialize():
    user = os.environ.get('USER')
    if not user:
        user = os.environ.get('USER')

    if not get_port():
        raise Exception('Could not get port')

    os.environ['PGPORT'] = get_port()
    os.environ['PGHOST'] = get_hostname()
    os.environ['PGDATABASE'] = 'gptest'

    if platform.system() not in ['Windows', 'Microsoft']:
        run("psql -U "+user+" -d "+DBNAME+" -h "+get_hostname()+" -p "+get_port()+" -f " + MYD + "/drop.sql 2>&1")
        run("psql -U "+user+" -d "+DBNAME+" -h "+get_hostname()+" -p "+get_port()+" -f " + MYD + "/create.sql 2>&1")
    else:
        run('psql -h '+get_hostname()+' -U '+os.environ.get('USER')+' -d '+DBNAME+' -p '+get_port()+' -f ' + MYD + '/drop.sql')
        run('psql -h '+get_hostname()+' -U '+os.environ.get('USER')+' -d '+DBNAME+' -p '+get_port()+' -f ' + MYD + '/create.sql')
    if os.path.isfile(configPath + "/config_file"):
        os.system(windows_path(remove_command + " " + configPath + "/config_file"))
    if os.path.isfile(MYD + "/log.log"):
        os.system(windows_path(remove_command+" " + MYD + "/log.log"))
    if os.path.isfile("*.diff"):
        os.system(windows_path(remove_command+" " + MYD + "/*.diff"))
    if platform.system() not in ['Windows', 'Microsoft']:
        os.system(remove_command+" /tmp/log.log")
        os.system(remove_command+" "+os.environ.get('HOME')+"/gpAdminLogs/gpload_"+datetime.date.today().strftime('%Y%m%d') + ".log")
        if os.path.isfile(os.environ['GPHOME']+"/docs/cli_help/gpload_help.bak"):
            os.system(rename_command+" "+os.environ['GPHOME']+"/docs/cli_help/gpload_help.bak "+os.environ['GPHOME']+"/docs/cli_help/gpload_help")
    if platform.system() in ['Windows', 'Microsoft']:
        if os.path.isfile("gpAdminLogs/gpload_"+datetime.date.today().strftime('%Y%m%d') + ".log"):
            os.system(windows_path(remove_command+" gpAdminLogs/gpload_"+datetime.date.today().strftime('%Y%m%d') + ".log"))


def get_hostname(local = ''):
    if platform.system() in ['Windows', 'Microsoft']:
        if local == 'true':
            return '0.0.0.0'
        else:
            #have to change to IP address of machine to load data into if running over the network
            return '0.0.0.0'
    else:
        if local == 'true':
            #have to change to IP address of local machine if running over the network
            hostname = socket.gethostname()
            address_list = socket.getaddrinfo(hostname,None)
            if len(address_list) > 0:
                ipv6_tuple_index = len(address_list) - 1
                return address_list[ipv6_tuple_index][4][0]
            else:
                return hostname
        else:
            #have to change to IP address of machine to load data into if running over the network
            hostname = socket.gethostname()
            # getaddrinfo(host,port) returns a list of tuples
            # in the form of (family, socktype, proto, canonname, sockaddr)
            # and we extract the IP address from 'sockaddr'
            address_list = socket.getaddrinfo(hostname,None)
            if len(address_list) > 0:
                ipv6_tuple_index = len(address_list) - 1
                return address_list[ipv6_tuple_index][4][0]
            else:
                return hostname

def modify_sql_file(num):
    file = MYD + '/query'+str(num)+'.sql'
    user = os.environ.get('USER')
    if not user:
        user = os.environ.get('USER')
    if os.path.isfile(file):
        for line in fileinput.FileInput(file,inplace=1):
            if platform.system() in ['Windows', 'Microsoft']:
                line = line.replace("\!gpload ","\!gpload.py")
                line = line.replace("gpload ","gpload.py ")
            else:
                line = line.replace("gpload.py ","gpload ")
            # using absolute path
            line = re.sub('-h WinnBook.local', '-h '+get_hostname(), line)
            line = line.replace("-h localhost",'-h '+get_hostname())
            line = re.sub('-p \d+', '-p '+get_port(), line)
            line = re.sub('-p$', '-p '+get_port(), line)
            line = re.sub('-h (\d+)\.(\d+)\.(\d+)\.(\d+)', '-h '+get_hostname(), line)
            if num == 12 or num == 13 or num == 14 or num == 15 or num == 176:
                line = re.sub('-U \w+', '-U fake_user', line)
            else:
                line = re.sub('-U \w+', '-U '+user, line)
            print str(re.sub('\n','',line))

def windows_path(command):
    if platform.system() in ['Windows', 'Microsoft']:
        string = ''
        for line in command:
            line = line.replace("/","\\")
            string += str(re.sub('\n','',line))
        return string
    else:
        return command

def get_port():
    file = os.environ.get('MASTER_DATA_DIRECTORY')+'/postgresql.conf'
    if os.path.isfile(file):
        f = open(file)
        for line in f.xreadlines():
            match = re.search('port=\d+',line)
            if match:
                match1 = re.search('\d+', match.group())
                if match1:
                    return match1.group()
        f.close()
        raise Exception('Could not get port from '+file)
    else:
        raise Exception(file+' does not exist.  Cannot get port.')

class GPLoad_Env_TestCase(unittest.TestCase):

    def setUp(self):
        initialize()
        write_config_file()

    def tearDown(self):
        os.environ['PGPORT'] = get_port()
        os.environ['PGHOST'] = get_hostname()
        os.environ['PGDATABASE'] = 'gptest'

    def doTest(self, num):
        file = MYD + '/query'+str(num)+'.diff'
        if os.path.isfile(file):
            run(windows_path(remove_command+" "+file))
        modify_sql_file(num)
        file = mkpath('query%d.sql' % num)
        (ok, out) = runfile(file)
        self.check_result(file)

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

    def testQuery00(self):
        "0  gpload setup"
        for num in range(1,6):
           f = open(mkpath('query%d.sql' % num),'w')
           f.write("\! gpload -f "+mkpath('config/config_file')+ " -d gptest")
           f.close()

    def testQuery01(self):
        "1  get username from environment variable 'USER'"
        initialize_env_vars('username')
        del os.environ['PGUSER']
        write_config_file('gptest','',get_hostname(),get_port())
        self.doTest(1)

    def testQuery02(self):
        "2  get username from environment variable 'PGUSER'"
        write_config_file('gptest','',get_hostname(),get_port())
        self.doTest(2)

    def testQuery03(self):
        "3  get username, host and port from environment"
        initialize_env_vars('username')
        del os.environ['PGUSER']
        self.doTest(3)

    def testQuery04(self):
        "4  specify port, user and hostname"
        os.environ['PGPORT'] = get_port()
        write_config_file('gptest',os.environ.get('USER'),get_hostname(),'')
        self.doTest(4)

    def testQuery05(self):
        "5  only specify port"
        os.environ['PGPORT'] = get_port()
        self.doTest(5)

if __name__ == '__main__':
    suite = unittest.TestLoader().loadTestsFromTestCase(GPLoad_Env_TestCase)
    runner = unittest.TextTestRunner(verbosity=2)
    ret = not runner.run(suite).wasSuccessful()
    sys.exit(ret)
