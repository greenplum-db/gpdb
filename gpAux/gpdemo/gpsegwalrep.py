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
import datetime
import time
from gppylib.db import dbconn

PRINT_LOCK = threading.Lock()
THREAD_LOCK = threading.Lock()

def runcommands(commands, thread_name, command_finish, exit_on_error=True):
    output = []

    for command in commands:
        try:
            output.append('%s: Running command... %s' % (datetime.datetime.now(), command))
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

    output.append('%s: %s' % (datetime.datetime.now(), command_finish))
    with PRINT_LOCK:
        for line in output:
            print '%s:  %s' % (thread_name, line)
        print ''

class InitMirrors():
    ''' Initialize the WAL replication mirror segment '''

    def __init__(self, cluster_config, hostname):
        self.clusterconfig = cluster_config
        self.segconfigs = cluster_config.get_seg_configs()
        self.hostname = hostname

    def initThread(self, segconfig, user):
        commands = []
        primary_port = segconfig.port
        primary_dir = segconfig.fselocation
        mirror_contentid = segconfig.content
        mirror_dir = segconfig.fselocation.replace('dbfast', 'dbfast_mirror')
        mirror_port = primary_port + 10000

        commands.append("echo 'host  replication  %s  samenet  trust' >> %s/pg_hba.conf" % (user, primary_dir))
        commands.append("pg_ctl -D %s reload" % primary_dir)

        # 1. create base backup
        commands.append("pg_basebackup -x -R -c fast -E ./pg_log -E ./db_dumps -E ./gpperfmon/data -E ./gpperfmon/logs -D %s -h %s -p %d" % (mirror_dir, self.hostname, primary_port))
        commands.append("mkdir %s/pg_log; mkdir %s/pg_xlog/archive_status" % (mirror_dir, mirror_dir))

        # 2. update catalog
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
            if segconfig.preferred_role == GpSegmentConfiguration.ROLE_PRIMARY and segconfig.content != GpSegmentConfiguration.MASTER_CONTENT_ID:
                thread = threading.Thread(target=self.initThread, args=(segconfig, user))
                thread.start()
                initThreads.append(thread)

        for thread in initThreads:
            thread.join()

class StartInstances():
    ''' Start a greenplum segment '''

    def __init__(self, cluster_config, host, segment_type='all', wait=False):
        self.clusterconfig = cluster_config
        self.segconfigs = cluster_config.get_seg_configs()
        self.host = host
        self.segment_type = segment_type
        self.wait = wait

    def startThread(self, segconfig):
        commands = []
        waitstring = ''
        dbid = segconfig.dbid
        contentid = segconfig.content
        segment_port = segconfig.port
        segment_dir = segconfig.fselocation
        segment_role = StartInstances.getRole(contentid)

        # Need to set the dbid to 0 on segments to prevent use in mmxlog records
        if contentid != -1:
            dbid = 0

        opts = "-p %d --gp_dbid=%d --silent-mode=true -i -M %s --gp_contentid=%d --gp_num_contents_in_cluster=%d" % \
               (segment_port, dbid, segment_role, contentid, self.clusterconfig.get_num_contents())

        # Arguments for the master. -x sets the dbid for the standby master. Hardcoded to 0 for now, but may need to be
        # refactored when we start to focus on the standby master.
        #
        # -E echos internal commands to the  terminal. Set here mostly to maintain parity with gpstart behavior.
        if contentid == -1:
            opts += " -x 0 -E"

        if self.wait:
            waitstring = "-w -t 180"

        commands.append("pg_ctl -D %s %s -o '%s' start" % (segment_dir, waitstring, opts))
        commands.append("pg_ctl -D %s status" % segment_dir)

        if contentid == -1:
            segment_label = 'master'
        elif segconfig.preferred_role == GpSegmentConfiguration.ROLE_PRIMARY:
            segment_label = 'primary'
        else:
            segment_label = 'mirror'

        thread_name = 'Segment %s content %d' % (segment_label, contentid)
        command_finish = 'Started %s segment with content %d and port %d at %s' % (segment_label, contentid, segment_port, segment_dir)
        runcommands(commands, thread_name, command_finish)

    @staticmethod
    def getRole(contentid):
        if contentid == GpSegmentConfiguration.MASTER_CONTENT_ID:
            return 'master'
        else:
            return 'mirrorless'

    def run(self):
        startThreads = []
        for segconfig in self.segconfigs:
            if self.segment_type == 'all' or segconfig.preferred_role == self.segment_type:
                thread = threading.Thread(target=self.startThread, args=(segconfig,))
                thread.start()
                startThreads.append(thread)

        for thread in startThreads:
            thread.join()

class StopInstances():
    ''' Stop all segments'''

    def __init__(self, cluster_config, segment_type='all'):
        self.clusterconfig = cluster_config
        self.segconfigs = cluster_config.get_seg_configs()
        self.segment_type = segment_type

    def stopThread(self, segconfig):
        commands = []
        segment_contentid = segconfig.content
        segment_dir = segconfig.fselocation

        if segment_contentid == GpSegmentConfiguration.MASTER_CONTENT_ID:
            segment_type = 'master'
        elif segconfig.preferred_role == GpSegmentConfiguration.ROLE_PRIMARY:
            segment_type = 'primary'
        else:
            segment_type = 'mirror'

        commands.append("pg_ctl -D %s stop" % segment_dir)

        thread_name = 'Segment %s content %d' % (segment_type, segment_contentid)
        command_finish = 'Stopped %s segment at %s' % (segment_type, segment_dir)
        runcommands(commands, thread_name, command_finish)

    def shouldTerminate(self, segconfig):
        if self.segment_type == 'all':
            return True

        if self.segment_type == 'master' and segconfig.content == GpSegmentConfiguration.MASTER_CONTENT_ID:
            return True

        if segconfig.preferred_role == self.segment_type:
            return True

        return False

    def run(self):
        stopThreads = []
        for segconfig in self.segconfigs:
            if self.shouldTerminate(segconfig):
                thread = threading.Thread(target=self.stopThread, args=(segconfig,))
                thread.start()
                stopThreads.append(thread)

        for thread in stopThreads:
            thread.join()

class DestroyMirrors():
    ''' Destroy the WAL replication mirror segment '''

    def __init__(self, cluster_config):
        self.clusterconfig = cluster_config
        self.segconfigs = cluster_config.get_seg_configs()

    def destroyThread(self, segconfig):
        commands = []
        mirror_contentid = segconfig.content
        mirror_dir = segconfig.fselocation

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
            if segconfig.preferred_role == GpSegmentConfiguration.ROLE_MIRROR:
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

class GpSegmentConfiguration():
    ROLE_PRIMARY = 'p'
    ROLE_MIRROR = 'm'
    MIRROR_DOWN = 'd'
    MIRROR_UP = 'u'
    MASTER_CONTENT_ID = -1

    def __init__(self, dbid, content, port, fselocation, preferred_role, status):
        self.dbid = dbid
        self.content = content
        self.port = port
        self.fselocation = fselocation
        self.preferred_role = preferred_role
        self.status = status

class ClusterConfiguration():
    ''' Cluster configuration '''

    def __init__(self, hostname, port, dbname):
        self.hostname = hostname
        self.port = port
        self.dbname = dbname
        self.refresh()

    def get_num_contents(self):
        return self.num_contents;

    def get_seg_configs(self):
        return self.seg_configs;

    def refresh(self):
        query = "SELECT dbid, content, port, fselocation, preferred_role, status FROM gp_segment_configuration s, pg_filespace_entry f WHERE s.dbid = fsedbid"
        print '%s: fetching cluster configuration' % (datetime.datetime.now())
        dburl = dbconn.DbURL(self.hostname, self.port, self.dbname)
        print '%s: fetched cluster configuration' % (datetime.datetime.now())

        try:
            with dbconn.connect(dburl, utility=True) as conn:
               resultsets  = dbconn.execSQL(conn, query).fetchall()
        except Exception, e:
            print e
            sys.exit(1)

        self.seg_configs = []
        self.num_contents = 0
        for result in resultsets:
            seg_config = GpSegmentConfiguration(result[0], result[1], result[2], result[3], result[4], result[5])
            self.seg_configs.append(seg_config)

            # Count primary segments
            if seg_config.preferred_role == GpSegmentConfiguration.ROLE_PRIMARY and seg_config.content  != GpSegmentConfiguration.MASTER_CONTENT_ID:
                self.num_contents += 1

    def check_mirror_status(self, expected_mirror_status):
        ''' Check if all the mirror reached the expected_mirror_state '''

        for seg_config in self.seg_configs:
            if seg_config.preferred_role == GpSegmentConfiguration.ROLE_MIRROR and seg_config.status != expected_mirror_status:
                return False

        return True

class ColdMasterClusterConfiguration(ClusterConfiguration):

    # this constructor is only used for ColdStartMaster
    def __init__(self, port, master_directory):
        self.seg_configs = []

        master_seg_config = GpSegmentConfiguration(1, -1, port, master_directory, GpSegmentConfiguration.ROLE_PRIMARY)
        self.seg_configs.append(master_seg_config)

        self.num_contents = 1 # need to be > 1 to avoid assert failure

def defargs():
    parser = argparse.ArgumentParser(description='Initialize, start, stop, or destroy WAL replication mirror segments')
    parser.add_argument('--host', type=str, required=False, default=os.getenv('PGHOST', 'localhost'),
                        help='Master host to get segment config information from')
    parser.add_argument('--port', type=str, required=False, default=os.getenv('PGPORT', '15432'),
                        help='Master port to get segment config information from')
    parser.add_argument('--master-directory', type=str, required=False, default=os.getenv('MASTER_DATA_DIRECTORY'),
                        help='Master port to get segment config information from')
    parser.add_argument('--database', type=str, required=False, default='postgres',
                        help='Database name to get segment config information from')
    parser.add_argument('operation', type=str, choices=['init', 'clusterstart', 'start', 'stop', 'destroy'])

    return parser.parse_args()

def ForceFTSProbeScan(cluster_configuration, expected_mirror_status = None):
    '''Force FTS probe scan to reflect primary and mirror status in catalog.'''

    commands = []
    commands.append("psql postgres -c \"SELECT gp_request_fts_probe_scan()\"")

    # if the expected_mirror_status is not None, we wait until the mirror status is updated
    while(True):
        runcommands(commands, "Force FTS probe scan", "FTS probe refreshed catalog")

        if (expected_mirror_status == None):
            return

        cluster_configuration.refresh()

        if (cluster_configuration.check_mirror_status(expected_mirror_status)):
            return

        time.sleep(0.1)

if __name__ == "__main__":
    # Get parsed args
    args = defargs()

    # If we are starting the cluster, we need to start the master before we get the segment info
    if args.operation == 'clusterstart':
        cold_master_cluster_config = ColdMasterClusterConfiguration(int(args.port), args.master_directory)
        StartInstances(cold_master_cluster_config, args.host, segment_type=GpSegmentConfiguration.ROLE_PRIMARY, wait=True).run()

    # Get information on all segments
    cluster_config = ClusterConfiguration(args.host, args.port, args.database)

    if args.operation == 'clusterstart':
        StopInstances(cold_master_cluster_config, 'master').run()

    # Execute the chosen operation
    if args.operation == 'init':
        InitMirrors(cluster_config, args.host).run()
        ForceFTSProbeScan(cluster_config, GpSegmentConfiguration.MIRROR_DOWN)
    elif args.operation == 'clusterstart':
        StartInstances(cluster_config, args.host).run()
        ForceFTSProbeScan(cluster_config)
    elif args.operation == 'start':
        StartInstances(cluster_config, args.host, segment_type=GpSegmentConfiguration.ROLE_MIRROR).run()
        ForceFTSProbeScan(cluster_config, GpSegmentConfiguration.MIRROR_UP)
    elif args.operation == 'stop':
        StopInstances(cluster_config, segment_type=GpSegmentConfiguration.ROLE_MIRROR).run()
        ForceFTSProbeScan(cluster_config, GpSegmentConfiguration.MIRROR_DOWN)
    elif args.operation == 'destroy':
        DestroyMirrors(cluster_config).run()
