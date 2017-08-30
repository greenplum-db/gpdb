#! /usr/bin/env python

"""
Initialize, start, stop, or destroy WAL replication mirror segments.

============================= DISCLAIMER =============================
This is a developer tool to assist with development of WAL replication
for mirror segments.  This tool is not meant to be used in production.
It is suggested to only run this tool against a gpdemo cluster that
was initialized with no FileRep mirrors.

Example:
    WITH_MIRRORS=false make create-demo-cluster
======================================================================

Assumptions:
1. Greenplum cluster was compiled with --enable-segwalrep
2. Greenplum cluster was initialized without mirror segments.
3. Cluster is all on one host
4. Greenplum environment is all setup (greenplum_path.sh, MASTER_DATA_DIRECTORY, PGPORT, etc.)
5. Greenplum environment is started
6. Greenplum environment is the same throughout tool usage

Assuming all of the above, you can just run the tool as so:
    ./gpsegwalrep.py [init|start|stop|destroy]
"""

import argparse
import os
import sys
import subprocess
import threading
from gppylib.db import dbconn

PRINT_LOCK = threading.Lock()
THREAD_LOCK = threading.Lock()

def runcommands(commands, thread_name, command_finish, exit_on_error=True):
    output = []

    for command in commands:
        try:
            output.append('Running command... %s' % command)
            with THREAD_LOCK:
                output = output + subprocess.check_output(command, stderr=subprocess.STDOUT, shell=True).split('\n')
        except subprocess.CalledProcessError, e:
            output.append(str(e))
            output.append(e.output)
            if exit_on_error:
                with PRINT_LOCK:
                    for line in output:
                        print '%s:  %s' % (thread_name, line)
                    print ''

                sys.exit(e.returncode)

    output.append(command_finish)
    with PRINT_LOCK:
        for line in output:
            print '%s:  %s' % (thread_name, line)
        print ''

class InitMirrors():
    ''' Initialize the WAL replication mirror segment '''

    def __init__(self, segconfigs, hostname):
        self.segconfigs = segconfigs
        self.hostname = hostname

    def initThread(self, segconfig, user):
        commands = []
        primary_port = segconfig[2]
        primary_dir = segconfig[3]
        mirror_contentid = segconfig[1]
        mirror_dir = segconfig[3].replace('dbfast', 'dbfast_mirror')
        mirror_port = primary_port + 10000

        commands.append("echo 'host  replication  %s  samenet  trust' >> %s/pg_hba.conf" % (user, primary_dir))
        commands.append("pg_ctl -D %s reload" % primary_dir)
        commands.append("pg_basebackup -x -R -c fast -E ./pg_log -E ./db_dumps -E ./gpperfmon/data -E ./gpperfmon/logs -D %s -h %s -p %d" % (mirror_dir, self.hostname, primary_port))
        commands.append("mkdir %s/pg_log; mkdir %s/pg_xlog/archive_status" % (mirror_dir, mirror_dir))

        catalog_update_query = "select pg_catalog.gp_add_segment_mirror(%d::int2, '%s', '%s', %d, -1, '{pg_system, %s}')" % (mirror_contentid, self.hostname, self.hostname, mirror_port, mirror_dir)
        commands.append("PGOPTIONS=\"-c gp_session_role=utility\" psql postgres -c \"%s\"" % catalog_update_query)


        thread_name = 'Mirror content %d' % mirror_contentid
        command_finish = 'Initialized mirror at %s' % mirror_dir
        runcommands(commands, thread_name, command_finish)

    def run(self):
        # Assume db user is current user
        user = subprocess.check_output(["whoami"]).rstrip('\n')

        ''' Notify Primary of mirror addition, to start blocking. Currently do not have
        way to set GUC for specific segment. Hence at end of initializing all
        mirrors adding the GUC. Can't have it in InitMirrors as query to master
        at StartMirror gets blocked, so need to perform the same after starting
        mirrors. '''
        commands = []
        commands.append("gpconfig -c synchronous_standby_names -v \"*\"");
        commands.append("gpstop -u");
        runcommands(commands, "Main Mirror Init", "Notified primaries of mirror addition")

        initThreads = []
        for segconfig in self.segconfigs:
            if segconfig[4] == 'p':
                thread = threading.Thread(target=self.initThread, args=(segconfig, user))
                thread.start()
                initThreads.append(thread)

        for thread in initThreads:
            thread.join()

class StartMirrors():
    ''' Start the WAL replication mirror segment '''

    def __init__(self, segconfigs, host):
        self.segconfigs = segconfigs
        self.num_contents = len(segconfigs)
        self.host = host

    def startThread(self, segconfig):
        commands = []
        mirror_contentid = segconfig[1]
        mirror_port = segconfig[2]
        mirror_dir = segconfig[3]

        opts = "-p %d --gp_dbid=0 --silent-mode=true -i -M mirrorless --gp_contentid=%d --gp_num_contents_in_cluster=%d" % (mirror_port, mirror_contentid, self.num_contents)
        commands.append("pg_ctl -D %s -o '%s' start" % (mirror_dir, opts))
        commands.append("pg_ctl -D %s status" % mirror_dir)

        thread_name = 'Mirror content %d' % mirror_contentid
        command_finish = 'Started mirror with content %d and port %d at %s' % (mirror_contentid, mirror_port, mirror_dir)
        runcommands(commands, thread_name, command_finish)

    def run(self):
        startThreads = []
        for segconfig in self.segconfigs:
            if segconfig[4] == 'm':
                thread = threading.Thread(target=self.startThread, args=(segconfig,))
                thread.start()
                startThreads.append(thread)

        for thread in startThreads:
            thread.join()


class StopMirrors():
    ''' Stop the WAL replication mirror segment '''

    def __init__(self, segconfigs):
        self.segconfigs = segconfigs

    def stopThread(self, segconfig):
        commands = []
        mirror_contentid = segconfig[1]
        mirror_dir = segconfig[3]

        commands.append("pg_ctl -D %s stop" % mirror_dir)

        thread_name = 'Mirror content %d' % mirror_contentid
        command_finish = 'Stopped mirror at %s' % mirror_dir
        runcommands(commands, thread_name, command_finish)

    def run(self):
        stopThreads = []
        for segconfig in self.segconfigs:
            if segconfig[4] == 'm':
                thread = threading.Thread(target=self.stopThread, args=(segconfig,))
                thread.start()
                stopThreads.append(thread)

        for thread in stopThreads:
            thread.join()

class DestroyMirrors():
    ''' Destroy the WAL replication mirror segment '''

    def __init__(self, segconfigs):
        self.segconfigs = segconfigs

    def destroyThread(self, segconfig):
        commands = []
        mirror_contentid = segconfig[1]
        mirror_dir = segconfig[3]

        commands.append("pg_ctl -D %s stop" % mirror_dir)
        commands.append("rm -rf %s" % mirror_dir)

        catalog_update_query = "select pg_catalog.gp_remove_segment_mirror(%d::int2)" % (mirror_contentid)
        commands.append("PGOPTIONS=\"-c gp_session_role=utility\" psql postgres -c \"%s\"" % catalog_update_query)

        thread_name = 'Mirror content %d' % mirror_contentid
        command_finish = 'Destroyed mirror at %s' % mirror_dir
        runcommands(commands, thread_name, command_finish, False)

    def run(self):
        destroyThreads = []
        for segconfig in self.segconfigs:
            if segconfig[4] == 'm':
                thread = threading.Thread(target=self.destroyThread, args=(segconfig,))
                thread.start()
                destroyThreads.append(thread)

        for thread in destroyThreads:
            thread.join()

        commands = []

        '''
        Notify Primary of mirror deletion, to stop blocking. Currently do not
        have way to set GUC for specific segment. Hence at end of removing
        all mirrors removing the GUC.
        '''
        commands.append("gpconfig -r synchronous_standby_names");
        commands.append("gpstop -u");
        runcommands(commands, "Main Mirror Destroy", "Notified primaries of mirror removal")

def getSegInfo(hostname, port, dbname):
    query = "SELECT dbid, content, port, fselocation, preferred_role FROM gp_segment_configuration s, pg_filespace_entry f WHERE s.content != -1 AND s.dbid = fsedbid"
    dburl = dbconn.DbURL(hostname, port, dbname)

    try:
        with dbconn.connect(dburl, utility=True) as conn:
            segconfigs = dbconn.execSQL(conn, query).fetchall()
    except Exception, e:
        print e
        sys.exit(1)

    return segconfigs

def defargs():
    parser = argparse.ArgumentParser(description='Initialize, start, stop, or destroy WAL replication mirror segments')
    parser.add_argument('--host', type=str, required=False, default=os.getenv('PGHOST', 'localhost'),
                        help='Master host to get segment config information from')
    parser.add_argument('--port', type=str, required=False, default=os.getenv('PGPORT', '15432'),
                        help='Master port to get segment config information from')
    parser.add_argument('--database', type=str, required=False, default='postgres',
                        help='Database name to get segment config information from')
    parser.add_argument('operation', type=str, choices=['init', 'start', 'stop', 'destroy'])

    return parser.parse_args()

if __name__ == "__main__":
    # Get parsed args
    args = defargs()

    # Get information on all primary segments
    segconfigs = getSegInfo(args.host, args.port, args.database)

    # Execute the chosen operation
    if args.operation == 'init':
        InitMirrors(segconfigs, args.host).run()
    elif args.operation == 'start':
        StartMirrors(segconfigs, args.host).run()
    elif args.operation == 'stop':
        StopMirrors(segconfigs).run()
    elif args.operation == 'destroy':
        DestroyMirrors(segconfigs).run()
