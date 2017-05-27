#!/usr/bin/env python
# -*- coding: utf-8 -*-
# gpunload - unload file(s) from Greenplum Database
# Copyright Greenplum 2008

#CLIN v1.3: Remove keywords and is_keyword
#           Remove not-used functions
#     v1.4: Remove NUM_WARN_ROWS
#           Simplify read_table_metadata
#           Remove serial checks
#     v1.5: Change check on permission
#           Change self.into_columns to self.source_columns
#           Remove source_columns_dict
#     v1.6: Change ident external table
#           Use of format_type(atttypid, atttypmod)
#           Change the filter on pgext.fmtopts (= instead of like)
#     v1.7: Try to add the option -t (replacing -f)
#     v1.8: Raise an error when the source table doesn't exist
#     v1.9: Changes for Windows
#
'''gpunload [options] -f configuration file

Options:
    -h hostname: host to connect to
    -p port: port to connect to
    -U username: user to connect as
    -d database: database to connect to
    -W: force password authentication
    -q: quiet mode
    -D: do not actually extract data
    -v: verbose
    -V: very verbose
    -l logfile: log output to logfile
    -t Source table (not compatible with option -f)
    --output_dir Directory for output file (not compatible with option -f)
    --gpfdist_timeout timeout: gpfdist timeout value
    --version: print version number and exit
    -?: help
'''

import sys
if sys.hexversion<0x2040400:
    sys.stderr.write("gpunload needs python 2.4.4 or higher\n")
    sys.exit(2)

try:
    import yaml
except ImportError:
    sys.stderr.write("gpunload needs pyyaml.  You can get it from http://pyyaml.org.\n")
    sys.exit(2)

try:
    from pygresql import pg
except Exception, e:
    errorMsg = "gpunload was unable to import The PyGreSQL Python module (pg.py) - %s\n" % str(e)
    sys.stderr.write(str(errorMsg))
    sys.exit(2)

import hashlib
import datetime,getpass,os,signal,socket,subprocess,threading,time,traceback,re
import platform
import uuid

thePlatform = platform.system()
if thePlatform in ['Windows', 'Microsoft']:
   windowsPlatform = True
else:
   windowsPlatform = False

if windowsPlatform == False:
   import select


EXECNAME = 'gpunload'


# Mapping for validing our configuration file. We're only concerned with
# keys -- stuff left of ':'. It gets complex in two cases: firstly when 
# we handle blocks which have keys which are not keywords -- such as under 
# COLUMNS:. Secondly, we want to detect when users put keywords in the wrong
# place. To that end, the mapping is structured such that:
#
#       key -> { 'parse_children' -> [ True | False ],
#                'parent' -> <parent name> }
#
# Each key is a keyword in the configuration file. parse_children tells us
# whether children are expected to be keywords. parent tells us the parent
# keyword or None 
valid_tokens = {
    "version": {'parse_children': True, 'parent': None},
    "database": {'parse_children': True, 'parent': None}, 
    "user": {'parse_children': True, 'parent': None}, 
    "host": {'parse_children': True, 'parent': None}, 
    "port": {'parse_children': True, 'parent': [None, "target"]},
    "password": {'parse_children': True, 'parent': None},
    "gpunload": {'parse_children': True, 'parent': None},
    "input": {'parse_children': True, 'parent': "gpunload"},
    "target": {'parse_children': True, 'parent': "output"},
    "local_hostname": {'parse_children': False, 'parent': "target"},
    "port_range": {'parse_children': False, 'parent': "target"},
    "windows_disk": {'parse_children': False, 'parent': "target"}, # v1.9
    "file": {'parse_children': False, 'parent': "target"},
    "directory": {'parse_children': False, 'parent': "target"},
    "ssl": {'parse_children': False, 'parent': "target"},
    "certificates_path": {'parse_children': False, 'parent': "target"},
    "columns": {'parse_children': False, 'parent': "output"},
    "max_line_length": {'parse_children': True, 'parent': "input"},
    "format": {'parse_children': True, 'parent': "output"},
    "delimiter": {'parse_children': True, 'parent': "output"}, 
    "escape": {'parse_children': True, 'parent': "output"},
    "null_as": {'parse_children': True, 'parent': "output"},
    "quote": {'parse_children': True, 'parent': "output"}, 
    "encoding": {'parse_children': True, 'parent': "output"},
    "force_quote": {'parse_children': False, 'parent': "output"},
    "header": {'parse_children': True, 'parent': "output"},
    "output": {'parse_children': True, 'parent': "gpunload"},
    "table": {'parse_children': True, 'parent': "input"}, 
    "extract_query": {'parse_children': True, 'parent': "input"}, 
    "extract_condition": {'parse_children': True, 'parent': "input"}, 
    "mapping": {'parse_children': False, 'parent': "output"},
    "options": {'parse_children': True, 'parent': 'gpunload'},
    "append": {'parse_children': False, 'parent': 'options'},
    "reuse_tables": {'parse_children': False, 'parent': 'options'},
    "sql": {'parse_children': True, 'parent': 'gpunload'},
    "before": {'parse_children': False, 'parent': 'sql'},
    "after": {'parse_children': False, 'parent': 'sql'},
    "external": {'parse_children': True, 'parent': 'gpunload'},
    "schema": {'parse_children': False, 'parent': 'external'}}

_abbrevs = [
    (1<<50L, ' PB'),
    (1<<40L, ' TB'),
    (1<<30L, ' GB'),
    (1<<20L, ' MB'),
    (1<<10L, ' kB'),
    (1, ' bytes')
    ]

received_kill = False


def caseInsensitiveDictLookup(key, dictionary):
    """
    Do a case insensitive dictionary lookup. Return the dictionary value if found,
    or None if not found.                                                                                                               
    """
    for entry in dictionary:
        if entry.lower() == key.lower():
           return dictionary[entry]
    return None



def sqlIdentifierCompare(x, y):
    """                                                                                                            
    Compare x and y as SQL identifiers. Use SQL rules for comparing delimited
    and non-delimited identifiers. Return True if they are equivalent or False
    if they are not equivalent.
    """
    if x == None or y == None:
       return False

    if isDelimited(x):
       x = quote_unident(x)
    else:
       x = x.lower()
    if isDelimited(y):
       y = quote_unident(y)
    else:
       y = y.lower()

    if x == y:
       return True
    else:
       return False


def isDelimited(value):
    """
    This method simply checks to see if the user supplied value has delimiters.
    That is, if it starts and ends with double-quotes, then it is delimited.
    """
    if len(value) < 2:
       return False
    if value[0] == '"' and value[-1] == '"':
       return True
    else:
       return False


def convertListToDelimited(identifiers):
    """
    This method will convert a list of identifiers, which may be a mix of
    delimited and non-delimited identifiers, and return a list of 
    delimited identifiers.
    """
    returnList = []

    for id in identifiers:
        if isDelimited(id) == False:
           id = id.lower()
           returnList.append(quote_ident(id))
        else:
           returnList.append(id)
    return returnList



def splitUpMultipartIdentifier(id):
    """
    Given a sql identifer like sch.tab, return a list of its
    individual elements (e.g.  sch.tab would return ['sch','tab']
    """
    returnList = []

    elementList = splitIntoLiteralsAndNonLiterals(id, quoteValue='"')
    # If there is a leading empty string, remove it.
    if elementList[0] == ' ':
       elementList.pop(0)

    # Remove the dots, and split up undelimited multipart names
    for e in elementList:
        if e != '.':
           if e[0] != '"':
              subElementList = e.split('.')
           else:
              subElementList = [e]
           for se in subElementList:
               # remove any empty elements
               if se != '':
                  returnList.append(se)

    return returnList    


def splitIntoLiteralsAndNonLiterals(str1, quoteValue="'"):
    """
    Break the string (str1) into a list of literals and non-literals where every
    even number element is a non-literal and every odd number element is a literal.
    The delimiter between literals and non-literals is the quoteValue, so this
    function will not take into account any modifiers on a literal (e.g. E'adf').
    """
    returnList = []

    if len(str1) > 1 and str1[0] == quoteValue:
       # Always start with a non-literal
       str1 = ' ' + str1

    inLiteral = False
    i = 0
    tokenStart = 0
    while i < len(str1):
        if str1[i] == quoteValue:
           if inLiteral == False:
              # We are at start of literal
              inLiteral = True
              returnList.append(str1[tokenStart:i])
              tokenStart = i
           elif i + 1 < len(str1) and str1[i+1] == quoteValue:
              # We are in a literal and found quote quote, so skip over it
              i = i + 1
           else:
              # We are at the end of a literal or end of str1
              returnList.append(str1[tokenStart:i+1])
              tokenStart = i + 1
              inLiteral = False
        i = i + 1
    if tokenStart < len(str1):
       returnList.append(str1[tokenStart:])
    return returnList


def quote_ident(val):
    """
    This method returns a new string replacing " with "",
    and adding a " at the start and end of the string.
    """
    return '"' + val.replace('"', '""') + '"'


def quote_unident(val):
    """
    This method returns a new string replacing "" with ",
    and  removing the " at the start and end of the string.
    """
    if val != None and len(val) > 0:
       val = val.replace('""', '"')
       if val != None and len(val) > 1 and val[0] == '"' and val[-1] == '"':  
           val = val[1:-1]
    
    return val


def handle_kill(signum, frame):
    # already dying?
    global received_kill
    if received_kill:
        return

    received_kill = True

    g.log(g.INFO, "received signal %d" % signum)
    g.exitValue = 2
    sys.exit(2)


def bytestr(size, precision=1):
    """Return a string representing the greek/metric suffix of a size"""
    if size==1:
        return '1 byte'
    for factor, suffix in _abbrevs:
        if size >= factor:
            break

    float_string_split = `size/float(factor)`.split('.')
    integer_part = float_string_split[0]
    decimal_part = float_string_split[1]
    if int(decimal_part[0:precision]):
        float_string = '.'.join([integer_part, decimal_part[0:precision]])
    else:
        float_string = integer_part
    return float_string + suffix

class CatThread(threading.Thread):
    """
    Simple threading wrapper to read a file descriptor and put the contents
    in the log file.

    The fd is assumed to be stdout and stderr from gpfdist. We must use select.select
    and locks to ensure both threads are not read at the same time. A dead lock 
    situation could happen if they did. communicate() is not used since it blocks.
    We will wait 1 second between read attempts.

    """
    def __init__(self,gpunload,fd, sharedLock = None):
        threading.Thread.__init__(self)
        self.gpunload = gpunload
        self.fd = fd
        self.theLock = sharedLock

    def run(self):
        if windowsPlatform == True:
           while 1:
               # Windows select does not support select on non-file fd's, so we can use the lock fix. Deadlock is possible here.
               # We need to look into the Python windows module to see if there is another way to do this in Windows.
               line = self.fd.readline()
               if line=='':
                   break
               self.gpunload.log(self.gpunload.DEBUG, 'gpfdist: ' + line.strip('\n'))
        else:
           while 1:
               retList = select.select( [self.fd]
                                      , []
                                      , []
                                      , 1
                                      )
               if retList[0] == [self.fd]:
                  self.theLock.acquire()
                  line = self.fd.readline()
                  self.theLock.release()
               else:
                  continue
               if line=='':
                  break
               self.gpunload.log(self.gpunload.DEBUG, 'gpfdist: ' + line.strip('\n'))


def cli_help():
    help_path = os.path.join(sys.path[0], '..', 'docs', 'cli_help', EXECNAME + 
                             '_help');
    #print ('help path: %s' % help_path)
    f = None
    try:
        try:
            f = open(help_path);
            return f.read(-1)
        except:
            return ''
    finally:
        if f: f.close()

#============================================================
def usage(error = None):
    print cli_help() or __doc__
    sys.stdout.flush()
    if error:
        sys.stderr.write('ERROR: ' + error + '\n')
        sys.stderr.write('\n')
        sys.stderr.flush()
  
    sys.exit(2)

def quote(a):
    """
    SQLify a string
    """
    return "'"+a.replace("'","''").replace('\\','\\\\')+"'"

def splitPgpassLine(a):
    """
    If the user has specified a .pgpass file, we'll have to parse it. We simply
    split the string into arrays at :. We could just use a native python
    function but we need to escape the ':' character.
    """
    b = []
    escape = False
    d = ''
    for c in a:
        if not escape and c=='\\':
            escape = True
        elif not escape and c==':':
            b.append(d)
            d = ''
        else:
            d += c
            escape = False
    if escape:
        d += '\\'
    b.append(d)
    return b

def test_key(gp, key, crumb):
    """
    Make sure that a key is a valid keyword in the configuration grammar and
    that it appears in the configuration file where we expect -- that is, where
    it has the parent we expect
    """
    val = valid_tokens.get(key)
    if val == None:
        gp.log(gp.ERROR, 'unrecognized key: "%s"' % key)
    
    p = val['parent']

    # simplify for when the same keyword can appear in multiple places
    if type(p) != list:
        p = [p]

    c = None
    if len(crumb):
        c = crumb[-1]

    found = False
    for m in p:
        if m == c:
            found = True
            break

    if not found:
        gp.log(gp.ERROR, 'unexpected key: "%s"' % key)

    return val

def yaml_walk(gp, node, crumb):
    if type(node) == list:
        for a in node:
            if type(a) == tuple:
                key = a[0].value.lower()

                val = test_key(gp, key, crumb)

                if (len(a) > 1 and val['parse_children'] and
                    (isinstance(a[1], yaml.nodes.MappingNode) or
                     isinstance(a[1], yaml.nodes.SequenceNode))):
                    crumb.append(key)
                    yaml_walk(gp, a[1], crumb)
                    crumb.pop()
            elif isinstance(a, yaml.nodes.ScalarNode):
                test_key(gp, a.value, crumb)
            else:
                yaml_walk(gp, a, crumb)
    elif isinstance(node, yaml.nodes.MappingNode):
        yaml_walk(gp, node.value, crumb)

    elif isinstance(node, yaml.nodes.ScalarNode):
        pass

    elif isinstance(node, yaml.nodes.SequenceNode):
        yaml_walk(gp, node.value, crumb)

    elif isinstance(node, yaml.nodes.CollectionNode):
        pass


def changeToUnicode(a):
    """
    Change every entry in a list or dictionary to a unicode item
    """
    if type(a) == list:
        return map(changeToUnicode,a)
    if type(a) == dict:
        b = dict()
        for key,value in a.iteritems():
            if type(key) == str:
                key = unicode(key)                                                                  
            b[key] = changeToUnicode(value)
        return b
    if type(a) == str:
        a = unicode(a)
    return a



def dictKeyToLower(a):
    """
    down case all entries in a list or dict
    """
    if type(a) == list:
        return map(dictKeyToLower,a)
    if type(a) == dict:
        b = dict()
        for key,value in a.iteritems():
            if type(key) == str:
                key = unicode(key.lower())
            b[key] = dictKeyToLower(value)
        return b
    if type(a) == str:
        a = unicode(a)
    return a




class options:
    pass

class gpunload:
    """
    Main class wrapper
    """

    def __init__(self,argv):
        self.threads = [] # remember threads so that we can join() against them
        self.exitValue = 0
        self.options = options()
        self.options.h = None
        self.options.gpfdist_timeout = None
        self.options.p = None
        self.options.U = None
        self.options.W = False
        self.options.D = False
        self.options.password = None
        self.options.d = None
        self.DEBUG = 5
        self.LOG = 4
        self.INFO = 3
        self.WARN = 2
        self.ERROR = 1
        self.options.qv = self.INFO
        self.options.l = None
        
        self.options.t = None #CLIN v1.7
        if platform.system() in ['Windows', 'Microsoft']: # v1.9
            self.options.output_dir = "C:/" #CLIN v1.9
            #CLIN self.windows_disk3 = self.options.output_dir.split(":")[0]
            #CLIN print ('self.windows_disk3: %s' % self.windows_disk3)
        else:
            self.options.output_dir = "/tmp/" #CLIN v1.9
        seenv = False
        seenq = False

        # Create External table names. However external table name could 
        # get overwritten with another name later on (see create_external_table_name). 
        # gpunload external table name problem. We use uuid to avoid
        # external table name confliction.
        self.unique_suffix = str(uuid.uuid1()).replace('-', '_')
        self.extTableName  = 'ext_gpunload_' + self.unique_suffix

        # SQL to run in order to undo our temporary work
        self.cleanupSql = []
        self.distkey = None
        configFilename = None
        while argv:
            try:
                try:
                    if argv[0]=='-h':
                        self.options.h = argv[1]
                        argv = argv[2:]
                    if argv[0]=='--gpfdist_timeout':
                        self.options.gpfdist_timeout = argv[1]
                        argv = argv[2:]
                    elif argv[0]=='-p':
                        self.options.p = int(argv[1])
                        argv = argv[2:]
                    elif argv[0]=='-l':
                        self.options.l = argv[1]
                        argv = argv[2:]
                    elif argv[0]=='-t': #CLIN v1.7
                        self.options.t = argv[1] #CLIN v1.7
                        argv = argv[2:] #CLIN v1.7
                    elif argv[0]=='--output_dir': #CLIN v1.7
                        self.options.output_dir = argv[1] #CLIN v1.7
                        argv = argv[2:] #CLIN v1.7

                    elif argv[0]=='-q':
                        self.options.qv -= 1
                        argv = argv[1:]
                        seenq = True
                    elif argv[0]=='--version':
                        sys.stderr.write("gpunload version 4.3.8.0 build 1\n")
                        sys.exit(0)
                    elif argv[0]=='-v':
                        self.options.qv = self.LOG
                        argv = argv[1:]
                        seenv = True
                    elif argv[0]=='-V':
                        self.options.qv = self.DEBUG
                        argv = argv[1:]
                        seenv = True
                    elif argv[0]=='-W':
                        self.options.W = True
                        argv = argv[1:]
                    elif argv[0]=='-D':
                        self.options.D = True
                        argv = argv[1:]
                    elif argv[0]=='-U':
                        self.options.U = argv[1]
                        argv = argv[2:]
                    elif argv[0]=='-d':
                        self.options.d = argv[1]
                        argv = argv[2:]
                    elif argv[0]=='-f':
                        configFilename = argv[1]
                        argv = argv[2:]
                    elif argv[0]=='-?':
                        usage()
                    else:
                        break
                except IndexError:
                    sys.stderr.write("Option %s needs a parameter.\n"%argv[0])
                    sys.exit(2)
            except ValueError:
                sys.stderr.write("Parameter for option %s must be an integer.\n"%argv[0])
                sys.exit(2)

        if configFilename==None and self.options.t==None: #CLIN v1.7
            usage('Either configuration file required, either option -t required ') #CLIN v1.7
        elif configFilename!=None and self.options.t!=None: #CLIN v1.7
            usage('configuration file and option -t can not be used together') #CLIN v1.7
        elif argv:
            a = ""
            if len(argv) > 1:
                a = "s"
            usage('unrecognized argument%s: %s' % (a, ' '.join(argv)))

        # default to gpAdminLogs for a log file, may be overwritten
        if self.options.l is None:
            self.options.l = os.path.join(os.environ.get('HOME', '.'),'gpAdminLogs')
            if not os.path.isdir(self.options.l):
                os.mkdir(self.options.l)

            self.options.l = os.path.join(self.options.l, 'gpunload_' + \
                                          datetime.date.today().strftime('%Y%m%d') + '.log')

        try:
            self.logfile = open(self.options.l,'a')
        except Exception, e:
            self.log(self.ERROR, "could not open logfile %s: %s" % \
                      (self.options.l, e))

        if seenv and seenq:
            self.log(self.ERROR, "-q conflicts with -v and -V")

        if self.options.D:
            self.log(self.INFO, 'gpunload has the -D option, so it does not actually unload any data')

        #CLIN v1.7 - Start
        if configFilename==None: #CLIN v1.7
            self.flag_configFilename = False #CLIN v1.7

            output_dir_exists = os.path.isdir(self.options.output_dir)
            #CLINif platform.system() in ['Windows', 'Microsoft']: # v1.9
               #CLIN self.windows_disk2 = self.options.output_dir #v1.9
               #CLIN print ('no config - windows_disk2: %s' % self.windows_disk2) #v1.9
               #CLIN print ('no config - gpfdist_directory %s' % self.gpfdist_directory)

            if output_dir_exists == False:

                self.log(self.ERROR, "The --output_dir directory (%s) does not exist" % self.options.output_dir)			

        else: #CLIN v1.7
            self.flag_configFilename = True #CLIN v1.7
            self.read_config_file(configFilename) #CLIN v1.7

        #CLIN v1.7 - End

        self.subprocesses = []
        self.log(self.INFO,'gpunload session started ' + \
                 datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S'))

    def control_file_warning(self, msg):
        self.log(self.WARN, "A gpunload control file processing warning occurred. %s" % msg)

    def control_file_error(self, msg):
        self.log(self.ERROR, "A gpunload control file processing error occurred. %s" % msg)

    def elevel2str(self, level):
        if level == self.DEBUG:
            return "DEBUG"
        elif level == self.LOG:
            return "LOG"
        elif level == self.INFO:
            return "INFO"
        elif level == self.ERROR:
            return "ERROR"
        elif level == self.WARN:
            return "WARN"
        else:
            self.log(self.ERROR, "unknown log type %i" % level)

    def log(self, level, a):
        """
        Level is either DEBUG, LOG, INFO, ERROR. a is the message
        """
        t = time.localtime()
        str = '|'.join(
                       [datetime.datetime.today().strftime('%Y-%m-%d %H:%M:%S'),
                        self.elevel2str(level), a]) + '\n'

        str = str.encode('utf-8')

        if level <= self.options.qv:
            sys.stdout.write(str)

        if level <= self.options.qv or level <= self.INFO:
            try:
               self.logfile.write(str)
               self.logfile.flush()
            except AttributeError, e:
                pass

        if level == self.ERROR:
            self.exitValue = 2;
            sys.exit(self.exitValue)

    def read_config_file(self,configFilename):
        #print ('filename: %s' % configFilename)
        try:
            f = open(configFilename,'r')
        except IOError,e:
            self.log(self.ERROR, "could not open configuration file: %s" % e)

        # pull in the config file, which should be in valid YAML
        try:
            # do an initial parse, validating the config file
            doc = f.read()
            self.config = yaml.load(doc)

            self.configOriginal = changeToUnicode(self.config)
            self.config = dictKeyToLower(self.config)
            #print ('self.config: %s' % self.config)
            ver = self.getconfig('version', unicode, extraStuff = ' tag')
            if ver != '1.0.0.1':
                self.control_file_error("gpunload configuration schema version must be 1.0.0.1")
            # second parse, to check that the keywords are sensible
            y = yaml.compose(doc)
            # first should be MappingNode
            if not isinstance(y, yaml.MappingNode):
                self.control_file_error("configuration file must begin with a mapping")

            yaml_walk(self, y.value, [])
        except yaml.scanner.ScannerError,e:
            self.log(self.ERROR, "configuration file error: %s, line %s" % \
                (e.problem, e.problem_mark.line))
        except yaml.reader.ReaderError, e:
            es = ""
            if isinstance(e.character, str):
                es = "'%s' codec can't decode byte #x%02x: %s position %d" % \
                        (e.encoding, ord(e.character), e.reason,
                         e.position)
            else:
                es = "unacceptable character #x%04x at byte %d: %s"    \
                    % (ord(e.character), e.position, e.reason)
            self.log(self.ERROR, es)
        except yaml.error.MarkedYAMLError, e:
            self.log(self.ERROR, "configuration file error: %s, line %s" % \
                (e.problem, e.problem_mark.line))

        f.close()

			
    def getconfig(self, a, typ=None, default='error', extraStuff='', returnOriginal=False):
        """
        Look for a config entry, via a column delimited string. a:b:c points to
        
        a:
            b:
                c

        Make sure that end point is of type 'typ' when not set to None.
 
        If returnOriginal is False, the return value will be in lower case, 
        else the return value will be in its original form (i.e. the case that
        the user specified in their yaml file).
        """
        if self.flag_configFilename==False:
            config = default
            if a=='gpunload:input:table':
                config = self.options.t
            elif a=='gpunload:output:format':
                config = "csv"
            elif a=='gpunload:output:delimiter':
                config = ","
            elif a=='gpunload:output:escape':
                config = "\\"
            elif a=='gpunload:output:quote':
                config = '"'
            elif a=='gpunload:output:target(1)':
                config = True
            elif a=='gpunload:output:target(1):file':
                #config=[self.options.output_dir+"/"+self.options.t+".dat"]
                if platform.system() in ['Windows', 'Microsoft']: # v1.9
                    config=[self.options.output_dir.split(":")[1]+"/"+self.options.t+".dat"]
                else:
                    config=[self.options.output_dir+"/"+self.options.t+".dat"]
            elif a=='gpunload:output:target(1):windows_disk':
                if platform.system() in ['Windows', 'Microsoft']: # v1.9
                    config=[self.options.output_dir.split(":")[0]][0]                
                    #print ('gpunload:output:target(1):windows_disk - %s' % config) #v1.9


            #print('getconfig returned value: %s' % config)
            return config
			
			
        self.log(self.DEBUG, "getting config for " + a)
        if returnOriginal == True:
           config = self.configOriginal
        else:
           config = self.config
        for s in a.split(':'):
            self.log(self.DEBUG, "trying " + s)
            index = 1

            if s[-1:]==')':
                j = s.index('(')
                index = int(s[j+1:-1])
                s = s[:j]

            if type(config)!=list:
                config = [config]

            for c in config:
                if type(c)==dict:
                    temp = caseInsensitiveDictLookup(s, c)
                    if temp != None:
                       index -= 1
                       if not index:
                           self.log(self.DEBUG, "found " + s)
                           config = temp
                           break
            else:
                if default=='error':
                    self.control_file_error("The configuration must contain %s %s"%(a,extraStuff))
                    sys.exit(2)
                return default

        if typ != None and type(config) != typ:
            if typ == list:
                self.control_file_error("The %s entry must be a YAML sequence %s"% (a ,extraStuff))
            elif typ == dict:
                self.control_file_error("The %s entry must be a YAML mapping %s"% (a, extraStuff))
            elif typ == unicode or typ == str:
                self.control_file_error("%s must be a string %s" % (a, extraStuff))
            elif typ == int:
                self.control_file_error("The %s entry must be a YAML integer %s" % (a, extraStuff))
            else:
                assert 0

            self.control_file_error("Encountered unknown configuration type %s"% type(config))
            sys.exit(2)
        return config

    def read_config(self):
        """
        Configure ourselves
        """
        # Precendence for configuration: command line > config file > env
        # variable

        # host to connect to
        if not self.options.h:
            self.options.h = self.getconfig('host', unicode, None)
            if self.options.h:
                self.options.h = str(self.options.h)
        if not self.options.h:
            self.options.h = os.environ.get('PGHOST')
        if not self.options.h or len(self.options.h) == 0:
            self.log(self.INFO, "no host supplied, defaulting to localhost")
            self.options.h = "localhost"

        # Port to connect to
        if not self.options.p:
            self.options.p = self.getconfig('port',int,None)
        if not self.options.p:
            try:
                    self.options.p = int(os.environ.get('PGPORT'))
            except (ValueError, TypeError):
                    pass
        if not self.options.p:
            self.options.p = 5432

        # User to connect as
        if not self.options.U:
            self.options.U = self.getconfig('user', unicode, None)
        if not self.options.U:
            self.options.U = os.environ.get('PGUSER')
        if not self.options.U:
            self.options.U = os.environ.get('USER') or \
                    os.environ.get('LOGNAME') or \
                    os.environ.get('USERNAME')

        if not self.options.U or len(self.options.U) == 0:
            self.log(self.ERROR,
                       "You need to specify your username with the -U " +
                       "option or in your configuration or in your " +
                       "environment as PGUSER")

        # database to connect to
        if not self.options.d:
            self.options.d = self.getconfig('database', unicode, None)
        if not self.options.d:
            self.options.d = os.environ.get('PGDATABASE')
        if not self.options.d:
            # like libpq, just inherit USER
            self.options.d = self.options.U

        # Password
        if self.options.W:
            if self.options.password==None:
                self.options.password = getpass.getpass()
        else:
            if self.options.password==None:
                self.options.password = self.getconfig('password', unicode,
                                                       None)
            if self.options.password==None:
                self.options.password = os.environ.get('PGPASSWORD')
            if self.options.password==None:
                self.readPgpass(os.environ.get('PGPASSFILE',
                                os.environ.get('HOME','.')+'/.pgpass'))


        # ensure output is of type list
        self.getconfig('gpunload:output', list)

        # The user supplied table name can be completely or partially delimited,
        # and it can be a one or two part name. Get the originally supplied name
        # and parse it into its delimited one or two part name.
        self.schemaTable = self.getconfig('gpunload:input:table', unicode, returnOriginal=True)
        schemaTableList  = splitUpMultipartIdentifier(self.schemaTable)
        schemaTableList  = convertListToDelimited(schemaTableList)
        if len(schemaTableList) == 2:
           self.schema = schemaTableList[0]
           self.table  = schemaTableList[1]
        else:
           self.schema = None
           self.table  = schemaTableList[0]

        #Output File Format
        if platform.system() in ['Windows', 'Microsoft']: # v1.9
           self.windows_disk = self.getconfig('gpunload:output:target(1):windows_disk',unicode,'C') #v1.9
           #print ('windows_disk: %s' % self.windows_disk) #v1.9
        self.formatType = self.getconfig('gpunload:output:format', unicode, 'text').lower()
        self.delimiterValue = self.final_delimiter(self.formatType)
        #print('delimiter: %s' % self.delimiterValue)#CLIN

        self.flag_header = self.getconfig('gpunload:output:header',bool,False)

        # Get the options
        list_options = self.getconfig('gpunload:options', list, default=None)
        self.flag_append = False
        self.reuse_tables = False
        if list_options:
            self.flag_append = self.getconfig('gpunload:options:append',bool,False)
            self.reuse_tables = self.getconfig('gpunload:options:reuse_tables',bool,False)

    def gpfdist_port_options(self, name, availablePorts, popenList):
        """
        Adds gpfdist -p / -P port options to popenList based on port and port_range in YAML file.
        Raises errors if options are invalid or ports are unavailable.

        @param name: input source name from YAML file.
        @param availablePorts: current set of available ports 
        @param popenList: gpfdist options (updated)
        """
        port = self.getconfig(name + ':port', int, None)
        port_range = self.getconfig(name+':port_range', list, None)

        if port:
            startPort = endPort = port
            endPort += 1
        elif port_range:
            try:
                startPort = int(port_range[0])
                endPort = int(port_range[1])
            except (IndexError,ValueError):
                self.control_file_error(name + ":port_range must be a YAML sequence of two integers")
        else:
            startPort = self.getconfig(name+':port',int,8000)
            endPort = self.getconfig(name+':port',int,9000)

        if (startPort > 65535 or endPort > 65535):
            # Do not allow invalid ports
            self.control_file_error("Invalid port. Port values must be less than or equal to 65535.")
        elif not (set(xrange(startPort,endPort+1)) & availablePorts):
            self.log(self.ERROR, "no more ports available for gpfdist")

        popenList.append('-p')
        popenList.append(str(startPort))

        popenList.append('-P')
        popenList.append(str(endPort))


    def list_filenames(self, name):
        """
        Get list of target files
        Raises errors if YAML file option is invalid.

        @param name: input source name from YAML file.
        @return: list of files names
        """
        file = self.getconfig(name+':file',list)

        for i in file:
            if type(i)!= unicode and type(i) != str:
                self.control_file_error(name + ":file must be a YAML sequence of strings")
        return file


    def gpfdist_timeout_options(self, popenList):
        """
        Adds gpfdist -t timeout option to popenList.

        @param popenList: gpfdist options (updated)
        """
        if self.options.gpfdist_timeout != None:
            gpfdistTimeout = self.options.gpfdist_timeout
        else:
            gpfdistTimeout = 30
        popenList.append('-t')
        popenList.append(str(gpfdistTimeout))


    def gpfdist_verbose_options(self, popenList):
        """
        Adds gpfdist -v / -V options to popenList depending on logging level

        @param popenList: gpfdist options (updated)
        """
        if self.options.qv == self.LOG:
            popenList.append('-v')
        elif self.options.qv > self.LOG:
            popenList.append('-V')


    def gpfdist_max_line_length(self, popenList):
        """
        Adds gpfdist -m option to popenList when max_line_length option specified in YAML file.

        @param popenList: gpfdist options (updated)
        """
        max_line_length = self.getconfig('gpunload:output:max_line_length',int,None)
        if max_line_length is not None:
            popenList.append('-m')
            popenList.append(str(max_line_length))




    def gpfdist_ssl(self, popenList):
        """
        Adds gpfdist --ssl option to popenList when ssl option specified as true in YAML file.

        @param popenList: gpfdist options (updated)
        """
        ssl = self.getconfig('gpunload:output:target:ssl',bool, False)
        certificates_path = self.getconfig('gpunload:output:target:certificates_path', unicode, None)

        if ssl and certificates_path:
            dir_exists = os.path.isdir(certificates_path)
            if dir_exists == False:
                self.log(self.ERROR, "could not access CERTIFICATES_PATH directory: %s" % certificates_path)			

            popenList.append('--ssl')
            popenList.append(certificates_path)

        else:
            if ssl:
                self.control_file_error("CERTIFICATES_PATH is required when SSL is specified as true")
            elif certificates_path:    # ssl=false (or not specified) and certificates_path is specified
                self.control_file_error("CERTIFICATES_PATH is specified while SSL is not specified as true")


    def start_gpfdists(self):
        """
        Start gpfdist daemon(s)
        """
        self.locations = []
        self.ports = []
        targetIndex = 0
        availablePorts = set(xrange(1,65535))
        found_target = False

        self.getconfig('gpunload:input', list)
        while 1:
            targetIndex += 1
            name = 'gpunload:output:target(%d)'%targetIndex
            a = self.getconfig(name,None,None)
            if not a:
                break
            found_target = True
            local_hostname = self.getconfig(name+':local_hostname', list, False)
            #print 'local_hostname: %s' % local_hostname

            # do default host, the current one
            if not local_hostname:
                try:
                    pipe = subprocess.Popen("hostname",
                                            stdout=subprocess.PIPE,
                                            stderr=subprocess.PIPE)
                    result  = pipe.communicate();
                except OSError, e:
                    self.log(self.ERROR, "command failed: " + str(e))
                
                local_hostname = [result[0].strip()]

            # build gpfdist parameters
            if platform.system() in ['Windows', 'Microsoft']: # v1.9
                #print('Windows platform') #v1.9
                self.gpfdist_directory=self.windows_disk + ':\\' #v1.9
            else:
                #print('Other platform') #v1.9
                self.gpfdist_directory='/home/..' #v1.9
            #print ('gpfdist_directory: %s' % self.gpfdist_directory) # v1.9: Disk identification
            popenList = ['gpfdist -d ''' + self.gpfdist_directory + ''] #v1.9
            #print ('popenlist: %s' % popenList) # v1.9

            #popenList = ['gpfdist -d ''/home/..'''] #v1.9
            self.gpfdist_ssl(popenList)
            self.gpfdist_port_options(name, availablePorts, popenList)
            self.gpfdist_timeout_options(popenList)
            self.gpfdist_verbose_options(popenList)
            self.gpfdist_max_line_length(popenList)


            try:
                self.log(self.LOG, 'trying to run %s' % ' '.join(popenList))
                cfds = True
                if platform.system() in ['Windows', 'Microsoft']: # not supported on win32
                    cfds = False
                    cmd = ' '.join(popenList)
                    needshell = False
                    #print ('filename: %s' % configFilename) # v1.8: Disk identification
                else:
                    srcfile = None
                    if os.environ.get('GPHOME_LOADERS'):
                        srcfile = os.path.join(os.environ.get('GPHOME_LOADERS'),
                                           'greenplum_loaders_path.sh')
                    elif os.environ.get('GPHOME'):
                        srcfile = os.path.join(os.environ.get('GPHOME'),
                                           'greenplum_path.sh')

                    if (not (srcfile and os.path.exists(srcfile))):
                        self.log(self.ERROR, 'cannot find greenplum environment ' +
                                    'file: environment misconfigured')

                    cmd = 'source %s ; exec ' % srcfile
                    cmd += ' '.join(popenList)
                    needshell = True

                a = subprocess.Popen(cmd, stdout=subprocess.PIPE,
                                     stderr=subprocess.PIPE,
                                     close_fds=cfds, shell=needshell)
                self.subprocesses.append(a)
            except Exception, e:
                self.log(self.ERROR, "could not run %s: %s" % \
                                (' '.join(popenList), str(e)))

            """ 
            Reading from stderr and stdout on a Popen object can result in a dead lock if done at the same time.
            Create a lock to share when reading stderr and stdout from gpfdist. 
            """
            readLock = threading.Lock()

            # get all the output from the daemon(s)
            t = CatThread(self,a.stderr, readLock)
            t.start()
            self.threads.append(t)

            while 1:
                readLock.acquire()
                line = a.stdout.readline()
                readLock.release()
                if line=='':
                    self.log(self.ERROR,'failed to start gpfdist: ' +
                             'gpfdist command line: ' + ' '.join(popenList))

                line = line.strip('\n')
                self.log(self.LOG,'gpfdist says: ' + line)
                if (line.startswith('Serving HTTP on port ') or line.startswith('Serving HTTPS on port ')):
                    port = int(line[21:line.index(',')])
                    break

            self.log(self.INFO, 'started %s' % ' '.join(popenList))
            self.log(self.LOG,'gpfdist is running on port %d'%port)
            if port in availablePorts:
                availablePorts.remove(port)
            self.ports.append(port)
            t = CatThread(self,a.stdout,readLock)
            t.start()
            self.threads.append(t)

            ssl = self.getconfig('gpunload:output:target:ssl', bool, False)
            if ssl:
                protocol = 'gpfdists'
            else:
                protocol = 'gpfdist'
            self.init_Target_Files(name)
            for l in local_hostname:
                if type(l) != str and type(l) != unicode:
                    self.control_file_error(name + ":local_hostname must be a YAML sequence of strings")
                l = str(l)
                sep = ''
                #print 'location: %s' % l
                #CLIN print('file: %s' % file) #CLIN
                #CLIN print(file)  #CLIN
                #CLIN print(type(file))  #CLIN
                if self.list_files[0] != '/':
                    sep = '/'
                # MPP-13617
                if ':' in l:
                    l = '[' + l + ']'
                for file_item in self.list_files:
                    #CLIN print('name file:%s' % file_item)
                    self.locations.append('%s://%s:%d%s%s' % (protocol, l, port, sep, file_item)) 
        if not found_target:
            self.control_file_error("configuration file must contain target definition")


    def init_Target_Files(self,name):

        str_header = []
        if self.flag_header == True:
            for name_col,typ,mapto in self.output_columns:
                str_header.append(name_col)
        
        self.list_files = self.list_filenames(name)
        if not self.options.D:
            for file_item in self.list_files:
                #print('loop file_item: %s - %s' % (file_item,self.flag_append))
                # If Windows, add the disk before the file path
                if platform.system() in ['Windows', 'Microsoft']: # v1.9
                   file_item = self.windows_disk + ':' + file_item # v1.9
                   print('file_item windows: %s' % (file_item)) #v1.9
                if self.flag_append == False or os.path.isfile(file_item) == False:
                    text_file = open(file_item,'wb')
                    if self.flag_header == True:
                        text_file.write('%s\n' % self.delimiterValue.join(str_header))
                    text_file.close()


    def readPgpass(self,pgpassname):
        """
        Get password form .pgpass file
        """
        try:
            f = open(pgpassname,'r')
        except IOError:
            return
        for row in f:
            try:
                row = row.rstrip("\n")
                line = splitPgpassLine(row)
                if line[0]!='*' and line[0].lower()!=self.options.h.lower():
                    continue
                if line[1]!='*' and int(line[1])!=self.options.p:
                    continue
                if line[2]!='*' and line[2]!=self.options.d:
                    continue
                if line[3]!='*' and line[3]!=self.options.U:
                    continue
                self.options.password = line[4]
                break
            except (ValueError,IndexError):
                pass
        f.close()


    def setup_connection(self, recurse = 0):
        """
        Connect to the backend
        """
        if self.db != None: 
            self.db.close()
            self.db = None
        try:
            self.log(self.DEBUG, "connection string:" + 
                     " user=" + str(self.options.U) +
                     " host=" + str(self.options.h) +
                     " port=" + str(self.options.p) +
                     " database=" + str(self.options.d))
            self.db = pg.DB( dbname=self.options.d
                           , host=self.options.h 
                           , port=self.options.p
                           , user=self.options.U
                           , passwd=self.options.password
                           )
            self.log(self.DEBUG, "Successfully connected to database")
        except Exception, e:
            errorMessage = str(e)
            if errorMessage.find("no password supplied") != -1:
                self.options.password = getpass.getpass()
                recurse += 1
                if recurse > 10:
                    self.log(self.ERROR, "too many login attempt failures")
                self.setup_connection(recurse)
            else:
                self.log(self.ERROR, "could not connect to database: %s. Is " \
                    "the Greenplum Database running on port %i?" % (errorMessage,
                    self.options.p))

    def read_columns(self):
        columns = self.getconfig('gpunload:output:columns',list,None, returnOriginal=True)
        if columns != None:
            self.output_columns_user = True # user specified output columns
            self.output_columns = []
            for d in columns:
                if type(d)!=dict:
                    self.control_file_error("gpunload:output:columns must be a sequence of YAML mappings")
                tempkey = d.keys()[0]
                value = d[tempkey]
                """ remove leading or trailing spaces """
                d = { tempkey.strip() : value }
                key = d.keys()[0]
                if d[key] == None:
                    self.log(self.DEBUG, 
                             'getting target column data type from source')
                    for name, typ, mapto in self.source_columns:
                        if sqlIdentifierCompare(name, key):
                            d[key] = typ
                            break

                # Mark this column as having no mapping, which is important
                # for do_insert()
                self.output_columns.append([key,d[key],None])
        else:
            self.output_columns = self.source_columns
            self.output_columns_user = False

        # make sure that all columns have a type
        for name, typ, map in self.output_columns:
            if typ == None:
                self.log(self.ERROR, 'output column "%s" has no type ' % name +
                       'and does not appear in source table "%s"' % self.schemaTable)
        self.log(self.DEBUG, 'from columns are:')
        for c in self.output_columns:
            name = c[0]
            typ = c[1]
            self.log(self.DEBUG, '%s: %s'%(name,typ))



    def read_table_metadata(self):
        # find the shema name for this table (according to search_path)
        # if it was not explicitly specified in the configuration file.
        if self.schema == None:
            queryString = """SELECT n.nspname
                             FROM pg_catalog.pg_class c 
                             LEFT JOIN pg_catalog.pg_namespace n 
                             ON n.oid = c.relnamespace
                             WHERE c.relname = '%s' 
                             AND pg_catalog.pg_table_is_visible(c.oid);""" % quote_unident(self.table)
            
            resultList = self.db.query(queryString.encode('utf-8')).getresult()
            
            if len(resultList) > 0: 
                self.schema = (resultList[0])[0]
                self.log(self.INFO, "setting schema '%s' for table '%s'" % (self.schema, quote_unident(self.table)))
            else:
                self.log(self.ERROR, "table %s not found in any database schema" % self.table)

                   
        queryString = """select nt.nspname as table_schema,
         c.relname as table_name,
         has_table_privilege('%s',c.oid,'SELECT') as flag_select_permission,
         a.attname as column_name,
         a.attnum as ordinal_position, 
         format_type(a.atttypid, a.atttypmod) as data_type
          from pg_catalog.pg_class c join pg_catalog.pg_namespace nt on (c.relnamespace = nt.oid) 
             join pg_attribute a on (a.attrelid = c.oid) 
         where c.relname = '%s' and nt.nspname = '%s'
         and a.attnum > 0 and a.attisdropped = 'f'
         order by a.attnum """ % (quote_unident(self.options.U),quote_unident(self.table), quote_unident(self.schema))
        #print 'queryString: %s' % queryString

        count = 0
        self.source_columns = []
        resultList = self.db.query(queryString.encode('utf-8')).dictresult()
        #print 'resultList: %s' % resultList
        if len(resultList) == 0: 
            self.log(self.ERROR, "table %s.%s not found" % (self.schema,self.table))
        flag_select_privilege = resultList[0]['flag_select_permission']
        if flag_select_privilege == 'f':
            tableName = quote_unident(self.table)
            tableSchema = quote_unident(self.schema)
            self.log(self.ERROR, "permission denied for table %s.%s" % \
                        (tableSchema, tableName))

        while count < len(resultList):
            row = resultList[count]
            count += 1
            ct = unicode(row['data_type'])
            name = unicode(row['column_name'], 'utf-8')
            name = quote_ident(name)
            i = [name,ct,None]
            self.source_columns.append(i)
            self.log(self.DEBUG, "found input column: " + str(i))
        #CLIN print('read_table_metadata - self.into_columns: %s' % self.into_columns) #CLIN
        #CLIN print('read_table_metadata - self.into_columns_dict: %s' % self.into_columns_dict) #CLIN

    def read_mapping(self):
        mapping = self.getconfig('gpunload:output:mapping',dict,None, returnOriginal=True)
        #CLIN print('read_mapping mapping: %s' % mapping)

        self.flag_mapping = False
        for x in self.output_columns:
            #CLIN print ('read_mapping - x before: %s' % x)
            # Check to see if it already has a mapping value
            found = False
            if mapping:
                self.flag_mapping = True
                for key,value in mapping.iteritems():
                    #CLIN print ('read_mapping - key/value 1: %s/%s' % (key,value))
                    if type(key) != unicode or type(value) != unicode:
                        self.control_file_error("gpunload:output:mapping must be a YAML type mapping from strings to strings")
                    if sqlIdentifierCompare(x[0], key) == True:
                       x[2] = value
                       found = True
                       break
            if found == False:
                x[2] = x[0]
            #CLIN print ('read_mapping - x after: %s' % x)
        
        for name,typ,mapto in self.output_columns:
            self.log(self.DEBUG,'%s: %s = %s'%(name,typ,mapto))

       
    # In order to find out whether we have an existing external table in the 
    # catalog which could be reused for this operation we need to make sure
    # that it has the same column names and types, the same data format, and
    # location specification, and single row error handling specs.
    #
    # This function will return the SQL to run in order to find out whether
    # such a table exists.
    # 
    def get_reuse_exttable_query(self, formatType, formatOpts, distKeyList,  output_cols, schemaName):
        sqlFormat = """select attrelid::regclass
                 from (
                        select 
                            attrelid, 
                            row_number() over (partition by attrelid order by attnum) as attord, 
                            attnum,
                            attname,
                            atttypid,
                            atttypmod
                        from 
                            pg_attribute
                            join
                            pg_class
                            on (pg_class.oid = attrelid)
                            %s
                        where 
                            relstorage = 'x' and
                            relname like 'ext_gpunload_reusable_%%' and
                            attnum > 0 and
                            not attisdropped and %s
                    ) pgattr 
                    join 
                    pg_exttable pgext 
                    on(pgattr.attrelid = pgext.reloid) 
                    """
        joinStr = ""
        conditionStr = ""
       
        # if schemaName is None, find the resuable ext table which is visible to
        # current search path. Else find the resuable ext table under the specific
        # schema, and this needs to join pg_namespace.
        if schemaName is None:
            joinStr = ""
            conditionStr = "pg_table_is_visible(pg_class.oid)"
        else:
            joinStr = """join
                         pg_namespace pgns
                         on(pg_class.relnamespace = pgns.oid)
                      """
            conditionStr = "pgns.nspname = '%s'" % schemaName

        sql = sqlFormat % (joinStr, conditionStr)

        for i, l in enumerate(self.locations):
            sql += " and pgext.location[%s] = %s\n" % (i + 1, quote(l))
             
        sql+= """and pgext.fmttype = %s
                 and pgext.writable = true
                 and replace(pgext.fmtopts,E'\\\\',E'\\\\\\\\') = %s """ % (quote(formatType[0]),quote(quote_unident(formatOpts.rstrip())))

        sql+= "group by attrelid " 

        sql+= """having 
                    count(*) = %s and 
                    bool_and(case """ % len(output_cols)
        
        for i, c in enumerate(output_cols):
            name = c[0]
            typ = c[1]
            sql+= "when attord = %s then format_type(atttypid, atttypmod) = %s and attname = %s\n" % (i+1, quote(typ), quote(quote_unident(name)))

        sql+= """else true 
                 end)
                 limit 1;"""
        
        self.log(self.DEBUG, "query used to identify reusable external relations: %s" % sql)
        return sql


    
    def get_ext_schematable(self, schemaName, tableName):
        if schemaName == None:
            return tableName
        else:
            schemaTable = "%s.%s" % (schemaName, tableName)
            return schemaTable

    def final_delimiter(self,formatType):
        delimiterValue = self.getconfig('gpunload:output:delimiter', unicode, False)
        self.formatOptDelimiter = "delimiter '%s' " % delimiterValue
        if isinstance(delimiterValue, bool) and delimiterValue == False:
            """ implies the DELIMITER option has no value in the yaml file, so use default """
            if formatType=='csv':
                delimiterValue = ","
                
            else:
                delimiterValue = "\t"
            self.formatOptDelimiter = "delimiter '%s' " % delimiterValue
        elif len(delimiterValue) != 1:
            # is a escape sequence character like '\x1B' or '\u001B'?
            if len(delimiterValue.decode('unicode-escape')) == 1:
                delimiterValue = delimiterValue.strip("'")
            # is a escape string syntax support by gpdb like E'\x1B' or E'\x\u001B'
            elif len(delimiterValue.lstrip("E'").rstrip("'").decode('unicode-escape')) ==1:
                self.formatOptDelimiter = "delimiter %s " % delimiterValue
                delimiterValue = delimiterValue.lstrip("E'").rstrip("'").decode('unicode-escape')
            else:
                self.control_file_warning('''A delimiter must be single ASCII charactor, you can also use unprintable characters(for example: '\\x1c' / E'\\x1c' or '\\u001c' / E'\\u001c') ''')
                self.control_file_error("Invalid delimiter, gpunload quit immediately")
                sys.exit(2);

        return delimiterValue

    # 
    # Create a new external table or find a reusable external table to use for this operation
    #    
    def create_external_table(self):

        # extract all control file information and transform it accordingly
        # in order to construct a CREATE EXTERNAL TABLE statement if will be
        # needed later on
        
        formatType = self.getconfig('gpunload:output:format', unicode, 'text').lower()
        formatOpts = ""
        locationStr = ','.join(map(quote,self.locations))

        formatOpts = self.formatOptDelimiter
        nullas = self.getconfig('gpunload:output:null_as', unicode, False)
        self.log(self.DEBUG, "null " + unicode(nullas))
        if nullas != False: # could be empty string
            formatOpts += "null %s " % quote(nullas)
        elif formatType=='csv':
            formatOpts += "null '' "
        else:
            formatOpts += "null %s " % quote("\N")

        esc = self.getconfig('gpunload:output:escape', None, None)
        if esc:
            if type(esc) != unicode and type(esc) != str:
                self.control_file_error("gpunload:output:escape must be a string")
            if esc.lower() == 'off':
                if formatType == 'csv':
                    self.control_file_error("ESCAPE cannot be set to OFF in CSV format")
                formatOpts += "escape 'off' "
            else:
                formatOpts += "escape %s " % quote(esc)
        else:
            if formatType=='csv':
                formatOpts += "escape %s " % quote(self.getconfig('gpunload:output:quote', 
                    unicode, extraStuff='for csv formatted data'))
            else:
                formatOpts += "escape %s " % quote("\\")

        if formatType=='csv':
            formatOpts += "quote %s " % quote(self.getconfig('gpunload:output:quote', 
                    unicode, extraStuff='for csv formatted data'))

        force_quote = self.getconfig('gpunload:output:force_quote',list,[])
        if force_quote:
            for i in force_quote:
                if type(i) != unicode and type(i) != str:
                    self.control_file_error("gpunload:output:force_quote must be a YAML sequence of strings")
            formatOpts += "force quote %s " % ','.join(force_quote)

        encodingStr = self.getconfig('gpunload:output:encoding', unicode, None)

        self.extSchemaName = self.getconfig('gpunload:external:schema', unicode, None)
        if self.extSchemaName == '%':
            self.extSchemaName = self.schema

        # get the list of columns to use in the external table
        output_cols = self.output_columns

        distKeyList = ''
        if self.flag_mapping == False:
            distKeyList = self.get_table_dist_key()

        # If the 'reuse tables' option was specified we now try to find an
        # already existing external table in the catalog which will match
        # the one that we need to use. It must have identical attributes,
        # external location, format, and encoding specifications.
        if self.reuse_tables == True:
            sql = self.get_reuse_exttable_query(formatType, formatOpts,
                    distKeyList, output_cols, self.extSchemaName)
            #CLIN print('sql ext table: %s' % sql) # CLIN
            resultList = self.db.query(sql.encode('utf-8')).getresult()
            if len(resultList) > 0:
                # found an external table to reuse. no need to create one. we're done here.
                self.extTableName = (resultList[0])[0]
                self.extSchemaTable = self.extTableName
                self.log(self.INFO, "reusing external table %s" % self.extSchemaTable)
                return

            # didn't find an existing external table suitable for reuse. Format a reusable
            # name and issue a CREATE EXTERNAL TABLE on it. Hopefully we can use it next time
            # around

            self.extTableName = "ext_gpunload_reusable_%s" % self.unique_suffix
            self.log(self.INFO, "did not find an external table to reuse. creating %s" % self.extTableName)
        
        # construct a CREATE EXTERNAL TABLE statement and execute it
        self.extSchemaTable = self.get_ext_schematable(self.extSchemaName, self.extTableName)
        sql = "create writable external table %s" % self.extSchemaTable
        sql += "(%s)" % ','.join(map(lambda a:'%s %s' % (a[0], a[1]), output_cols))

        sql += "location(%s) "%locationStr
        sql += "format%s "% quote(formatType)
        if len(formatOpts) > 0:
            sql += "(%s) "% formatOpts
        if encodingStr:
            sql += "encoding%s "%quote(encodingStr)

        if len(distKeyList) > 0:
            sql += " DISTRIBUTED BY (%s)" % ','.join(distKeyList)
        try:
            self.db.query(sql.encode('utf-8'))
        except Exception, e:
            self.log(self.ERROR, 'could not run SQL "%s": %s' % (sql, unicode(e)))

        # set up to drop the external table at the end of operation, unless user
        # specified the 'reuse_tables' option, in which case we don't drop
        if self.reuse_tables == False:
            self.cleanupSql.append('drop external table if exists %s'%self.extSchemaTable)

    def do_extract(self, dest):
        """
        Handle the INSERT case
        """
        self.log(self.DEBUG, "output columns " + str(self.output_columns))
        cols = filter(lambda a:a[2]!=None, self.output_columns)


        sql = 'INSERT INTO %s' % self.extSchemaTable #CLIN
        sql += ' (%s)' % ','.join(map(lambda a:a[0], cols))
        sql += ' SELECT %s' % ','.join(map(lambda a:a[2], cols))
        sql += ' FROM %s' % dest #CLIN
        extract_condition = self.getconfig('gpunload:input:extract_condition',
                            unicode, None)
        extract_query = self.getconfig('gpunload:input:extract_query',
                            unicode, None)
        #CLIN print('extract_condition : %s' % extract_condition)

        if extract_condition:
            sql += ' where %s' % extract_condition

        #CLIN print('extract sql: %s' % sql)
        # cktan: progress thread is not reliable. revisit later.
        #progress = Progress(self,self.ports)
        #progress.start()
        #self.threads.append(progress)
        self.log(self.LOG, sql)
        if not self.options.D:
            try:
                self.rowsExtracted = self.db.query(sql.encode('utf-8'))
            except Exception, e:
                # We need to be a bit careful about the error since it may contain non-unicode characters
                strE = unicode(str(e), errors = 'ignore')
                strF = unicode(str(sql), errors = 'ignore')
                self.log(self.ERROR, strE + ' encountered while running ' + strF)

    def do_method_extract(self):
        self.create_external_table()
        self.do_extract(self.get_qualified_tablename())


    def do_extract_condition(self,fromname,index):
        extract_condition = self.getconfig('gpunload:input:extract_condition',
                            unicode, None)
        #CLIN print('extract_condition : %s' % extract_condition)
        sql_where_clause = None
        if extract_condition:
            sql_where_clause = ' where %s' % extract_condition
        return sql_where_clause
				
    def get_qualified_tablename(self):
    
        tblname = "%s.%s" % (self.schema, self.table)        
        return tblname
    
    def get_table_dist_key(self):
                
        # NOTE: this query should be re-written better. the problem is that it is
        # not possible to perform a cast on a table name with spaces...
        sql = "select attname from pg_attribute a, gp_distribution_policy p , pg_class c, pg_namespace n "+\
              "where a.attrelid = c.oid and " + \
              "a.attrelid = p.localoid and " + \
              "a.attnum = any (p.attrnums) and " + \
              "c.relnamespace = n.oid and " + \
              "n.nspname = '%s' and c.relname = '%s'; " % (quote_unident(self.schema), quote_unident(self.table))
              
              
        resultList = self.db.query(sql.encode('utf-8')).getresult()
        attrs = []
        count = 0
        while count < len(resultList):
            attrs.append((resultList[count])[0])
            count = count + 1

        return attrs


    def do_method(self):
        # sql pre or post processing?
        sql = self.getconfig('gpunload:sql', list, default=None)
        before   = None
        after    = None
        if sql:
            before   = self.getconfig('gpunload:sql:before', unicode, default=None)
            after    = self.getconfig('gpunload:sql:after', unicode, default=None)
        if before:
            self.log(self.LOG, "Pre-SQL from user: %s" % before)
            if not self.options.D:
                try:
                    self.db.query(before.encode('utf-8'))
                except Exception, e:
                    self.log(self.ERROR, 'could not execute SQL in sql:before "%s": %s' %
                             (before, str(e)))

        
        self.do_method_extract()

        if after:
            self.log(self.LOG, "Post-SQL from user: %s" % after)
            if not self.options.D:
                try:
                    self.db.query(after.encode('utf-8'))
                except Exception, e:
                    self.log(self.ERROR, 'could not execute SQL in sql:after "%s": %s' %
                             (after, str(e)))

    def run2(self):
        start = time.time()
        if self.flag_configFilename==True:
            self.log(self.DEBUG, 'config ' + str(self.config))
        self.read_config()
        self.setup_connection()
        self.read_table_metadata()
        self.read_columns()
        self.read_mapping()
        self.start_gpfdists()
        self.do_method()
        self.log(self.INFO, 'running time: %.2f seconds'%(time.time()-start))

    def run(self):
        self.db = None
        self.rowsExtracted = 0
        signal.signal(signal.SIGINT, handle_kill)
        signal.signal(signal.SIGTERM, handle_kill)
        # win32 doesn't do SIGQUIT
        if not platform.system() in ['Windows', 'Microsoft']:
            signal.signal(signal.SIGQUIT, handle_kill)
            signal.signal(signal.SIGHUP, signal.SIG_IGN)

        try:
            try:
                self.run2()
            except Exception:
                traceback.print_exc(file=self.logfile)
                self.logfile.flush()
                self.exitValue = 2
                if (self.options.qv > self.INFO):
                    traceback.print_exc()
                else:
                    self.log(self.ERROR, "unexpected error -- backtrace " +
                             "written to log file")
        finally:
            if self.cleanupSql:
                self.log(self.LOG, 'removing temporary data')
                self.setup_connection()
                for a in self.cleanupSql:
                    try:
                        self.log(self.DEBUG, a)
                        self.db.query(a.encode('utf-8'))
                    except Exception:
                        traceback.print_exc(file=self.logfile)
                        self.logfile.flush()
                        traceback.print_exc()
            if self.subprocesses:
                self.log(self.LOG, 'killing gpfdist')
                for a in self.subprocesses:
                    try:
                        if platform.system() in ['Windows', 'Microsoft']:
                            # win32 API is better but hard for us
                            # to install, so we use the crude method
                            subprocess.Popen("taskkill /F /T /PID %i" % a.pid,
                                             shell=True, stdout=subprocess.PIPE,
                                             stderr=subprocess.PIPE) 

                        else:
                            os.kill(a.pid, signal.SIGTERM)
                    except OSError:
                        pass
            for t in self.threads:
                t.join()

            self.log(self.INFO, 'rows Extracted          = ' + str(self.rowsExtracted))
            if self.exitValue==0:
                self.log(self.INFO, 'gpunload succeeded')
            else:
                self.log(self.INFO, 'gpunload failed')

            ## MPP-19015 - Extra python thread shutdown time is needed on HP-UX
            if platform.uname()[0] == 'HP-UX':
                time.sleep(1)


if __name__ == '__main__':
    g = gpunload(sys.argv[1:])
    g.run()
    sys.exit(g.exitValue)
