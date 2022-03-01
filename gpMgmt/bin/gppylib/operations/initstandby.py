#!/usr/bin/env python3

import os
import json

from gppylib.gpparseopts import OptParser, OptChecker
from gppylib.mainUtils import addStandardLoggingAndHelpOptions
from collections import defaultdict
from gppylib import gplog
from gppylib.commands import unix, gp
from gppylib.commands.base import REMOTE, WorkerPool, Command
from gppylib.mainUtils import ExceptionNoStackTraceNeeded
from gppylib.operations import Operation

PG_HBA_BACKUP = 'pg_hba.conf.bak'

logger = gplog.get_default_logger()

DEFAULT_BATCH_SIZE = 16

def create_standby_pg_hba_entries(standby_host, is_hba_hostnames=False):
    standby_ips = gp.IfAddrs.list_addrs(standby_host)
    current_user = unix.UserId.local('get userid')
    standby_pg_hba_info = ['# standby coordinator host ip addresses\n']
    for ip in standby_ips:
        if not is_hba_hostnames:
            cidr_suffix = '/128' if ':' in ip else '/32' # MPP-15889
            address = ip + cidr_suffix
        else:
            address = standby_host
        standby_pg_hba_info.append('host\tall\t%s\t%s\ttrust\n' % (current_user, address))
    return standby_pg_hba_info

def cleanup_pg_hba_backup(data_dirs_list):
    """
    cleanup the backup of pg_hba.config created on segments
    """
    try:
        for data_dir in data_dirs_list:
            backup_file = os.path.join(data_dir, PG_HBA_BACKUP)
            if os.path.exists(backup_file):
                os.remove(backup_file)
                logger.info('Removing pg_hba.conf backup file %s' % backup_file)
            else:
                logger.warning('pg_hba.conf backup file %s does not exist' % backup_file)
    except Exception as ex:
        logger.error('Unable to cleanup backup of pg_hba.conf %s' % ex)

def cleanup_pg_hba_backup_on_segment(gparr, unreachable_hosts=[]):
    """
    Cleanup the pg_hba.conf on all of the segments
    present in the array
    """
    logger.debug('Removing pg_hba.conf backup file on segments...')

    host_to_seg_map = defaultdict(list)
    for seg in gparr.getDbList():
        if not seg.isSegmentCoordinator() and not seg.isSegmentStandby():
            host_to_seg_map[seg.getSegmentHostName()].append(seg.getSegmentDataDirectory())

    pool = WorkerPool(numWorkers=DEFAULT_BATCH_SIZE)

    try:
        for host, data_dirs_list in list(host_to_seg_map.items()):
            if host in unreachable_hosts:
                continue
            json_data_dirs_list = json.dumps(data_dirs_list)
            cmdStr = "$GPHOME/lib/python/gppylib/operations/initstandby.py -d '%s' -D" % json_data_dirs_list
            cmd = Command('Cleanup the pg_hba.conf backups on remote hosts', cmdStr=cmdStr , ctxt=REMOTE, remoteHost=host)
            pool.addCommand(cmd)

        pool.join()

        for item in pool.getCompletedItems():
            result = item.get_results()
            if result.rc != 0:
                logger.error('Unable to cleanup pg_hba.conf backup file %s' % str(result.stderr))
                logger.error('Please check the segment for more details')

    finally:
        pool.haltWork()
        pool.joinWorkers()
        pool = None

def restore_pg_hba(data_dirs_list):
    for data_dir in data_dirs_list:
        logger.info('Restoring pg_hba.conf for %s' % data_dir)
        os.system('mv %s/%s %s/pg_hba.conf' % (data_dir, PG_HBA_BACKUP, data_dir))

def restore_pg_hba_on_segment(gparr):
    """
    Restore the pg_hba.conf on all of the segments
    present in the array
    """
    logger.debug('Restoring pg_hba.conf file on segments...')

    host_to_seg_map = defaultdict(list)
    for seg in gparr.getDbList():
        if not seg.isSegmentCoordinator() and not seg.isSegmentStandby():
            host_to_seg_map[seg.getSegmentHostName()].append(seg.getSegmentDataDirectory())

    pool = WorkerPool(numWorkers=DEFAULT_BATCH_SIZE)

    try:
        for host, data_dirs_list in list(host_to_seg_map.items()):
            json_data_dirs_list = json.dumps(data_dirs_list)
            cmdStr = "$GPHOME/lib/python/gppylib/operations/initstandby.py -d '%s' -r" % json_data_dirs_list
            cmd = Command('Restore the pg_hba.conf on remote hosts', cmdStr=cmdStr , ctxt=REMOTE, remoteHost=host)
            pool.addCommand(cmd)

        pool.join()

        for item in pool.getCompletedItems():
            result = item.get_results()
            if result.rc != 0:
                logger.error('Unable to restore pg_hba.conf %s' % str(result.stderr))
                logger.error('Please check the segment for more details')

    finally:
        pool.haltWork()
        pool.joinWorkers()
        pool = None


def backup_pg_hba(data_dirs_list):
    try:
        for data_dir in data_dirs_list:
            logger.info('Backing up pg_hba.conf for %s' % data_dir)
            # back it up
            os.system('cp %s/pg_hba.conf %s/%s'% (data_dir, data_dir, PG_HBA_BACKUP))
    except Exception as ex:
        raise Exception('Failed to backup pg_hba.config file %s' % ex)

def update_pg_hba(standby_pg_hba_info, data_dirs_list):
    backup_pg_hba(data_dirs_list)
    pg_hba_content_list = []
    for data_dir in data_dirs_list:
        logger.info('Updating pg_hba.conf for %s' % data_dir)
        with open('%s/pg_hba.conf' % data_dir) as fp:
            pg_hba_contents = fp.read()

        if standby_pg_hba_info in pg_hba_contents:
            continue
        else:
            pg_hba_contents += standby_pg_hba_info
          
        with open('%s/pg_hba.conf' % data_dir, 'w') as fp:
            fp.write(pg_hba_contents)

        pg_hba_content_list.append(pg_hba_contents)
    return pg_hba_content_list

def create_parser():
    parser = OptParser(option_class=OptChecker,
                       description='update the pg_hba.conf on all segments')

    addStandardLoggingAndHelpOptions(parser, includeNonInteractiveOption=True)
    parser.add_option('-p', '--pg-hba-info', dest='pg_hba_info', metavar='<pg_hba entries>',
                      help='Entries that get added to pg_hba.conf file')
    parser.add_option('-d', '--data-dirs', dest='data_dirs', metavar='<list of data dirs>',
                      help='A list of all data directories present on this host')
    parser.add_option('-b', '--backup', action='store_true',
                      help='Backup the pg_hba.conf file')
    parser.add_option('-r', '--restore', action='store_true',
                      help='Restore the pg_hba.conf file')
    parser.add_option('-D', '--delete', action='store_true',
                      help='Cleanup the pg_hba.conf backup file')

    return parser

if __name__ == '__main__':
    parser = create_parser()
    (options, args) = parser.parse_args()
    data_dirs = json.loads(options.data_dirs)
    if options.pg_hba_info:
        pg_hba_info = json.loads(options.pg_hba_info)
        update_pg_hba(pg_hba_info, data_dirs)
    elif options.backup:
        backup_pg_hba(data_dirs)
    elif options.restore:
        restore_pg_hba(data_dirs)
    else:
        cleanup_pg_hba_backup(data_dirs)
