#!/usr/bin/env python
#
# Copyright (c) Greenplum Inc 2008. All Rights Reserved.
#
"""
TODO: module docs
"""
import sys
import os
import stat

try:
    from pygresql import pgdb
    from gppylib.commands.unix import UserId

except ImportError, e:
    sys.exit('Error: unable to import module: ' + str(e))

from gppylib import gplog

logger = gplog.get_default_logger()


class ConnectionError(StandardError): pass

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
        mode = str(oct(st_info[stat.ST_MODE] & 0777))

        if mode != "0600":
            print 'WARNING: password file "%s" has group or world access; permissions should be u=rw (0600) or less' % PGPASSFILE
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
                        print 'Invalid line in .pgpass file.  Line number %d' % lineno
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

        if port is 0:
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


def connect(dburl, utility=False, verbose=False,
            encoding=None, allowSystemTableMods=False, logConn=True, unsetSearchPath=True):

    if utility:
        options = '-c gp_session_role=utility'
    else:
        options = ''

    # MPP-13779, et al
    if allowSystemTableMods:
        options += ' -c allow_system_table_mods=true'

    # bypass pgdb.connect() and instead call pgdb._connect_
    # to avoid silly issues with : in ipv6 address names and the url string
    #
    dbbase   = dburl.pgdb
    dbhost   = dburl.pghost
    dbport   = int(dburl.pgport)
    dbopt    = options
    dbtty    = "1"
    dbuser   = dburl.pguser
    dbpasswd = dburl.pgpass
    timeout  = dburl.timeout
    cnx      = None

    # All quotation and escaping here are to handle database name containing
    # special characters like ' and \ and white spaces.

    # Need to escape backslashes and single quote in db name
    # Also single quoted the connection string for dbname
    dbbase = dbbase.replace('\\', '\\\\')
    dbbase = dbbase.replace('\'', '\\\'')

    # MPP-14121, use specified connection timeout
    # Single quote the connection string for dbbase name
    if timeout is not None:
        cstr    = "dbname='%s' connect_timeout=%s" % (dbbase, timeout)
        retries = dburl.retries
    else:
        cstr    = "dbname='%s'" % dbbase
        retries = 1

    # This flag helps to avoid logging the connection string in some special
    # situations as requested
    if (logConn == True):
        (logger.info if timeout is not None else logger.debug)("Connecting to %s" % cstr)

    for i in range(retries):
        try:
            cnx  = pgdb._connect_(cstr, dbhost, dbport, dbopt, dbtty, dbuser, dbpasswd)
            break

        except pgdb.InternalError, e:
            if 'timeout expired' in str(e):
                logger.warning('Timeout expired connecting to %s, attempt %d/%d' % (dbbase, i+1, retries))
                continue
            raise

    if cnx is None:
        raise ConnectionError('Failed to connect to %s' % dbbase)

    # NOTE: the code to set ALWAYS_SECURE_SEARCH_PATH_SQL below assumes it is not part of an existing transaction
    conn = pgdb.pgdbCnx(cnx)

    #by default, libpq will print WARNINGS to stdout
    if not verbose:
        cursor=conn.cursor()
        cursor.execute("SET CLIENT_MIN_MESSAGES='ERROR'")
        conn.commit()
        cursor.close()

    # set client encoding if needed
    if encoding:
        cursor=conn.cursor()
        cursor.execute("SET CLIENT_ENCODING='%s'" % encoding)
        conn.commit()
        cursor.close()

    # unset search path due to CVE-2018-1058
    if unsetSearchPath:
        ALWAYS_SECURE_SEARCH_PATH_SQL = "SELECT pg_catalog.set_config('search_path', '', false)"
        execSQL(conn, ALWAYS_SECURE_SEARCH_PATH_SQL).close()

    def __enter__(self):
        return self
    def __exit__(self, type, value, traceback):
        self.close()
    conn.__class__.__enter__, conn.__class__.__exit__ = __enter__, __exit__
    return conn


def execSQL(conn,sql):
    """
    If necessary, user must invoke conn.commit().
    Do *NOT* violate that API here without considering
    the existing callers of this function.
    """
    cursor=conn.cursor()
    cursor.execute(sql)
    return cursor

def execSQLForSingletonRow(conn, sql):
    """
    Run SQL that returns exactly one row, and return that one row

    TODO: Handle like gppylib.system.comfigurationImplGpdb.fetchSingleOutputRow().
    In the event of the wrong number of rows/columns, some logging would be helpful...
    """
    cursor=conn.cursor()
    cursor.execute(sql)

    if cursor.rowcount != 1 :
        raise UnexpectedRowsError(1, cursor.rowcount, sql)

    res = cursor.fetchall()[0]
    cursor.close()
    return res

class UnexpectedRowsError(Exception):
    def __init__(self, expected, actual, sql):
        self.expected, self.actual, self.sql = expected, actual, sql
        Exception.__init__(self, "SQL retrieved %d rows but %d was expected:\n%s" % \
                                 (self.actual, self.expected, self.sql))

def execSQLForSingleton(conn, sql):
    """
    Run SQL that returns exactly one row and one column, and return that cell

    TODO: Handle like gppylib.system.comfigurationImplGpdb.fetchSingleOutputRow().
    In the event of the wrong number of rows/columns, some logging would be helpful...
    """
    row = execSQLForSingletonRow(conn, sql)
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
