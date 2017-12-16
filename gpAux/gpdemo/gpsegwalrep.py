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
from sets import Set
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

def displaySegmentConfiguration():
    commands = []
    commands.append("psql postgres -c \"select * from gp_segment_configuration order by content, dbid\"")
    runcommands(commands, "", "")

class InitMirrors():
    ''' Initialize the WAL replication mirror segment '''

    def __init__(self, cluster_config, hostname, init=True):
        self.clusterconfig = cluster_config
        self.segconfigs = cluster_config.get_seg_configs()
        self.hostname = hostname
        self.init = init

    def initThread(self, segconfig, user):
        commands = []
        if self.init:
            primary_port = segconfig.port
            primary_dir = segconfig.datadir
            mirror_dir = cluster_config.get_pair_dir(segconfig)
            mirror_port = cluster_config.get_pair_port(segconfig)
        else:
            primary_port = cluster_config.get_pair_port(segconfig)
            primary_dir = cluster_config.get_pair_dir(segconfig)
            mirror_dir = segconfig.datadir
            mirror_port = segconfig.port

        mirror_contentid = segconfig.content

        if self.init:
            commands.append("echo 'host  replication  %s  samenet  trust' >> %s/pg_hba.conf" % (user, primary_dir))
            commands.append("pg_ctl -D %s reload" % primary_dir)

        # 1. create base backup
        commands.append("rm -rf %s" % mirror_dir);
        commands.append("pg_basebackup -x -R -c fast -E ./pg_log -E ./db_dumps -E ./gpperfmon/data -E ./gpperfmon/logs -D %s -h %s -p %d" % (mirror_dir, self.hostname, primary_port))
        commands.append("mkdir %s/pg_log; mkdir %s/pg_xlog/archive_status" % (mirror_dir, mirror_dir))

        if self.init:
            # 2. update catalog
            catalog_update_query = "select pg_catalog.gp_add_segment_mirror(%d::int2, '%s', '%s', %d, '%s')" % (mirror_contentid, self.hostname, self.hostname, mirror_port, mirror_dir)
            commands.append("PGOPTIONS=\"-c gp_session_role=utility\" psql postgres -c \"%s\"" % catalog_update_query)

        thread_name = 'Mirror content %d' % mirror_contentid
        command_finish = 'Initialized mirror at %s' % mirror_dir
        runcommands(commands, thread_name, command_finish)

    def run(self):
        # Assume db user is current user
        user = subprocess.check_output(["whoami"]).rstrip('\n')

        initThreads = []
        for segconfig in self.segconfigs:
            assert(segconfig.content != GpSegmentConfiguration.MASTER_CONTENT_ID)
            if self.init:
                assert(segconfig.role == GpSegmentConfiguration.ROLE_PRIMARY)
            else:
                assert(segconfig.role == GpSegmentConfiguration.ROLE_MIRROR)
            thread = threading.Thread(target=self.initThread, args=(segconfig, user))
            thread.start()
            initThreads.append(thread)

        for thread in initThreads:
            thread.join()

class StartInstances():
    ''' Start a greenplum segment '''

    def __init__(self, cluster_config, host, wait=False):
        self.clusterconfig = cluster_config
        self.segconfigs = cluster_config.get_seg_configs()
        self.host = host
        self.wait = wait

    def startThread(self, segconfig):
        commands = []
        waitstring = ''
        dbid = segconfig.dbid
        contentid = segconfig.content
        segment_port = segconfig.port
        segment_dir = segconfig.datadir
        segment_role = StartInstances.getRole(contentid)

        # Need to set the dbid to 0 on segments to prevent use in mmxlog records
        if contentid != GpSegmentConfiguration.MASTER_CONTENT_ID:
            dbid = 0

        opts = ("-p %d --gp_dbid=%d --silent-mode=true -i -M %s --gp_contentid=%d --gp_num_contents_in_cluster=%d" %
                (segment_port, dbid, segment_role, contentid, self.clusterconfig.get_num_contents()))

        # Arguments for the master. -x sets the dbid for the standby master. Hardcoded to 0 for now, but may need to be
        # refactored when we start to focus on the standby master.
        #
        # -E in GPDB will set Gp_entry_postmaster = true;
        # to start master in utility mode, need to remove -E and add -c gp_role=utility
        #
        # we automatically assume people want to start in master only utility mode
        # if the self.clusterconfig.get_num_contents() is 0
        if contentid == GpSegmentConfiguration.MASTER_CONTENT_ID:
            opts += " -x 0"
            if self.clusterconfig.get_num_contents() == 0:
                opts += " -c gp_role=utility"
            else:
                opts += " -E"

        if self.wait:
            waitstring = "-w -t 180"

        commands.append("pg_ctl -D %s %s -o '%s' start" % (segment_dir, waitstring, opts))
        commands.append("pg_ctl -D %s status" % segment_dir)

        if contentid == GpSegmentConfiguration.MASTER_CONTENT_ID:
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
            thread = threading.Thread(target=self.startThread, args=(segconfig,))
            thread.start()
            startThreads.append(thread)

        for thread in startThreads:
            thread.join()

class StopInstances():
    ''' Stop all segments'''

    def __init__(self, cluster_config):
        self.clusterconfig = cluster_config
        self.segconfigs = cluster_config.get_seg_configs()

    def stopThread(self, segconfig):
        commands = []
        segment_contentid = segconfig.content
        segment_dir = segconfig.datadir

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

    def run(self):
        stopThreads = []
        for segconfig in self.segconfigs:
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
        mirror_dir = segconfig.datadir

        commands.append("pg_ctl -D %s stop" % mirror_dir)
        commands.append("rm -rf %s" % mirror_dir)
        thread_name = 'Mirror content %d' % mirror_contentid
        command_finish = 'Destroyed mirror at %s' % mirror_dir
        runcommands(commands, thread_name, command_finish, False)

        # Let FTS recognize that mirrors are gone.  As a result,
        # primaries will be marked not-in-sync.  If this step is
        # omitted, FTS will stop probing as soon as mirrors are
        # removed from catalog and primaries will be left "in-sync"
        # without mirrors.
        #
        # FIXME: enhance gp_remove_segment_mirror() to do this, so
        # that utility remains simplified.  Remove this stopgap
        # thereafter.
        ForceFTSProbeScan(self.clusterconfig,
                          GpSegmentConfiguration.STATUS_DOWN,
                          GpSegmentConfiguration.NOT_IN_SYNC)

        commands = []
        catalog_update_query = "select pg_catalog.gp_remove_segment_mirror(%d::int2)" % (mirror_contentid)
        commands.append("PGOPTIONS=\"-c gp_session_role=utility\" psql postgres -c \"%s\"" % catalog_update_query)

        command_finish = 'Removed mirror %s from catalog' % mirror_dir
        runcommands(commands, thread_name, command_finish, False)

    def run(self):
        destroyThreads = []
        for segconfig in self.segconfigs:
            assert(segconfig.preferred_role == GpSegmentConfiguration.ROLE_MIRROR)
            thread = threading.Thread(target=self.destroyThread, args=(segconfig,))
            thread.start()
            destroyThreads.append(thread)

        for thread in destroyThreads:
            thread.join()

class GpSegmentConfiguration():
    ROLE_PRIMARY = 'p'
    ROLE_MIRROR = 'm'
    STATUS_DOWN = 'd'
    STATUS_UP = 'u'
    NOT_IN_SYNC = 'n'
    IN_SYNC = 's'
    MASTER_CONTENT_ID = -1

    def __init__(self, dbid, content, port, datadir, role, preferred_role, status, mode):
        self.dbid = dbid
        self.content = content
        self.port = port
        self.datadir = datadir
        self.role = role
        self.preferred_role = preferred_role
        self.status = status
        self.mode = mode

class ClusterConfiguration():
    ''' Cluster configuration '''

    def __init__(self, hostname, port, dbname, role = "all", status = "all", include_master = True):
        self.hostname = hostname
        self.port = port
        self.dbname = dbname
        self.role = role
        self.status = status
        self.include_master = include_master
        self._all_seg_configs = None
        self.refresh()

    def get_num_contents(self):
        return self.num_contents;

    def get_seg_configs(self):
        return self.seg_configs;

    def get_pair_port(self, input_config):
        for seg_config in self._all_seg_configs:
            if (seg_config.content == input_config.content
                and seg_config.role != input_config.role):
                return seg_config.port

        assert(input_config.role == GpSegmentConfiguration.ROLE_PRIMARY)
        ''' if not found then assume its mirror and hence return port at which mirror must be created '''
        return input_config.port + 10000

    def get_pair_dir(self, input_config):
        for seg_config in self._all_seg_configs:
            if (seg_config.content == input_config.content
                and seg_config.role != input_config.role):
                return seg_config.datadir

        assert(input_config.role == GpSegmentConfiguration.ROLE_PRIMARY)
        ''' if not found then assume its mirror and hence return location at which mirror must be created '''
        return input_config.datadir.replace('dbfast', 'dbfast_mirror')

    def get_gp_segment_ids(self):
        ids = []
        for seg_config in self.seg_configs:
            ids.append(str(seg_config.content))
        return ','.join(ids)

    def refresh(self):
        query = ("SELECT dbid, content, port, datadir, role, preferred_role, status, mode "
                "FROM gp_segment_configuration s WHERE 1 = 1")

        print '%s: fetching cluster configuration' % (datetime.datetime.now())
        dburl = dbconn.DbURL(self.hostname, self.port, self.dbname)
        print '%s: fetched cluster configuration' % (datetime.datetime.now())

        try:
            with dbconn.connect(dburl, utility=True) as conn:
               resultsets  = dbconn.execSQL(conn, query).fetchall()
        except Exception, e:
            print e
            sys.exit(1)

        contentIDs = Set()
        self.seg_configs = []
        self.num_contents = 0
        for result in resultsets:
            seg_config = GpSegmentConfiguration(result[0], result[1], result[2], result[3], result[4], result[5], result[6], result[7])
            self._all_seg_configs.append(seg_config)

            append = True
            if (self.status != "all"
                and self.status != seg_config.status):
                append = False
            if (self.role != "all"
                and self.role != seg_config.role):
                append = False

            if (not self.include_master
                and seg_config.content == GpSegmentConfiguration.MASTER_CONTENT_ID):
                append = False

            if append:
                self.seg_configs.append(seg_config)

            # Count distinct content IDs, not including the master.
            if (seg_config.content != GpSegmentConfiguration.MASTER_CONTENT_ID):
                contentIDs.add(seg_config.content)

        self.num_contents = len(contentIDs)
        print 'found %d distinct content IDs' % (self.num_contents)

    def check_status_and_mode(self, expected_status, expected_mode):
        ''' Check if all the instance reached the expected_state and expected_mode '''

        for seg_config in self.seg_configs:
            if (seg_config.status != expected_status
                or seg_config.mode != expected_mode):
                return False

        return True

class ColdMasterClusterConfiguration(ClusterConfiguration):

    # this constructor is only used for ColdStartMaster
    def __init__(self, port, master_directory):
        self.seg_configs = []

        master_seg_config = GpSegmentConfiguration(1, GpSegmentConfiguration.MASTER_CONTENT_ID,
                                                   port, master_directory,
                                                   GpSegmentConfiguration.ROLE_PRIMARY,
                                                   GpSegmentConfiguration.ROLE_PRIMARY,
                                                   GpSegmentConfiguration.STATUS_DOWN,
                                                   GpSegmentConfiguration.NOT_IN_SYNC)
        self.seg_configs.append(master_seg_config)

        self.num_contents = 0

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
    parser.add_argument('operation', type=str, choices=['clusterstart', 'clusterstop', 'init', 'start', 'stop', 'destroy', 'recover', 'recoverfull'])

    return parser.parse_args()

def GetNumberOfSegments(input_segments):
    if len(input_segments) > 0:
        return len(input_segments.split(','))
    return 0

def WaitForRecover(cluster_configuration, max_retries = 200):
    '''Wait for the gp_stat_replication to reach given sync_error'''

    cmd_all_sync = ("psql postgres -A -R ',' -t -c \"SELECT gp_segment_id"
                    " FROM gp_stat_replication"
                    " WHERE gp_segment_id in (%s) and coalesce(sync_state, 'NULL') = 'sync'\"" %
                    cluster_configuration.get_gp_segment_ids())

    cmd_find_error = ("psql postgres -A -R ',' -t -c \"SELECT gp_segment_id"
                      " FROM gp_stat_replication"
                      " WHERE gp_segment_id in (%s) and sync_error != 'none'\"" %
                      cluster_configuration.get_gp_segment_ids())

    number_of_segments = len(cluster_configuration.seg_configs)

    print "cmd_all_sync: %s" % cmd_all_sync
    print "cmd_find_error: %s" % cmd_find_error
    print "number of contents: %s " % number_of_segments

    retry_count = 1
    while (retry_count < max_retries):
        result_all_sync = subprocess.check_output(cmd_all_sync, stderr=subprocess.STDOUT, shell=True).strip()
        number_of_all_sync = GetNumberOfSegments(result_all_sync)

        result_find_error = subprocess.check_output(cmd_find_error, stderr=subprocess.STDOUT, shell=True).strip()
        number_of_find_error = GetNumberOfSegments(result_find_error)

        if number_of_all_sync + number_of_find_error == number_of_segments:
            return result_find_error
        else:
            retry_count += 1

    print "WARNING: Incremental recovery took longer than expected!"
    cmd_find_recovering = ("psql postgres -A -R ',' -t -c \"SELECT gp_segment_id"
                           " FROM gp_stat_replication"
                           " WHERE gp_segment_id in (%s) and sync_error = 'none'\"" %
                           cluster_configuration.get_gp_segment_ids())
    result_find_recovering = subprocess.check_output(cmd_find_recovering, stderr=subprocess.STDOUT, shell=True).strip()
    return result_find_recovering

def ForceFTSProbeScan(cluster_configuration, expected_status = None, expected_mode = None, max_probes=2000):
    '''Force FTS probe scan to reflect primary and mirror status in catalog.'''

    commands = []
    commands.append("psql postgres -c \"SELECT gp_request_fts_probe_scan()\"")

    probe_count = 1
    # if the expected_mirror_status is not None, we wait until the mirror status is updated
    while(True):
        runcommands(commands, "Force FTS probe scan", "FTS probe refreshed catalog")

        if (expected_status == None or expected_mode == None):
            return

        cluster_configuration.refresh()

        if (cluster_configuration.check_status_and_mode(expected_status, expected_mode)):
            return

        if probe_count >= max_probes:
            print("ERROR: Server did not trasition to expected_mirror_status %s within %d probe attempts"
                  % (expected_status, probe_count))
            sys.exit(1)

        probe_count += 1

        time.sleep(0.1)

if __name__ == "__main__":
    # Get parsed args
    args = defargs()

    # Execute the chosen operation
    if args.operation == 'init':
        cluster_config = ClusterConfiguration(args.host, args.port, args.database,
                                              role=GpSegmentConfiguration.ROLE_PRIMARY, include_master=False)
        InitMirrors(cluster_config, args.host).run()
        cluster_config = ClusterConfiguration(args.host, args.port, args.database,
                                              role=GpSegmentConfiguration.ROLE_MIRROR, include_master=False)
        ForceFTSProbeScan(cluster_config, GpSegmentConfiguration.STATUS_DOWN, GpSegmentConfiguration.NOT_IN_SYNC)
    elif args.operation == 'clusterstart':
        # If we are starting the cluster, we need to start the master before we get the segment info
        cold_master_cluster_config = ColdMasterClusterConfiguration(int(args.port), args.master_directory)
        StartInstances(cold_master_cluster_config, args.host, wait=True).run()
        cluster_config = ClusterConfiguration(args.host, args.port, args.database)
        StopInstances(cold_master_cluster_config).run()
        StartInstances(cluster_config, args.host).run()
        ForceFTSProbeScan(cluster_config)
    elif args.operation == 'start':
        cluster_config = ClusterConfiguration(args.host, args.port, args.database,
                                              role=GpSegmentConfiguration.ROLE_MIRROR,
                                              status=GpSegmentConfiguration.STATUS_DOWN)
        StartInstances(cluster_config, args.host).run()
        ForceFTSProbeScan(cluster_config, GpSegmentConfiguration.STATUS_UP, GpSegmentConfiguration.IN_SYNC)
    elif args.operation == 'recover':
        cluster_config = ClusterConfiguration(args.host, args.port, args.database,
                                              role=GpSegmentConfiguration.ROLE_MIRROR,
                                              status=GpSegmentConfiguration.STATUS_DOWN)
        if len(cluster_config.seg_configs) > 0:
            StartInstances(cluster_config, args.host).run()
            failed_gp_segment_ids = WaitForRecover(cluster_config)
            if len(failed_gp_segment_ids) > 0:
                print("ERROR: incremental recovery failed for some segments (%s)" % failed_gp_segment_ids)
                cluster_config.refresh()
                StopInstances(cluster_config).run()
                ForceFTSProbeScan(cluster_config)
                sys.exit(1)
            ForceFTSProbeScan(cluster_config, GpSegmentConfiguration.STATUS_UP, GpSegmentConfiguration.IN_SYNC)
    elif args.operation == 'recoverfull':
        cluster_config = ClusterConfiguration(args.host, args.port, args.database,
                                              role=GpSegmentConfiguration.ROLE_MIRROR,
                                              status=GpSegmentConfiguration.STATUS_DOWN)
        if len(cluster_config.seg_configs) > 0:
            InitMirrors(cluster_config, args.host, False).run()
            StartInstances(cluster_config, args.host).run()
        ForceFTSProbeScan(cluster_config, GpSegmentConfiguration.STATUS_UP, GpSegmentConfiguration.IN_SYNC)
    elif args.operation == 'stop':
        cluster_config = ClusterConfiguration(args.host, args.port, args.database,
                                              role=GpSegmentConfiguration.ROLE_MIRROR,
                                              status=GpSegmentConfiguration.STATUS_UP)
        StopInstances(cluster_config).run()
        ForceFTSProbeScan(cluster_config, GpSegmentConfiguration.STATUS_DOWN, GpSegmentConfiguration.NOT_IN_SYNC)
    elif args.operation == 'destroy':
        cluster_config = ClusterConfiguration(args.host, args.port, args.database,
                                              role=GpSegmentConfiguration.ROLE_MIRROR)
        DestroyMirrors(cluster_config).run()
    elif args.operation == 'clusterstop':
        cluster_config = ClusterConfiguration(args.host, args.port, args.database)
        StopInstances(cluster_config).run()

    if args.operation != 'clusterstop':
        displaySegmentConfiguration()
