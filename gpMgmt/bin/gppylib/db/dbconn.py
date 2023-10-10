#!/usr/bin/env python3
#
# Copyright (c) Greenplum Inc 2008. All Rights Reserved.
#
"""
TODO: module docs
"""
import collections
import sys
import os
import stat
import psycopg2
try:
    from gppylib.commands.unix import UserId

except ImportError as e:
    sys.exit('Error: unable to import module: ' + str(e))

from gppylib import gplog

logger = gplog.get_default_logger()


class ConnectionError(Exception): pass

class Pgpass():
    """ Class for handling .pgpass file.
    """
    entries = []
    valid_pgpass = True

    def __init__(self):
        HOME = os.getenv('HOME')
        PGPASSFILE = os.getenv('PGPASSFILE', '%s/.pgpass' % HOME)

        if not os.path.exists(PGPASSFILE):
            return

        st_info = os.stat(PGPASSFILE)
        mode = st_info[stat.ST_MODE] & 0o777

        if mode != 0o600:
            print('WARNING: password file "%s" has group or world access; permissions should be u=rw (0600) or less' % PGPASSFILE)
            self.valid_pgpass = False
            return

        try:
            fp = open(PGPASSFILE, 'r')
            try:
                lineno = 1
                for line in fp:
                    line = line.strip()
                    if line.startswith('#'):
                        continue
                    try:
                        (hostname, port, database, username, password) = line.strip().split(':')
                        entry = {'hostname': hostname,
                                 'port': port,
                                 'database': database,
                                 'username': username,
                                 'password': password }
                        self.entries.append(entry)
                    except:
                        print('Invalid line in .pgpass file.  Line number %d' % lineno)
                    lineno += 1
            except IOError:
                pass
            finally:
                if fp: fp.close()
        except OSError:
            pass


    def get_password(self, username, hostname, port, database):
        for entry in self.entries:
            if ((entry['hostname'] == hostname or entry['hostname'] == '*') and
               (entry['port'] == str(port) or entry['port'] == '*') and
               (entry['database'] == database or entry['database'] == '*') and
               (entry['username'] == username or entry['username'] == '*')):
                return entry['password']
        return None

    def pgpass_valid(self):
        return self.valid_pgpass

class DbURL:
    """ DbURL is used to store all of the data required to get at a PG
        or GP database.

    """
    pghost='foo'
    pgport=5432
    pgdb='template1'
    pguser='username'
    pgpass='pass'
    timeout=None
    retries=None

    def __init__(self,hostname=None,port=0,dbname=None,username=None,password=None,timeout=None,retries=None):

        if hostname is None:
            self.pghost = os.environ.get('PGHOST', 'localhost')
        else:
            self.pghost = hostname

        if port == 0:
            self.pgport = int(os.environ.get('PGPORT', '5432'))
        else:
            self.pgport = int(port)

        if dbname is None:
            self.pgdb = os.environ.get('PGDATABASE', 'template1')
        else:
            self.pgdb = dbname

        if username is None:
            self.pguser = os.environ.get('PGUSER', os.environ.get('USER', None))
            if self.pguser is None:
                # fall back to /usr/bin/id
                self.pguser = UserId.local('Get uid')
            if self.pguser is None or self.pguser == '':
                raise Exception('Both $PGUSER and $USER env variables are not set!')
        else:
            self.pguser = username

        if password is None:
            pgpass = Pgpass()
            if pgpass.pgpass_valid():
                password = pgpass.get_password(self.pguser, self.pghost, self.pgport, self.pgdb)
                if password:
                    self.pgpass = password
                else:
                    self.pgpass = os.environ.get('PGPASSWORD', None)
        else:
            self.pgpass = password

        if timeout is not None:
            self.timeout = int(timeout)

        if retries is None:
            self.retries = 1
        else:
            self.retries = int(retries)


    def __str__(self):

        # MPP-13617
        def canonicalize(s):
            if ':' not in s: return s
            return '[' + s + ']'

        return "%s:%d:%s:%s:%s" % \
            (canonicalize(self.pghost),self.pgport,self.pgdb,self.pguser,self.pgpass)


# This wrapper of pgdb provides two useful additions:
# 1. pg notice is accessible to a user of connection returned by dbconn.connect(),
# lifted from the underlying _pg connection
# 2. multiple calls to dbconn.close() should not return an error
class Connection:
    def __init__(self, connection):
        self._conn = connection
        self._conn.notices = collections.deque(maxlen=100)

    def __enter__(self):
        return self._conn.__enter__()

    # __exit__() does not close the connection. This is in line with the
    # python DB API v2 specification (pep-0249), where close() is done on
    # __del__(), not __exit__().
    def __exit__(self, *args):
        return self._conn.__exit__(*args)

    def __getattr__(self, name):
        return getattr(self._conn, name)

    def notices(self):
        notice_list = list(self._conn.notices)
        self._conn.notices.clear()
        return notice_list

    # don't return operational error if connection is already closed
    def close(self):
        if not self._conn.closed:
            self._conn.close()


def connect(dburl, utility=False, verbose=False,
            encoding=None, allowSystemTableMods=False, logConn=True, unsetSearchPath=True, cursorFactory=None):

    conninfo = {
        'user': dburl.pguser,
        'password': dburl.pgpass,
        'host': dburl.pghost,
        'port': dburl.pgport,
        'database': dburl.pgdb,
        'cursor_factory': cursorFactory
    }

    # building options
    options = []
    if utility:
        options.append("-c gp_role=utility")

    # unset search path due to CVE-2018-1058
    if unsetSearchPath:
        options.append("-c search_path=")

    if allowSystemTableMods:
        options.append("-c allow_system_table_mods=true")

    # set client encoding if needed
    if encoding:
        options.append("-c CLIENT_ENCODING=%s" % encoding)

    #by default, libpq will print WARNINGS to stdout
    if not verbose:
        options.append("-c CLIENT_MIN_MESSAGES=ERROR")

    if options:
        conninfo['options'] = " ".join(options)

    # use specified connection timeout
    retries = 1
    if dburl.timeout is not None:
        conninfo['connect_timeout'] = dburl.timeout
        retries = dburl.retries

    # This flag helps to avoid logging the connection string in some special
    # situations as requested
    if logConn:
        logFunc = logger.info if dburl.timeout is not None else logger.debug
        logFunc("Connecting to db {} on host {}".format(dburl.pgdb, dburl.pghost))

    conn = None
    for i in range(retries):
        try:
            conn = psycopg2.connect(**conninfo)
            conn.set_session(autocommit=True)
            break

        except psycopg2.OperationalError as e:
            if 'timeout expired' in str(e):
                logger.warning('Timeout expired connecting to %s, attempt %d/%d' % (dburl.pgdb, i+1, retries))
                continue
            raise

    if conn is None:
        raise ConnectionError('Failed to connect to %s' % dburl.pgdb)

    return Connection(conn)

def execSQL(conn, sql, autocommit=True):
    """
    Execute a sql command that is NOT expected to return any rows and expects to commit
    immediately.
    This function does not return a cursor object, and sets connection.autocommit = autocommit,
    which is necessary for queries like "create tablespace" that cannot be run inside a
    transaction.
    For SQL that captures some expected output, use "query()"

    Using `with dbconn.connect() as conn` syntax will override autocommit and complete
    queries in a transaction followed by a commit on context close
    """
    with conn.cursor() as cursor:
        cursor.execute(sql)

def query(conn, sql):
    """
    Run SQL that is expected to return some rows of output

    returns a cursor, which can then be used to iterate through all rows
    or return them in an array.
    """
    cursor=conn.cursor()
    cursor.execute(sql)
    return cursor

def queryRow(conn, sql):
    """
    Run SQL that returns exactly one row, and return that one row

    TODO: Handle like gppylib.system.comfigurationImplGpdb.fetchSingleOutputRow().
    In the event of the wrong number of rows/columns, some logging would be helpful...
    """
    with conn.cursor() as cursor:
        cursor.execute(sql)

        if cursor.rowcount != 1 :
            raise UnexpectedRowsError(1, cursor.rowcount, sql)
        res = cursor.fetchone()

    return res

class UnexpectedRowsError(Exception):
    def __init__(self, expected, actual, sql):
        self.expected, self.actual, self.sql = expected, actual, sql
        Exception.__init__(self, "SQL retrieved %d rows but %d was expected:\n%s" % \
                                 (self.actual, self.expected, self.sql))

def querySingleton(conn, sql):
    """
    Run SQL that returns exactly one row and one column, and return that cell

    TODO: Handle like gppylib.system.comfigurationImplGpdb.fetchSingleOutputRow().
    In the event of the wrong number of rows/columns, some logging would be helpful...
    """
    row = queryRow(conn, sql)
    if len(row) > 1:
        raise Exception("SQL retrieved %d columns but 1 was expected:\n%s" % \
                         (len(row), sql))
    return row[0]


def executeUpdateOrInsert(conn, sql, expectedRowUpdatesOrInserts):
    cursor=conn.cursor()
    cursor.execute(sql)

    if cursor.rowcount != expectedRowUpdatesOrInserts :
        raise Exception("SQL affected %s rows but %s were expected:\n%s" % \
                        (cursor.rowcount, expectedRowUpdatesOrInserts, sql))
    return cursor
