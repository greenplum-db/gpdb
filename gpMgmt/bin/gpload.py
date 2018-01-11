#!/usr/bin/env python
# -*- coding: utf-8 -*-
# gpload - load file(s) into Greenplum Database
# Copyright Greenplum 2008

'''gpload [options] -f configuration file

Options:
    -h hostname: host to connect to
    -p port: port to connect to
    -U username: user to connect as
    -d database: database to connect to
    -W: force password authentication
    -q: quiet mode
    -D: do not actually load data
    -v: verbose
    -V: very verbose
    -l logfile: log output to logfile
    --no_auto_trans: do not wrap gpload in transaction
    --gpfdist_timeout timeout: gpfdist timeout value
    --version: print version number and exit
    -?: help
'''

import sys
if sys.hexversion<0x2040400:
    sys.stderr.write("gpload needs python 2.4.4 or higher\n")
    sys.exit(2)

try:
    import yaml
except ImportError:
    sys.stderr.write("gpload needs pyyaml.  You can get it from http://pyyaml.org.\n")
    sys.exit(2)

import platform
try:
    from pygresql import pg
except Exception, e:
    from struct import calcsize
    sysWordSize = calcsize("P") * 8
    if (platform.system()) in ['Windows', 'Microsoft'] and (sysWordSize == 64):
        errorMsg = "gpload appears to be running in 64-bit Python under Windows.\n"
        errorMsg = errorMsg + "Currently only 32-bit Python is supported. Please \n"
        errorMsg = errorMsg + "reinstall a 32-bit Python interpreter.\n"
    else:
        errorMsg = "gpload was unable to import The PyGreSQL Python module (pg.py) - %s\n" % str(e)
    sys.stderr.write(str(errorMsg))
    sys.exit(2)

import hashlib
import datetime,getpass,os,signal,socket,subprocess,threading,time,traceback,re
import uuid
import socket

thePlatform = platform.system()
if thePlatform in ['Windows', 'Microsoft']:
   windowsPlatform = True
else:
   windowsPlatform = False

if windowsPlatform == False:
   import select


EXECNAME = 'gpload'

NUM_WARN_ROWS = 0

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
    "port": {'parse_children': True, 'parent': [None, "source"]},
    "password": {'parse_children': True, 'parent': None},
    "gpload": {'parse_children': True, 'parent': None},
    "input": {'parse_children': True, 'parent': "gpload"},
    "source": {'parse_children': True, 'parent': "input"},
    "local_hostname": {'parse_children': False, 'parent': "source"},
    "port_range": {'parse_children': False, 'parent': "source"},
    "file": {'parse_children': False, 'parent': "source"},
    "ssl": {'parse_children': False, 'parent': "source"},
    "certificates_path": {'parse_children': False, 'parent': "source"},
    "columns": {'parse_children': False, 'parent': "input"},
    "transform": {'parse_children': True, 'parent': "input"},
    "transform_config": {'parse_children': True, 'parent': "input"},
    "max_line_length": {'parse_children': True, 'parent': "input"},
    "format": {'parse_children': True, 'parent': "input"},
    "delimiter": {'parse_children': True, 'parent': "input"},
    "escape": {'parse_children': True, 'parent': "input"},
    "null_as": {'parse_children': True, 'parent': "input"},
    "quote": {'parse_children': True, 'parent': "input"},
    "encoding": {'parse_children': True, 'parent': "input"},
    "force_not_null": {'parse_children': False, 'parent': "input"},
    "error_limit": {'parse_children': True, 'parent': "input"},
    "error_percent": {'parse_children': True, 'parent': "input"},
    "error_table": {'parse_children': True, 'parent': "input"},
    "log_errors": {'parse_children': False, 'parent': "input"},
    "header": {'parse_children': True, 'parent': "input"},
    "fully_qualified_domain_name": {'parse_children': False, 'parent': 'input'},
    "output": {'parse_children': True, 'parent': "gpload"},
    "table": {'parse_children': True, 'parent': "output"},
    "mode": {'parse_children': True, 'parent': "output"},
    "match_columns": {'parse_children': False, 'parent': "output"},
    "update_columns": {'parse_children': False, 'parent': "output"},
    "update_condition": {'parse_children': True, 'parent': "output"},
    "mapping": {'parse_children': False, 'parent': "output"},
    "including_defaults": {'parse_children': False, 'parent': 'output'},
    "preload": {'parse_children': True, 'parent': 'gpload'},
    "truncate": {'parse_children': False, 'parent': 'preload'},
    "reuse_tables": {'parse_children': False, 'parent': 'preload'},
    "sql": {'parse_children': True, 'parent': 'gpload'},
    "before": {'parse_children': False, 'parent': 'sql'},
    "after": {'parse_children': False, 'parent': 'sql'},
    "external": {'parse_children': True, 'parent': 'gpload'},
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
keywords = {
	"abort": True,
	"absolute": True,
	"access": True,
	"action": True,
	"active": True,
	"add": True,
	"admin": True,
	"after": True,
	"aggregate": True,
	"all": True,
	"also": True,
	"alter": True,
	"analyse": True,
	"analyze": True,
	"and": True,
	"any": True,
	"array": True,
	"as": True,
	"asc": True,
	"assertion": True,
	"assignment": True,
	"asymmetric": True,
	"at": True,
	"authorization": True,
	"backward": True,
	"before": True,
	"begin": True,
	"between": True,
	"bigint": True,
	"binary": True,
	"bit": True,
	"boolean": True,
	"both": True,
	"by": True,
	"cache": True,
	"called": True,
	"cascade": True,
	"cascaded": True,
	"case": True,
	"cast": True,
	"chain": True,
	"char": True,
	"character": True,
	"characteristics": True,
	"check": True,
	"checkpoint": True,
	"class": True,
	"close": True,
	"cluster": True,
	"coalesce": True,
	"collate": True,
	"column": True,
	"comment": True,
	"commit": True,
	"committed": True,
	"concurrently": True,
	"connection": True,
	"constraint": True,
	"constraints": True,
	"conversion": True,
	"convert": True,
	"copy": True,
	"cost": True,
	"create": True,
	"createdb": True,
	"createrole": True,
	"createuser": True,
	"cross": True,
	"csv": True,
	"cube": True,
	"current": True,
	"current_date": True,
	"current_role": True,
	"current_time": True,
	"current_timestamp": True,
	"current_user": True,
	"cursor": True,
	"cycle": True,
	"database": True,
	"day": True,
	"deallocate": True,
	"dec": True,
	"decimal": True,
	"declare": True,
	"default": True,
	"defaults": True,
	"deferrable": True,
	"deferred": True,
	"definer": True,
	"delete": True,
	"delimiter": True,
	"delimiters": True,
	"desc": True,
	"disable": True,
	"distinct": True,
	"distributed": True,
	"do": True,
	"domain": True,
	"double": True,
	"drop": True,
	"each": True,
	"else": True,
	"enable": True,
	"encoding": True,
	"encrypted": True,
	"end": True,
	"errors": True,
	"escape": True,
	"every": True,
	"except": True,
	"exchange": True,
	"exclude": True,
	"excluding": True,
	"exclusive": True,
	"execute": True,
	"exists": True,
	"explain": True,
	"external": True,
	"extract": True,
	"false": True,
	"fetch": True,
	"fields": True,
	"fill": True,
	"filter": True,
	"first": True,
	"float": True,
	"following": True,
	"for": True,
	"force": True,
	"foreign": True,
	"format": True,
	"forward": True,
	"freeze": True,
	"from": True,
	"full": True,
	"function": True,
	"global": True,
	"grant": True,
	"granted": True,
	"greatest": True,
	"group": True,
	"group_id": True,
	"grouping": True,
	"handler": True,
	"hash": True,
	"having": True,
	"header": True,
	"hold": True,
	"host": True,
	"hour": True,
	"if": True,
	"ignore": True,
	"ilike": True,
	"immediate": True,
	"immutable": True,
	"implicit": True,
	"in": True,
	"including": True,
	"inclusive": True,
	"increment": True,
	"index": True,
	"indexes": True,
	"inherit": True,
	"inherits": True,
	"initially": True,
	"inner": True,
	"inout": True,
	"input": True,
	"insensitive": True,
	"insert": True,
	"instead": True,
	"int": True,
	"integer": True,
	"intersect": True,
	"interval": True,
	"into": True,
	"invoker": True,
	"is": True,
	"isnull": True,
	"isolation": True,
	"join": True,
	"keep": True,
	"key": True,
	"lancompiler": True,
	"language": True,
	"large": True,
	"last": True,
	"leading": True,
	"least": True,
	"left": True,
	"level": True,
	"like": True,
	"limit": True,
	"list": True,
	"listen": True,
	"load": True,
	"local": True,
	"localtime": True,
	"localtimestamp": True,
	"location": True,
	"lock": True,
	"log": True,
	"login": True,
	"master": True,
	"match": True,
	"maxvalue": True,
	"merge": True,
	"minute": True,
	"minvalue": True,
	"mirror": True,
	"missing": True,
	"mode": True,
	"modify": True,
	"month": True,
	"move": True,
	"names": True,
	"national": True,
	"natural": True,
	"nchar": True,
	"new": True,
	"next": True,
	"no": True,
	"nocreatedb": True,
	"nocreaterole": True,
	"nocreateuser": True,
	"noinherit": True,
	"nologin": True,
	"none": True,
	"noovercommit": True,
	"nosuperuser": True,
	"not": True,
	"nothing": True,
	"notify": True,
	"notnull": True,
	"nowait": True,
	"null": True,
	"nullif": True,
	"numeric": True,
	"object": True,
	"of": True,
	"off": True,
	"offset": True,
	"oids": True,
	"old": True,
	"on": True,
	"only": True,
	"operator": True,
	"option": True,
	"or": True,
	"order": True,
	"others": True,
	"out": True,
	"outer": True,
	"over": True,
	"overcommit": True,
	"overlaps": True,
	"overlay": True,
	"owned": True,
	"owner": True,
	"partial": True,
	"partition": True,
	"partitions": True,
	"password": True,
	"percent": True,
	"placing": True,
	"position": True,
	"preceding": True,
	"precision": True,
	"prepare": True,
	"prepared": True,
	"preserve": True,
	"primary": True,
	"prior": True,
	"privileges": True,
	"procedural": True,
	"procedure": True,
	"queue": True,
	"quote": True,
	"randomly": True,
	"range": True,
	"read": True,
	"real": True,
	"reassign": True,
	"recheck": True,
	"references": True,
	"reindex": True,
	"reject": True,
	"relative": True,
	"release": True,
	"rename": True,
	"repeatable": True,
	"replace": True,
	"reset": True,
	"resource": True,
	"restart": True,
	"restrict": True,
	"returning": True,
	"returns": True,
	"revoke": True,
	"right": True,
	"role": True,
	"rollback": True,
	"rollup": True,
	"row": True,
	"rows": True,
	"rule": True,
	"savepoint": True,
	"schema": True,
	"scroll": True,
	"second": True,
	"security": True,
	"segment": True,
	"select": True,
	"sequence": True,
	"serializable": True,
	"session": True,
	"session_user": True,
	"set": True,
	"setof": True,
	"sets": True,
	"share": True,
	"show": True,
	"similar": True,
	"simple": True,
	"smallint": True,
	"some": True,
	"split": True,
	"stable": True,
	"start": True,
	"statement": True,
	"statistics": True,
	"stdin": True,
	"stdout": True,
	"storage": True,
	"strict": True,
	"subpartition": True,
	"subpartitions": True,
	"substring": True,
	"superuser": True,
	"symmetric": True,
	"sysid": True,
	"system": True,
	"table": True,
	"tablespace": True,
	"temp": True,
	"template": True,
	"temporary": True,
	"then": True,
	"threshold": True,
	"ties": True,
	"time": True,
	"timestamp": True,
	"to": True,
	"trailing": True,
	"transaction": True,
	"transform": True,
	"treat": True,
	"trigger": True,
	"trim": True,
	"true": True,
	"truncate": True,
	"trusted": True,
	"type": True,
	"unbounded": True,
	"uncommitted": True,
	"unencrypted": True,
	"union": True,
	"unique": True,
	"unknown": True,
	"unlisten": True,
	"until": True,
	"update": True,
	"user": True,
	"using": True,
	"vacuum": True,
	"valid": True,
	"validation": True,
	"validator": True,
	"values": True,
	"varchar": True,
	"varying": True,
	"verbose": True,
	"view": True,
	"volatile": True,
	"web": True,
	"when": True,
	"where": True,
	"window": True,
	"with": True,
	"without": True,
	"work": True,
	"write": True,
	"year": True,
	"zone": True
}

def is_keyword(tab):
    if tab in keywords:
        return True
    else:
        return False


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


def notice_processor(self):
    if windowsPlatform == True:
       # We don't have a pygresql with our notice fix, so skip for windows.
       # This means we will not get any warnings on windows (MPP10989).
       return

    theNotices = self.db.notices()
    r = re.compile("^NOTICE:  Found (\d+) data formatting errors.*")
    messageNumber = 0
    m = None
    while messageNumber < len(theNotices) and m == None:
       aNotice = theNotices[messageNumber]
       m = r.match(aNotice)
       messageNumber = messageNumber + 1
       if m:
           global NUM_WARN_ROWS
           NUM_WARN_ROWS = int(m.group(1))

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
    def __init__(self,gpload,fd, sharedLock = None):
        threading.Thread.__init__(self)
        self.gpload = gpload
        self.fd = fd
        self.theLock = sharedLock

    def run(self):
        try:
            if windowsPlatform == True:
                while 1:
                    # Windows select does not support select on non-file fd's, so we can use the lock fix. Deadlock is possible here.
                    # We need to look into the Python windows module to see if there is another way to do this in Windows.
                    line = self.fd.readline()
                    if line=='':
                        break
                    self.gpload.log(self.gpload.DEBUG, 'gpfdist: ' + line.strip('\n'))
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
                    self.gpload.log(self.gpload.DEBUG, 'gpfdist: ' + line.strip('\n'))
        except Exception, e:
            # close fd so that not block the worker thread because of stdout/stderr pipe not finish/closed.
            self.fd.close()
            sys.stderr.write("\n\nWarning: gpfdist log halt because Log Thread '%s' got an exception: %s \n" % (self.getName(), str(e)))
            self.gpload.log(self.gpload.WARN, "gpfdist log halt because Log Thread '%s' got an exception: %s" % (self.getName(), str(e)))
            raise

class Progress(threading.Thread):
    """
    Determine our progress from the gpfdist daemon
    """
    def __init__(self,gpload,ports):
        threading.Thread.__init__(self)
        self.gpload = gpload
        self.ports = ports
        self.number = 0
        self.condition = threading.Condition()

    def get(self,port):
        """
        Connect to gpfdist and issue an HTTP query. No need to do this with
        httplib as the transaction is extremely simple
        """
        addrinfo = socket.getaddrinfo('localhost', port)
        s = socket.socket(addrinfo[0][0],socket.SOCK_STREAM)
        s.connect(('localhost',port))
        s.sendall('GET gpfdist/status HTTP/1.0\r\n\r\n')
        f = s.makefile()
        read_bytes = -1
        total_bytes = -1
        total_sessions = -1
        for line in f:
            self.gpload.log(self.gpload.DEBUG, "gpfdist stat: %s" % \
                        line.strip('\n'))
            a = line.split(' ')
            if not a:
                continue
            if a[0]=='read_bytes':
                read_bytes = int(a[1])
            elif a[0]=='total_bytes':
                total_bytes = int(a[1])
            elif a[0]=='total_sessions':
                total_sessions = int(a[1])
        s.close()
        f.close()
        return read_bytes,total_bytes,total_sessions

    def get1(self):
        """
        Parse gpfdist output
        """
        read_bytes = 0
        total_bytes = 0
        for port in self.ports:
            a = self.get(port)
            if a[2]<1:
                return
            if a[0]!=-1:
                read_bytes += a[0]
            if a[1]!=-1:
                total_bytes += a[1]
        self.gpload.log(self.gpload.INFO,'transferred %s of %s' % \
            (bytestr(read_bytes),bytestr(total_bytes)))

    def run(self):
        """
        Thread worker
        """
        while 1:
            try:
                self.condition.acquire()
                n = self.number
                self.condition.release()
                self.get1()
                if n:
                    self.gpload.log(self.gpload.DEBUG, "gpfdist status thread told to stop")
                    self.condition.acquire()
                    self.condition.notify()
                    self.condition.release()
                    break
            except socket.error, e:
                self.gpload.log(self.gpload.DEBUG, "got socket exception: %s" % e)
                break
            time.sleep(1)
def cli_help():
    help_path = os.path.join(sys.path[0], '..', 'docs', 'cli_help', EXECNAME +
                             '_help');
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

#
# MPP-13348
#

'''Jenkins hash - http://burtleburtle.net/bob/hash/doobs.html'''

def jenkinsmix(a, b, c):
    a &= 0xffffffff; b &= 0xffffffff; c &= 0xffffffff
    a -= b; a -= c; a ^= (c>>13); a &= 0xffffffff
    b -= c; b -= a; b ^= (a<<8); b &= 0xffffffff
    c -= a; c -= b; c ^= (b>>13); c &= 0xffffffff
    a -= b; a -= c; a ^= (c>>12); a &= 0xffffffff
    b -= c; b -= a; b ^= (a<<16); b &= 0xffffffff
    c -= a; c -= b; c ^= (b>>5); c &= 0xffffffff
    a -= b; a -= c; a ^= (c>>3); a &= 0xffffffff
    b -= c; b -= a; b ^= (a<<10); b &= 0xffffffff
    c -= a; c -= b; c ^= (b>>15); c &= 0xffffffff
    return a, b, c


def jenkins(data, initval = 0):
    length = lenpos = len(data)
    if length == 0:
        return 0
    a = b = 0x9e3779b9
    c = initval
    p = 0
    while lenpos >= 12:
        a += (ord(data[p+0]) + (ord(data[p+1])<<8) + (ord(data[p+2])<<16) + (ord(data[p+3])<<24))
        b += (ord(data[p+4]) + (ord(data[p+5])<<8) + (ord(data[p+6])<<16) + (ord(data[p+7])<<24))
        c += (ord(data[p+8]) + (ord(data[p+9])<<8) + (ord(data[p+10])<<16) + (ord(data[p+11])<<24))
        a, b, c = jenkinsmix(a, b, c)
        p += 12
        lenpos -= 12
    c += length
    if lenpos >= 11: c += ord(data[p+10])<<24
    if lenpos >= 10: c += ord(data[p+9])<<16
    if lenpos >= 9:  c += ord(data[p+8])<<8
    if lenpos >= 8:  b += ord(data[p+7])<<24
    if lenpos >= 7:  b += ord(data[p+6])<<16
    if lenpos >= 6:  b += ord(data[p+5])<<8
    if lenpos >= 5:  b += ord(data[p+4])
    if lenpos >= 4:  a += ord(data[p+3])<<24
    if lenpos >= 3:  a += ord(data[p+2])<<16
    if lenpos >= 2:  a += ord(data[p+1])<<8
    if lenpos >= 1:  a += ord(data[p+0])
    a, b, c = jenkinsmix(a, b, c)
    return c

# MPP-20927 Citibank: gpload external table name problem
# Not sure if it is used by other components, just leave it here.
def shortname(name):
    """
    Returns a 10 character string formed by concatenating the first two characters
    of the name with another 8 character string computed using the Jenkins hash
    function of the table name. When the original name has only a single non-space
    ascii character, we return '00' followed by 8 char hash.

    For example:

    >>> shortname('mytable')
    'my3cbb7ba8'
    >>> shortname('some_pretty_long_test_table_name')
    'so9068664a'
    >>> shortname('t')
    '006742be70'

    @param name: the input tablename
    @returns:    a string 10 characters or less built from the table name
    """

    # Remove spaces from original name
    name = re.sub(r' ', '', name)

    # Run the hash function
    j = jenkins(name)

    # Now also remove non ascii chars from original name.
    # We do this after jenkins so that we exclude the
    # (very rare) case of passing an empty string to jenkins
    name = "".join(i for i in name if ord(i) < 128)

    if len(name) > 1:
        return '%2s%08x' % (name[0:2], j)
    else:
        return '00%08x' % (j) # could be len 0 or 1

class options:
    pass

class gpload:
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
        self.options.no_auto_trans = False
        self.options.password = None
        self.options.d = None
        self.DEBUG = 5
        self.LOG = 4
        self.INFO = 3
        self.WARN = 2
        self.ERROR = 1
        self.options.qv = self.INFO
        self.options.l = None
        self.formatOpts = ""
        self.startTimestamp = time.time()
        self.error_table = False
        seenv = False
        seenq = False

        # Create Temp and External table names. However external table name could
        # get overwritten with another name later on (see create_external_table_name).
        # MPP-20927 Citibank: gpload external table name problem. We use uuid to avoid
        # external table name confliction.
        self.unique_suffix = str(uuid.uuid1()).replace('-', '_')
        self.staging_table_name = 'temp_staging_gpload_' + self.unique_suffix
        self.extTableName  = 'ext_gpload_' + self.unique_suffix

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
                    elif argv[0]=='-q':
                        self.options.qv -= 1
                        argv = argv[1:]
                        seenq = True
                    elif argv[0]=='--version':
                        sys.stderr.write("gpload version $Revision$\n")
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
                    elif argv[0]=='--no_auto_trans':
                        self.options.no_auto_trans = True
                        argv = argv[1:]
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

        if configFilename==None:
            usage('configuration file required')
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

            self.options.l = os.path.join(self.options.l, 'gpload_' + \
                                          datetime.date.today().strftime('%Y%m%d') + '.log')

        try:
            self.logfile = open(self.options.l,'a')
        except Exception, e:
            self.log(self.ERROR, "could not open logfile %s: %s" % \
                      (self.options.l, e))

        if seenv and seenq:
            self.log(self.ERROR, "-q conflicts with -v and -V")

        if self.options.D:
            self.log(self.INFO, 'gpload has the -D option, so it does not actually load any data')

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
            ver = self.getconfig('version', unicode, extraStuff = ' tag')
            if ver != '1.0.0.1':
                self.control_file_error("gpload configuration schema version must be 1.0.0.1")
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
        self.subprocesses = []
        self.log(self.INFO,'gpload session started ' + \
                 datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S'))

    def control_file_warning(self, msg):
        self.log(self.WARN, "A gpload control file processing warning occurred. %s" % msg)

    def control_file_error(self, msg):
        self.log(self.ERROR, "A gpload control file processing error occurred. %s" % msg)

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
        try:
            t = time.localtime()
            str = '|'.join(
                       [datetime.datetime.today().strftime('%Y-%m-%d %H:%M:%S'),
                        self.elevel2str(level), a]) + '\n'

            str = str.encode('utf-8')
        except Exception, e:
            # log even if contains non-utf8 data and pass this exception
            self.logfile.write("\nWarning: Log() threw an exception: %s \n" % (e))

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
                    self.control_file_error("The configuration must contain %s%s"%(a,extraStuff))
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

        # ensure output is of type list
        self.getconfig('gpload:output', list)

        # The user supplied table name can be completely or partially delimited,
        # and it can be a one or two part name. Get the originally supplied name
        # and parse it into its delimited one or two part name.
        self.schemaTable = self.getconfig('gpload:output:table', unicode, returnOriginal=True)
        schemaTableList  = splitUpMultipartIdentifier(self.schemaTable)
        schemaTableList  = convertListToDelimited(schemaTableList)
        if len(schemaTableList) == 2:
           self.schema = schemaTableList[0]
           self.table  = schemaTableList[1]
        else:
           self.schema = None
           self.table  = schemaTableList[0]

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

        if self.getconfig('gpload:input:error_table', unicode, None):
            self.error_table = True
            self.log(self.WARN,
                        "ERROR_TABLE is not supported. " +
                        "We will set LOG_ERRORS and REUSE_TABLES to True for compatibility.")

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


    def gpfdist_filenames(self, name, popenList):
        """
        Adds gpfdist -f filenames to popenList.
        Raises errors if YAML file option is invalid.

        @param name: input source name from YAML file.
        @param popenList: gpfdist options (updated)
        @return: list of files names
        """
        file = self.getconfig(name+':file',list)
        for i in file:
            if type(i)!= unicode and type(i) != str:
                self.control_file_error(name + ":file must be a YAML sequence of strings")
        popenList.append('-f')
        popenList.append('"'+' '.join(file)+'"')
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
        max_line_length = self.getconfig('gpload:input:max_line_length',int,None)
        if max_line_length is not None:
            popenList.append('-m')
            popenList.append(str(max_line_length))


    def gpfdist_transform(self, popenList):
        """
        Compute and return url fragment if transform option specified in YAML file.
        Checks for readable transform config file if transform_config option is specified.
        Adds gpfdist -c option to popenList if transform_config is specified.
        Validates that transform_config is present when transform option is specified.

        @param popenList: gpfdist options (updated)
        @returns: uri fragment for transform or "" if not appropriate.
        """
        transform = self.getconfig('gpload:input:transform', unicode, None)
        transform_config = self.getconfig('gpload:input:transform_config', unicode, None)
        if transform_config:
            try:
                f = open(transform_config,'r')
            except IOError,e:
                self.log(self.ERROR, "could not open transform_config file: %s" % e)
            f.close()
            popenList.append('-c')
            popenList.append(transform_config)
        else:
            if transform:
                self.control_file_error("transform_config is required when transform is specified")

        fragment = ""
        if transform is not None:
            fragment = "#transform=" + transform
        return fragment


    def gpfdist_ssl(self, popenList):
        """
        Adds gpfdist --ssl option to popenList when ssl option specified as true in YAML file.

        @param popenList: gpfdist options (updated)
        """
        ssl = self.getconfig('gpload:input:source:ssl',bool, False)
        certificates_path = self.getconfig('gpload:input:source:certificates_path', unicode, None)

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
        sourceIndex = 0
        availablePorts = set(xrange(1,65535))
        found_source = False

        self.getconfig('gpload:input', list)

        while 1:
            sourceIndex += 1
            name = 'gpload:input:source(%d)'%sourceIndex
            a = self.getconfig(name,None,None)
            if not a:
                break
            found_source = True
            local_hostname = self.getconfig(name+':local_hostname', list, False)

            # do default host, the current one
            if not local_hostname:
                # if fully_qualified_domain_name is defined and set to true we want to
                # resolve the fqdn rather than just grabbing the hostname.
                fqdn = self.getconfig('gpload:input:fully_qualified_domain_name', bool, False)
                if fqdn:
                    local_hostname = [socket.getfqdn()]
                else:
                    local_hostname = [socket.gethostname()]

            # build gpfdist parameters
            popenList = ['gpfdist']
            self.gpfdist_ssl(popenList)
            self.gpfdist_port_options(name, availablePorts, popenList)
            file = self.gpfdist_filenames(name, popenList)
            self.gpfdist_timeout_options(popenList)
            self.gpfdist_verbose_options(popenList)
            self.gpfdist_max_line_length(popenList)
            fragment = self.gpfdist_transform(popenList)

            try:
                self.log(self.LOG, 'trying to run %s' % ' '.join(popenList))
                cfds = True
                if platform.system() in ['Windows', 'Microsoft']: # not supported on win32
                    cfds = False
                    cmd = ' '.join(popenList)
                    needshell = False
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

            ssl = self.getconfig('gpload:input:source:ssl', bool, False)
            if ssl:
                protocol = 'gpfdists'
            else:
                protocol = 'gpfdist'

            for l in local_hostname:
                if type(l) != str and type(l) != unicode:
                    self.control_file_error(name + ":local_hostname must be a YAML sequence of strings")
                l = str(l)
                sep = ''
                if file[0] != '/':
                    sep = '/'
                # MPP-13617
                if ':' in l:
                    l = '[' + l + ']'
                self.locations.append('%s://%s:%d%s%s%s' % (protocol, l, port, sep, '%20'.join(file), fragment))
        if not found_source:
            self.control_file_error("configuration file must contain source definition")

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
        columns = self.getconfig('gpload:input:columns',list,None, returnOriginal=True)
        if columns != None:
            self.from_cols_from_user = True # user specified from columns
            self.from_columns = []
            for d in columns:
                if type(d)!=dict:
                    self.control_file_error("gpload:input:columns must be a sequence of YAML mappings")
                tempkey = d.keys()[0]
                value = d[tempkey]
                """ remove leading or trailing spaces """
                d = { tempkey.strip() : value }
                key = d.keys()[0]
                if d[key] == None:
                    self.log(self.DEBUG,
                             'getting source column data type from target')
                    for name, typ, mapto, hasseq in self.into_columns:
                        if sqlIdentifierCompare(name, key):
                            d[key] = typ
                            break

                # perform the same kind of magic type change that postgres does
                if d[key] == 'bigserial':
                    d[key] = 'bigint'
                elif d[key] == 'serial':
                    d[key] = 'int4'

                # Mark this column as having no mapping, which is important
                # for do_insert()
                self.from_columns.append([key.lower(),d[key].lower(),None, False])
        else:
            self.from_columns = self.into_columns
            self.from_cols_from_user = False

        # make sure that all columns have a type
        for name, typ, map, hasseq in self.from_columns:
            if typ == None:
                self.log(self.ERROR, 'column "%s" has no type ' % name +
                       'and does not appear in target table "%s"' % self.schemaTable)
        self.log(self.DEBUG, 'from columns are:')
        for c in self.from_columns:
            name = c[0]
            typ = c[1]
            self.log(self.DEBUG, '%s: %s'%(name,typ))



    def read_table_metadata(self):
        # KAS Note to self. If schema is specified, then probably should use PostgreSQL rules for defining it.

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
         a.attname as column_name,
         a.attnum as ordinal_position,
         format_type(a.atttypid, a.atttypmod) as data_type,
         c.relkind = 'r' AS is_updatable,
         a.atttypid in (23, 20) and a.atthasdef and
             (select position ( 'nextval(' in pg_catalog.pg_get_expr(adbin,adrelid) ) > 0 and
                          position ( '::regclass)' in pg_catalog.pg_get_expr(adbin,adrelid) ) > 0
              FROM pg_catalog.pg_attrdef d
              WHERE d.adrelid = a.attrelid AND d.adnum = a.attnum AND a.atthasdef) as has_sequence
          from pg_catalog.pg_class c join pg_catalog.pg_namespace nt on (c.relnamespace = nt.oid)
             join pg_attribute a on (a.attrelid = c.oid)
         where c.relname = '%s' and nt.nspname = '%s'
         and a.attnum > 0 and a.attisdropped = 'f'
         order by a.attnum """ % (quote_unident(self.table), quote_unident(self.schema))

        count = 0
        self.into_columns = []
        self.into_columns_dict = dict()
        resultList = self.db.query(queryString.encode('utf-8')).dictresult()
        while count < len(resultList):
            row = resultList[count]
            count += 1
            ct = unicode(row['data_type'])
            if ct == 'bigserial':
               ct = 'bigint'
            elif ct == 'serial':
               ct = 'int4'
            name = unicode(row['column_name'], 'utf-8')
            name = quote_ident(name)
            if unicode(row['has_sequence']) != unicode('f'):
                has_seq = True
            else:
                has_seq = False
            i = [name,ct,None, has_seq]
            self.into_columns.append(i)
            self.into_columns_dict[name] = i
            self.log(self.DEBUG, "found input column: " + str(i))
        if count == 0:
            # see if it's a permissions issue or it actually doesn't exist
            tableName = quote_unident(self.table)
            tableSchema = quote_unident(self.schema)
            sql = """select 1 from pg_class c, pg_namespace n
                        where c.relname = '%s' and
                        n.nspname = '%s' and
                        n.oid = c.relnamespace""" % (tableName, tableSchema)
            resultList = self.db.query(sql.encode('utf-8')).getresult()
            if len(resultList) > 0:
                self.log(self.ERROR, "permission denied for table %s.%s" % \
                            (tableSchema, tableName))
            else:
               self.log(self.ERROR, 'table %s.%s does not exist in database %s'% (tableSchema, tableName, self.options.d))

    def read_mapping(self):
        mapping = self.getconfig('gpload:output:mapping',dict,None, returnOriginal=True)

        if mapping:
            for key,value in mapping.iteritems():
                if type(key) != unicode or type(value) != unicode:
                    self.control_file_error("gpload:output:mapping must be a YAML type mapping from strings to strings")
                found = False
                for a in self.into_columns:
                    if sqlIdentifierCompare(a[0], key) == True:
                       a[2] = value
                       found = True
                       break
                if found == False:
                    self.log(self.ERROR,'%s in mapping is not in table %s'% \
                                    (key, self.schemaTable))
        else:
            # Now, map anything yet to be mapped to itself, picking up on those
            # columns which are not found in the table.
            for x in self.from_columns:
                # Check to see if it already has a mapping value
                i = filter(lambda a:a[2] == x[0], self.into_columns)
                if not i:
                    # Check to see if the target column names match the input column names.
                    for a in self.into_columns:
                        if sqlIdentifierCompare(a[0], x[0]) == True:
                           i = a
                           found = True
                           break
                    if i:
                        if i[2] == None: i[2] = i[0]
                    else:
                        self.log(self.ERROR, 'no mapping for input column ' +
                                 '"%s" to output table' % x[0])
        for name,typ,mapto,seq in self.into_columns:
            self.log(self.DEBUG,'%s: %s = %s'%(name,typ,mapto))

    # In order to find out whether we have an existing external table in the
    # catalog which could be reused for this operation we need to make sure
    # that it has the same column names and types, the same data format, and
    # location specification, and single row error handling specs.
    #
    # This function will return the SQL to run in order to find out whether
    # such a table exists.
    #
    def get_reuse_exttable_query(self, formatType, formatOpts, limitStr, from_cols, schemaName, log_errors):
        sqlFormat = """select attrelid::regclass
                 from (
                        select
                            attrelid,
                            row_number() over (partition by attrelid order by attnum) as attord,
                            attnum,
                            attname,
                            atttypid::regtype
                        from
                            pg_attribute
                            join
                            pg_class
                            on (pg_class.oid = attrelid)
                            %s
                        where
                            relstorage = 'x' and
                            relname like 'ext_gpload_reusable_%%' and
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

        if log_errors:
            sql += " WHERE pgext.fmterrtbl = pgext.reloid "
        else:
            sql += " WHERE pgext.fmterrtbl IS NULL "

        for i, l in enumerate(self.locations):
            sql += " and pgext.urilocation[%s] = %s\n" % (i + 1, quote(l))

        sql+= """and pgext.fmttype = %s
                 and pgext.writable = false
                 and pgext.fmtopts like %s """ % (quote(formatType[0]),quote("%" + quote_unident(formatOpts.rstrip()) +"%"))

        if limitStr:
            sql += "and pgext.rejectlimit = %s " % limitStr
        else:
            sql += "and pgext.rejectlimit IS NULL "

        sql+= "group by attrelid "

        sql+= """having
                    count(*) = %s and
                    bool_and(case """ % len(from_cols)

        for i, c in enumerate(from_cols):
            name = c[0]
            typ = c[1]
            sql+= "when attord = %s then atttypid = %s::regtype and attname = %s\n" % (i+1, quote(typ), quote(quote_unident(name)))

        sql+= """else true
                 end)
                 limit 1;"""

        self.log(self.DEBUG, "query used to identify reusable external relations: %s" % sql)
        return sql

    #
    # Create a string from the following conditions to reuse staging table:
    # 1. same target table
    # 2. same number of columns
    # 3. same names and types, in the same order
    # 4. same distribution key (according to columns' names and thier order)
    #
    def get_staging_conditions_string(self, target_table_name, staging_cols, distribution_cols):
			
        columns_num = len(staging_cols)

        staging_cols_str = '-'.join(map(lambda col:'%s-%s' % (quote(quote_unident(col[0])), quote(col[1])), staging_cols))

        distribution_cols_str = '-'.join([quote(quote_unident(col)) for col in distribution_cols])
		
        return '%s:%s:%s:%s' % (target_table_name, columns_num, staging_cols_str, distribution_cols_str)

		
    #
    # This function will return the SQL to run in order to find out whether
    # we have an existing staging table in the catalog which could be reused for this
    # operation, according to the mathod and the encoding conditions.
    #
    def get_reuse_staging_table_query(self, encoding_conditions):
		
        sql = """SELECT oid::regclass
                 FROM pg_class
                 WHERE relname = 'staging_gpload_reusable_%s';""" % (encoding_conditions)

        self.log(self.DEBUG, "query used to identify reusable temporary relations: %s" % sql)
        return sql

    #
    # get oid for table from pg_class, None if not exist
    #
    def get_table_oid(self, tableName):
        if tableName:
            sql = "select %s::regclass::oid" % quote(quote_unident(tableName))
            try:
                resultList = self.db.query(sql.encode('utf-8')).getresult()
                return resultList[0][0]
            except Exception, e:
                pass
        return None

    def get_ext_schematable(self, schemaName, tableName):
        if schemaName == None:
            return tableName
        else:
            schemaTable = "%s.%s" % (schemaName, tableName)
            return schemaTable

    def get_external_table_formatOpts(self, option, specify=''):

        formatType = self.getconfig('gpload:input:format', unicode, 'text').lower()
        if formatType == 'text':
            valid_token = ['delimiter','escape']
        elif formatType == 'csv':
            valid_token = ['delimiter', 'quote', 'escape']
        else:
            valid_token = []

        if not option in valid_token:
            self.control_file_error("The option you specified doesn't support now")
            return

        if option == 'delimiter':
            defval = ',' if formatType == 'csv' else '\t'
            val = self.getconfig('gpload:input:delimiter', unicode, defval)
        elif option == 'escape':
            defval = self.getconfig('gpload:input:quote', unicode, '"')
            val = self.getconfig('gpload:input:escape', unicode, defval)
        elif option == 'quote':
            val = self.getconfig('gpload:input:quote', unicode, '"')
        else:
            self.control_file_error("unexpected error -- backtrace " +
                             "written to log file")
            sys.exit(2)

        specify_str = str(specify) if specify else option
        if len(val) != 1:
            if val.startswith("E'") and val.endswith("'") and len(val[2:-1].decode('unicode-escape')) == 1:
                subval = val[2:-1]
                if subval == "\\'":
                    val = val
                    self.formatOpts += "%s %s " % (specify_str, val)
                else:
                    val = subval.decode('unicode-escape')
                    self.formatOpts += "%s '%s' " % (specify_str, val)
            elif len(val.decode('unicode-escape')) == 1:
                val = val.decode('unicode-escape')
                self.formatOpts += "%s '%s' " % (specify_str, val)

            else:
                self.control_file_warning(option +''' must be single ASCII charactor, you can also use unprintable characters(for example: '\\x1c' / E'\\x1c' or '\\u001c' / E'\\u001c' ''')
                self.control_file_error("Invalid option, gpload quit immediately")
                sys.exit(2);
        else:
            self.formatOpts += "%s '%s' " % (specify_str, val)


    #
    # Create a new external table or find a reusable external table to use for this operation
    #
    def create_external_table(self):

        # extract all control file information and transform it accordingly
        # in order to construct a CREATE EXTERNAL TABLE statement if will be
        # needed later on

        formatType = self.getconfig('gpload:input:format', unicode, 'text').lower()
        locationStr = ','.join(map(quote,self.locations))

        self.get_external_table_formatOpts('delimiter')

        nullas = self.getconfig('gpload:input:null_as', unicode, False)
        self.log(self.DEBUG, "null " + unicode(nullas))
        if nullas != False: # could be empty string
            self.formatOpts += "null %s " % quote(nullas)
        elif formatType=='csv':
            self.formatOpts += "null '' "
        else:
            self.formatOpts += "null %s " % quote("\N")


        esc = self.getconfig('gpload:input:escape', None, None)
        if esc:
            if type(esc) != unicode and type(esc) != str:
                self.control_file_error("gpload:input:escape must be a string")
            if esc.lower() == 'off':
                if formatType == 'csv':
                    self.control_file_error("ESCAPE cannot be set to OFF in CSV mode")
                self.formatOpts += "escape 'off' "
            else:
                self.get_external_table_formatOpts('escape')
        else:
            if formatType=='csv':
                self.get_external_table_formatOpts('quote','escape')
            else:
                self.formatOpts += "escape '\\'"

        if formatType=='csv':
            self.get_external_table_formatOpts('quote')

        if self.getconfig('gpload:input:header',bool,False):
            self.formatOpts += "header "

        force_not_null_columns = self.getconfig('gpload:input:force_not_null',list,[])
        if force_not_null_columns:
            for i in force_not_null_columns:
                if type(i) != unicode and type(i) != str:
                    self.control_file_error("gpload:input:force_not_null must be a YAML sequence of strings")
            self.formatOpts += "force not null %s " % ','.join(force_not_null_columns)

        encodingStr = self.getconfig('gpload:input:encoding', unicode, None)

        limitStr = self.getconfig('gpload:input:error_limit',int, None)
        if self.log_errors and not limitStr:
            self.control_file_error("gpload:input:log_errors requires " +
                    "gpload:input:error_limit to be specified")

        self.extSchemaName = self.getconfig('gpload:external:schema', unicode, None)
        if self.extSchemaName == '%':
            self.extSchemaName = self.schema

        # get the list of columns to use in the extnernal table
        if not self.from_cols_from_user:
            # don't put values serial columns
            from_cols = filter(lambda a: a[3] != True,
                               self.from_columns)
        else:
            from_cols = self.from_columns

        # If the 'reuse tables' option was specified we now try to find an
        # already existing external table in the catalog which will match
        # the one that we need to use. It must have identical attributes,
        # external location, format, and encoding specifications.
        if self.reuse_tables == True:
            # process the single quotes in order to successfully find an existing external table to reuse.
            self.formatOpts = self.formatOpts.replace("E'\\''","'\''")
            sql = self.get_reuse_exttable_query(formatType, self.formatOpts,
                    limitStr, from_cols, self.extSchemaName, self.log_errors)
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

            self.extTableName = "ext_gpload_reusable_%s" % self.unique_suffix
            self.log(self.INFO, "did not find an external table to reuse. creating %s" % self.get_ext_schematable(self.extSchemaName, self.extTableName))

        # process the single quotes in order to successfully create an external table.
        self.formatOpts = self.formatOpts.replace("'\''","E'\\''")

        # construct a CREATE EXTERNAL TABLE statement and execute it
        self.extSchemaTable = self.get_ext_schematable(self.extSchemaName, self.extTableName)
        sql = "create external table %s" % self.extSchemaTable
        sql += "(%s)" % ','.join(map(lambda a:'%s %s' % (a[0], a[1]), from_cols))

        sql += "location(%s) "%locationStr
        sql += "format%s "% quote(formatType)
        if len(self.formatOpts) > 0:
            sql += "(%s) "% self.formatOpts
        if encodingStr:
            sql += "encoding%s "%quote(encodingStr)
        if self.log_errors:
            sql += "log errors "

        if limitStr:
            if limitStr < 2:
                self.control_file_error("error_limit must be 2 or higher")
            sql += "segment reject limit %s "%limitStr

        try:
            self.db.query(sql.encode('utf-8'))
        except Exception, e:
            self.log(self.ERROR, 'could not run SQL "%s": %s' % (sql, unicode(e)))

        # set up to drop the external table at the end of operation, unless user
        # specified the 'reuse_tables' option, in which case we don't drop
        if self.reuse_tables == False:
            self.cleanupSql.append('drop external table if exists %s'%self.extSchemaTable)

		
    #
    # Create a new staging table or find a reusable staging table to use for this operation
    # (only valid for update/merge operations).
    #
    def create_staging_table(self):

        # Do some initial work to extract the update_columns and metadata
        # that may be needed in order to create or reuse a temp table
        if not self.from_cols_from_user:
            # don't put values serial columns
            from_cols = filter(lambda a: a[3] != True, self.from_columns)
        else:
            from_cols = self.from_columns

        # make sure we set the correct distribution policy
        distcols = self.getconfig('gpload:output:match_columns', list)

        # MPP-13399, CR-2227
        including_defaults = ""
        if self.getconfig('gpload:output:including_defaults',bool,True):
            including_defaults = " including defaults"

        sql = "SELECT * FROM pg_class WHERE relname LIKE 'temp_gpload_reusable_%%';"
        resultList = self.db.query(sql.encode('utf-8')).getresult()
        if len(resultList) > 0:
            self.log(self.WARN, """Old style, reusable tables named "temp_gpload_reusable_*" from a previous versions were found.
                         Greenplum recommends running "DROP TABLE temp_gpload_reusable_..." on each table. This only needs to be done once.""")
		
        # If the 'reuse tables' option was specified we now try to find an
        # already existing staging table in the catalog which will match
        # the one that we need to use. It must meet the reuse conditions
        is_temp_table = 'TEMP '
        target_columns = []
        for column in self.into_columns:
            if column[2]:
                target_columns.append([quote_unident(column[0]), column[1]])

        if self.reuse_tables == True:
            is_temp_table = ''
            target_table_name = quote_unident(self.table)

            # create a string from all reuse conditions for staging tables and ancode it
            conditions_str = self.get_staging_conditions_string(target_table_name, target_columns, distcols)
            encoding_conditions = hashlib.md5(conditions_str).hexdigest()
					
            sql = self.get_reuse_staging_table_query(encoding_conditions)
            resultList = self.db.query(sql.encode('utf-8')).getresult()

            if len(resultList) > 0:

                # found a temp table to reuse. no need to create one. we're done here.
                self.staging_table_name = (resultList[0])[0]
                self.log(self.INFO, "reusing staging table %s" % self.staging_table_name)

                # truncate it so we don't use old data
                self.do_truncate(self.staging_table_name)

                return

            # didn't find an existing staging table suitable for reuse. Format a reusable
            # name and issue a CREATE TABLE on it (without TEMP!). Hopefully we can use it
            # next time around
            # we no longer need the timestamp, since we will never want to create few
            # tables with same encoding_conditions
            self.staging_table_name = "staging_gpload_reusable_%s" % (encoding_conditions)
            self.log(self.INFO, "did not find a staging table to reuse. creating %s" % self.staging_table_name)
		
        # MPP-14667 - self.reuse_tables should change one, and only one, aspect of how we build the following table,
        # and that is, whether it's a temp table or not. In other words, is_temp_table = '' iff self.reuse_tables == True.
        sql = 'CREATE %sTABLE %s ' % (is_temp_table, self.staging_table_name)
        cols = map(lambda a:'%s %s' % (a[0], a[1]), target_columns)
        sql += "(%s)" % ','.join(cols)
        sql += " DISTRIBUTED BY (%s)" % ', '.join(distcols)
        self.log(self.LOG, sql)

        if not self.options.D:
            self.db.query(sql.encode('utf-8'))
            if not self.reuse_tables:
                self.cleanupSql.append('DROP TABLE IF EXISTS %s' % self.staging_table_name)


    def count_errors(self):
        notice_processor(self)
        if self.log_errors and not self.options.D:
            # make sure we only get errors for our own instance
            if not self.reuse_tables:
                queryStr = "select count(*) from gp_read_error_log('%s')" % pg.escape_string(self.extSchemaTable)
                results = self.db.query(queryStr.encode('utf-8')).getresult()
                return (results[0])[0]
            else: # reuse_tables
                queryStr = "select count(*) from gp_read_error_log('%s') where cmdtime > to_timestamp(%s)" % (pg.escape_string(self.extSchemaTable), self.startTimestamp)
                results = self.db.query(queryStr.encode('utf-8')).getresult()
                global NUM_WARN_ROWS
                NUM_WARN_ROWS = (results[0])[0]
                return (results[0])[0];
        return 0

    def report_errors(self):
        errors = self.count_errors()
        if errors==1:
            self.log(self.WARN, '1 bad row')
        elif errors:
            self.log(self.WARN, '%d bad rows'%errors)

        # error message is also deleted if external table is dropped.
        # if reuse_table is set, error message is not deleted.
        if errors and self.log_errors and self.reuse_tables:
            self.log(self.WARN, "Please use following query to access the detailed error")
            self.log(self.WARN, "select * from gp_read_error_log('{0}') where cmdtime > to_timestamp('{1}')".format(pg.escape_string(self.extSchemaTable), self.startTimestamp))
        self.exitValue = 1 if errors else 0


    def do_insert(self, dest):
        """
        Handle the INSERT case
        """
        self.log(self.DEBUG, "into columns " + str(self.into_columns))
        cols = filter(lambda a:a[2]!=None, self.into_columns)

        # only insert non-serial columns, unless the user told us to
        # insert the serials explicitly
        if not self.from_cols_from_user:
            cols = filter(lambda a:a[3] == False, cols)

        sql = 'INSERT INTO %s' % dest
        sql += ' (%s)' % ','.join(map(lambda a:a[0], cols))
        sql += ' SELECT %s' % ','.join(map(lambda a:a[2], cols))
        sql += ' FROM %s' % self.extSchemaTable

        # cktan: progress thread is not reliable. revisit later.
        #progress = Progress(self,self.ports)
        #progress.start()
        #self.threads.append(progress)
        self.log(self.LOG, sql)
        if not self.options.D:
            try:
                self.rowsInserted = self.db.query(sql.encode('utf-8'))
            except Exception, e:
                # We need to be a bit careful about the error since it may contain non-unicode characters
                strE = unicode(str(e), errors = 'ignore')
                strF = unicode(str(sql), errors = 'ignore')
                self.log(self.ERROR, strE + ' encountered while running ' + strF)

        #progress.condition.acquire()
        #progress.number = 1
        #progress.condition.wait()
        #progress.condition.release()
        self.report_errors()

    def do_method_insert(self):
        self.create_external_table()
        self.do_insert(self.get_qualified_tablename())

    def map_stuff(self,config,format,index):
        lis = []
        theList = self.getconfig(config,list)
        theList = convertListToDelimited(theList)
        for i in theList:
            if type(i) != unicode and type(i) != str:
                self.control_file_error("%s must be a YAML sequence of strings"%config)
            j = self.into_columns_dict.get(i)
            if not j:
                self.log(self.ERROR,'column %s in %s does not exist'%(i,config))
            if not j[index]:
                self.log(self.ERROR,'there is no mapping from the column %s in %s'%(i,config))
            lis.append(format(j[0],j[index]))
        return lis

    def fix_update_cond(self, match):
        self.log(self.DEBUG, match.group(0))
        return 'into_table.' + match.group(0)

    def do_update(self,fromname,index):
        """
        UPDATE case
        """
        sql = 'update %s into_table ' % self.get_qualified_tablename()
        sql += 'set %s '%','.join(self.map_stuff('gpload:output:update_columns',(lambda x,y:'%s=from_table.%s' % (x, y)),index))
        sql += 'from %s from_table' % fromname

        match = self.map_stuff('gpload:output:match_columns'
                              , lambda x,y:'into_table.%s=from_table.%s' % (x, y)
                              , index)

        update_condition = self.getconfig('gpload:output:update_condition',
                            unicode, None)
        if update_condition:
            #
            # Place the table alias infront of column references.
            #
            # The following logic is not bullet proof. It may not work
            # correctly if the user uses an identifier in both its
            # delimited and un-delimited format (e.g. where c1 < 7 and "c1" > 2)
            # Better lexing and parsing needs to be done here to fix all cases.
            #
            update_condition = ' ' + update_condition + ' '
            for name, type, mapto, seq in self.into_columns:
                regexp = '(?<=[^\w])%s(?=[^\w])' % name
                self.log(self.DEBUG, 'update_condition re: ' + regexp)
                temp_update_condition = update_condition
                updateConditionList = splitIntoLiteralsAndNonLiterals(update_condition)
                skip = False
                newUpdateConditionList = []
                update_condition = ''
                for uc in updateConditionList:
                    if skip == False:
                       uc = re.sub(regexp, self.fix_update_cond, uc)
                       skip = True
                    update_condition = update_condition + uc
                if update_condition == temp_update_condition:
                   # see if column can be undelimited, and try again.
                   if len(name) > 2 and name[1:-1] == name[1:-1].lower():
                      regexp = '(?<=[^\w])%s(?=[^\w])' % name[1:-1]
                      self.log(self.DEBUG, 'update_condition undelimited re: ' + regexp)
                      update_condition = re.sub( regexp
                                               , self.fix_update_cond
                                               , update_condition
                                               )
            self.log(self.DEBUG, "updated update_condition to %s" %
                         update_condition)
            match.append(update_condition)
        sql += ' where %s' % ' and '.join(match)
        self.log(self.LOG, sql)
        if not self.options.D:
            try:
                self.rowsUpdated = self.db.query(sql.encode('utf-8'))
            except Exception, e:
                # We need to be a bit careful about the error since it may contain non-unicode characters
                strE = unicode(str(e), errors = 'ignore')
                strF = unicode(str(sql), errors = 'ignore')
                self.log(self.ERROR, strE + ' encountered while running ' + strF)
				
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

    def table_supports_update(self):
        """Columns being updated cannot appear in the distribution key."""
        distKeyList = self.get_table_dist_key()
        distkey = set()
        for dk in distKeyList:
            distkey.add(quote_ident(dk))

        self.distkey = distkey
        if len(distkey) != 0:
            # not randomly distributed - check that UPDATE_COLUMNS isn't part of the distribution key
            updateColumnList = self.getconfig('gpload:output:update_columns',
                                              list,
                                              returnOriginal=True)
            update_columns = convertListToDelimited(updateColumnList)
            update_columns = set(update_columns)
            a = distkey.intersection(update_columns)
            if len(a):
                self.control_file_error('update_columns cannot reference column(s) in distribution key (%s)' % ', '.join(list(distkey)))

    def do_method_update(self):
        """Load the data in and update an existing table based upon it"""

        self.table_supports_update()
        self.create_staging_table()

        self.create_external_table()
        self.do_insert(self.staging_table_name)
        # These rows are inserted temporarily for processing, so set inserted rows back to zero.
        self.rowsInserted = 0
        self.do_update(self.staging_table_name, 0)

    def do_method_merge(self):
        """insert data not already in the table, update remaining items"""

        self.table_supports_update()
        self.create_staging_table()
        self.create_external_table()
        self.do_insert(self.staging_table_name)
        self.rowsInserted = 0 # MPP-13024. No rows inserted yet (only to temp table).
        self.do_update(self.staging_table_name, 0)
		
        # insert new rows to the target table
        match = self.map_stuff('gpload:output:match_columns',lambda x,y:'into_table.%s=from_table.%s'%(x,y),0)
        matchColumns = self.getconfig('gpload:output:match_columns',list)
		
        cols = filter(lambda a:a[2] != None, self.into_columns)				
        sql = 'INSERT INTO %s ' % self.get_qualified_tablename()
        sql += '(%s) ' % ','.join(map(lambda a:a[0], cols))
        sql += '(SELECT %s ' % ','.join(map(lambda a:'from_table.%s' % a[0], cols))
        sql += 'FROM (SELECT *, row_number() OVER (PARTITION BY %s) AS gpload_row_number ' % ','.join(matchColumns)
        sql += 'FROM %s) AS from_table ' % self.staging_table_name
        sql += 'LEFT OUTER JOIN %s into_table ' % self.get_qualified_tablename()
        sql += 'ON %s '%' AND '.join(match)
        where = self.map_stuff('gpload:output:match_columns',lambda x,y:'into_table.%s IS NULL'%x,0)
        sql += 'WHERE %s ' % ' AND '.join(where)
        sql += 'AND gpload_row_number=1)'

        self.log(self.LOG, sql)
        if not self.options.D:
            try:
                self.rowsInserted = self.db.query(sql.encode('utf-8'))
            except Exception, e:
                # We need to be a bit careful about the error since it may contain non-unicode characters
                strE = unicode(str(e), errors = 'ignore')
                strF = unicode(str(sql), errors = 'ignore')
                self.log(self.ERROR, strE + ' encountered while running ' + strF)
				

    def do_truncate(self, tblname):
        self.log(self.LOG, "Truncate table %s" %(tblname))
        if not self.options.D:
            try:
                truncateSQLtext = "truncate %s" % tblname
                self.db.query(truncateSQLtext.encode('utf-8'))
            except Exception, e:
                self.log(self.ERROR, 'could not execute truncate target %s: %s' % (tblname, str(e)))

    def do_method(self):
        # Is the table to be truncated before the load?
        preload = self.getconfig('gpload:preload', list, default=None)
        method = self.getconfig('gpload:output:mode', unicode, 'insert').lower()
        self.log_errors = self.getconfig('gpload:input:log_errors', bool, False)
        truncate = False
        self.reuse_tables = False

        if not self.options.no_auto_trans and not method=='insert':
            self.db.query("BEGIN")

        if preload:
            truncate = self.getconfig('gpload:preload:truncate',bool,False)
            self.reuse_tables = self.getconfig('gpload:preload:reuse_tables',bool,False)
        if self.error_table:
            self.log_errors = True
            self.reuse_tables = True
        if truncate == True:
            if method=='insert':
                self.do_truncate(self.schemaTable)
            else:
                self.log(self.ERROR, 'preload truncate operation should be used with insert ' +
                                     'operation only. used with %s' % method)

        # sql pre or post processing?
        sql = self.getconfig('gpload:sql', list, default=None)
        before   = None
        after    = None
        if sql:
            before   = self.getconfig('gpload:sql:before', unicode, default=None)
            after    = self.getconfig('gpload:sql:after', unicode, default=None)
        if before:
            self.log(self.LOG, "Pre-SQL from user: %s" % before)
            if not self.options.D:
                try:
                    self.db.query(before.encode('utf-8'))
                except Exception, e:
                    self.log(self.ERROR, 'could not execute SQL in sql:before "%s": %s' %
                             (before, str(e)))


        if method=='insert':
            self.do_method_insert()
        elif method=='update':
            self.do_method_update()
        elif method=='merge':
            self.do_method_merge()
        else:
            self.control_file_error('unsupported method %s' % method)

        # truncate the staging table to avoid dumping it's content - see MPP-15474
        if method=='merge' or method=='update':
            self.do_truncate(self.staging_table_name)

        if after:
            self.log(self.LOG, "Post-SQL from user: %s" % after)
            if not self.options.D:
                try:
                    self.db.query(after.encode('utf-8'))
                except Exception, e:
                    self.log(self.ERROR, 'could not execute SQL in sql:after "%s": %s' %
                             (after, str(e)))

        if not self.options.no_auto_trans and not method=='insert':
            self.db.query("COMMIT")


    def stop_gpfdists(self):
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
        self.log(self.LOG, 'terminating all threads')
        for t in self.threads:
            t.join()
        self.log(self.LOG, 'all threads are terminated')


    def run2(self):
        self.log(self.DEBUG, 'config ' + str(self.config))
        start = time.time()
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
        self.rowsInserted = 0
        self.rowsUpdated  = 0
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
            self.stop_gpfdists()

            if self.cleanupSql:
                self.log(self.LOG, 'removing temporary data')
                self.setup_connection()
                for a in self.cleanupSql:
                    try:
                        self.log(self.DEBUG, a)
                        self.db.query(a.encode('utf-8'))
                    except (Exception, SystemExit):
                        traceback.print_exc(file=self.logfile)
                        self.logfile.flush()
                        traceback.print_exc()

            if self.db != None:
                self.db.close()

            self.log(self.INFO, 'rows Inserted          = ' + str(self.rowsInserted))
            self.log(self.INFO, 'rows Updated           = ' + str(self.rowsUpdated))
            self.log(self.INFO, 'data formatting errors = ' + str(NUM_WARN_ROWS))
            if self.exitValue==0:
                self.log(self.INFO, 'gpload succeeded')
            elif self.exitValue==1:
                self.log(self.INFO, 'gpload succeeded with warnings')
            else:
                self.log(self.INFO, 'gpload failed')

            ## MPP-19015 - Extra python thread shutdown time is needed on HP-UX
            if platform.uname()[0] == 'HP-UX':
                time.sleep(1)


if __name__ == '__main__':
    g = gpload(sys.argv[1:])
    g.run()
    sys.exit(g.exitValue)
