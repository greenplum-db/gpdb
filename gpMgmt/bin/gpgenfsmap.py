#!/usr/bin/env python
#
# Copyright (c) Greenplum Inc 2011. All Rights Reserved.
'''
gpgenfsmap.py     Generates a mapping of Servers, Filesystems, GPDB Filespaces and Storage Pools
Options: 
    -h, -?, --help   Print this usage.
    -p --port        Port to use to connect to DB.  Defaults to $PGPORT
    -u --username    Username to connect to DB.  Defaults to $PGUSER
    -n --host        Hostname to connect to DB. Defaults to $PGHOST
    -w --password    Password to connect to DB.
    -v, --verbose    Enable verbose Logging.  Adds messages to stdout that will may break programs that parse the fsmap output.
    -q, --quiet      Disable most log messages.  This mode is on by default.
'''

import os, sys, subprocess

try:
    from optparse import Option, OptionParser
    from gppylib.gpparseopts import OptParser, OptChecker
    from gppylib.commands.base import WorkerPool, Command, REMOTE
    from gppylib import gphostcache
    from gppylib.commands import unix
    from gppylib.db import dbconn
    from gppylib.db import catalog
    from pygresql import pg  # Database interaction
    from gppylib import gplog  # Greenplum logging facility
    from getpass import getpass

except ImportError, e:
    sys.exit('Cannot import modules.  Please check that you have sourced greenplum_path.sh.  Detail: ' + str(e))


# Data Structures to be populated
class FSMap:
    def __init__(self):
        self.servers = []
        self.filespaces = []

    def addServer(self, name):
        newSvr = Server(name)
        self.servers.append(newSvr)
        return newSvr

    def getServer(self, name):
        for server in self.servers:
            if server.name == name:
                return server
        return None

    def getAllServers(self):
        return self.servers

    def addFilespace(self, filespace):
        self.filespaces.append(filespace)

    def toString(self):
        mapStr = ""
        for svr in self.servers:
            mapStr += svr.toString()
        return mapStr


class Server:
    def __init__(self, name):
        self.name = name
        self.filesystems = []

    def addFilesystem(self, name):
        newFs = Filesystem(name)
        self.filesystems.append(newFs)
        return newFs

    def getFilesystem(self, name):
        for filesystem in self.filesystems:
            if filesystem.name == name:
                return filesystem
        return None

    def toString(self):
        svrStr = ""
        for fs in self.filesystems:
            fsStrList = fs.toString()
            for fsStr in fsStrList:
                svrStr += "%s:%s:%s\n" % (self.name, fs.name, fsStr)
        return svrStr


# Global Variables
systemFSMap = None
# Keyed on servername, each value is a list of devices & directories
serverFSDict = None
serverFSMap = dict()
fsDetailsMap = dict()
dbConn = None
logger = None


def runPoolCommand(host, commandStr, pool):
    output = ""

    cmd = Command(host, commandStr, REMOTE, host)
    pool.addCommand(cmd)
    pool.join()
    items = pool.getCompletedItems()
    for i in items:
        if i.results.rc or i.results.halt or not i.results.completed:
            logger.info("Error running command on host: " + host + " Command: " + commandStr)
            logger.info(i.results.stderr.strip())
            logger.info(i.results.stdout.strip())
        output = i.results.stdout
    return output


def dbConnect(options):
    global dbConn

    if dbConn != None:
        return
    try:
        user = options.username
        host = options.host
        port = options.port
        db = 'template1'
        password = None

        dburl = dbconn.DbURL(username=user, hostname=host,
                             port=port, dbname=db, password=password)
        logger.info("Connecting to DB at: %s " % (str(dburl)))

        conn = dbconn.connect(dburl)
        logger.info("Connected to DB: %s" % (str(conn)))
        dbConn = conn
    except Exception, e:
        logger.error("Error Connecting to Database: %s" % (str(e)))
        sys.exit(1)


def runQuery(query):
    global dbConn
    if dbConn == None:
        logger.error("Error: Not connected to database.")
        sys.exit(1)

    try:
        rows = catalog.basicSQLExec(dbConn, query)
    except Exception, e:
        logger.error("Error: Failed to run query: %s" % (str(e)))
        sys.exit(1)
    return rows


def parseargs(args):
    global logger

    pguser = os.environ.get("PGUSER") or unix.getUserName()
    pghost = os.environ.get("PGHOST") or unix.getLocalHostname()
    pgport = os.environ.get("PGPORT") or 5432

    parser = OptParser(option_class=OptChecker)
    parser.remove_option('-h')
    parser.add_option('-?', '--help', '-h', action='store_true', default=False)
    parser.add_option('-n', '--host', default=pghost)
    parser.add_option('-p', '--port', default=pgport)
    parser.add_option('-u', '--username', default=pguser)
    parser.add_option('-w', '--password', default=False, action='store_true')
    parser.add_option('-v', '--verbose', default=False, action='store_true')
    parser.add_option('-q', '--quiet', default=True, action='store_true')

    (options, args) = parser.parse_args()

    if options.help:
        print __doc__
        sys.exit(1)
    try:
        options.port = int(options.port)
    except:
        logger.error("Invalid PORT: '%s'" % options.port)
        sys.exit(1)

    if options.verbose:
        gplog.enable_verbose_logging()
    elif options.quiet:
        gplog.quiet_stdout_logging()

    return options


def findFsDetails():
    global serverFSMap
    try:
        # find the mount points in parallel
        pool = WorkerPool()

        for hname in serverFSMap.keys():
            hname.strip()
            subCmd = "df -P %s" % (serverFSMap[hname])
            cmdStr = 'ssh -o PasswordAuthentication=no %s "%s"' % (hname, subCmd)
            pool.addCommand(Command(hname, cmdStr, REMOTE, hname))
        pool.join()
        items = pool.getCompletedItems()
        for i in items:
            if i.results.rc == 0:
                df_with_header = i.results.stdout.strip()
                df_list = df_with_header.splitlines()
                df_list.pop(0)
                fsList = serverFSMap[i.remoteHost].split()
                if len(df_list) != len(fsList):
                    print "Mismatch"
                    continue
                for df_vals in df_list:
                    df_val = df_vals.split()
                    fsDetailsMap[fsList.pop(0).strip()] = [i.remoteHost, df_val[0], df_val[5]]
            else:
                print("Failure in talking to host %s" % (i.remoteHost))

        pool.join()
        pool.haltWork()
        pool.joinWorkers()

    except Exception, e:
        print e.__str__()
        pool.join()
        pool.haltWork()
        pool.joinWorkers()
    except KeyboardInterrupt:
        pool.join()
        pool.haltWork()
        pool.joinWorkers()
        sys.exit(1)
    except:
        pool.join()
        pool.haltWork()
        pool.joinWorkers()


def genFSMap():
    results = runQuery("SELECT pgfs.oid AS oid, fsedbid AS seg_dbid, fselocation AS datadir, hostname \
                        FROM pg_filespace pgfs, pg_filespace_entry pgfse,  gp_segment_configuration gpsec \
                        WHERE pgfse.fsefsoid=pgfs.oid AND pgfse.fsedbid=gpsec.dbid  ORDER BY seg_dbid;")
    if len(results) == 0:
        logger.error("No data directories found.  Exiting.")
        sys.exit(1)
    logger.debug("Results from query:" + str(results))
    # result columns will be: [0] filespace name, [1] dbid, [2] datadir, [3] hostname
    for result in results:
        filespaceOid = result[0]
        datadir = result[2].strip()
        hostname = result[3]

        if fsDetailsMap.has_key(datadir):
            print fsDetailsMap[datadir][0], fsDetailsMap[datadir][2], filespaceOid


def genServerFsList():
    results = runQuery(
        "SELECT hostname, array_to_string(array_agg(fselocation), ' ') AS fs FROM pg_filespace_entry a , gp_segment_configuration b WHERE a.fsedbid = b.dbid GROUP BY hostname")

    if len(results) == 0:
        logger.error("Error: gp_segment_configuration empty")
        sys.exit(1)

    for item in results:
        if len(item) == 2:
            serverFSMap[item[0]] = item[1]


def main(argv):
    global systemFSMap
    global logger

    logger = gplog.get_default_logger()

    options = parseargs(argv)
    dbConnect(options)
    genServerFsList()
    findFsDetails()
    genFSMap()
    sys.exit(0)


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
