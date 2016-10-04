#!/usr/bin/env python

# Copyright (c) 2014 Pivotal Software, Inc. All Rights Reserved
#
# This software contains the intellectual property of Pivotal Software, Inc.
# or is licensed to Pivotal Software, Inc. from third parties. Use of this
# software and the intellectual property contained therein is expressly
# limited to the terms and conditions of the License Agreement under which
# it is provided by or on behalf of Pivotal Software, Inc.

import base64
import fileinput
import os
import pickle
import platform
import re
import sys
import fnmatch

from collections import defaultdict
from datetime import datetime
from gppylib import gplog, gpversion
from gppylib.commands.base import Command, ExecutionError, WorkerPool, REMOTE
from gppylib.commands.gp import GpStop, GpStart, GpVersion
from gppylib.commands.unix import findCmdInPath
from gppylib.db import dbconn
from gppylib.gparray import GpArray
from gppylib.operations import Operation
from gppylib.operations.utils import RemoteOperation, ParallelOperation
from gppylib.userinput import ask_yesno

logger = gplog.get_default_logger()

DEFAULT_BATCH_SIZE = 16
SYSTEM_FSOID = 3052
DEFAULT_DATABASE = 'postgres'
DEFAULT_BACKUP_DIR_PREFIX = 'pt_rebuild_bk_'
PGPORT=os.environ.get('PGPORT', '5432')
TIMESTAMP = datetime.now().strftime("%Y%m%d%H%M%S")
BACKUP_RESTORE_LOG = os.path.join('/tmp', 'pt_bkup_restore_' + TIMESTAMP + '.log')
TRANSACTION_LOG_DIRS = ['pg_clog', 'pg_xlog', 'pg_distributedlog', 'pg_distributedxidmap', 'pg_changetracking']
NON_EMPTY_TRANSACTION_LOG_DIRS = ['pg_clog', 'pg_xlog', 'pg_distributedlog']
GLOBAL_PERSISTENT_FILES = defaultdict(defaultdict) # {segment: {dbid1: [file1, file2], dbid2: [file3, file4]}}
PER_DATABASE_PERSISTENT_FILES = defaultdict(defaultdict)  # {dbid1:{dboid1:[list],dboid2:[list]},
                                                          #  dbid2:{dboid1:[list],dboid2:[list]}},
                                                          # 'list' containing the list of
                                                          # relfilenode ids of the perdb pt files (5094, 5095)

class ValidateContentID:
    '''
    Parses the content_id or the contentid_file passed in by the user and
    validates the content ids.
    '''
    def __init__(self, content_id, contentid_file, gparray):
        self.content_id = content_id
        self.contentid_file = contentid_file
        self.gparray = gparray

    def _parse_content_id(self):
        '''
        Parse the content id passed in by the user and
        return it as a list of integers
        '''
        try:
            self.content_id = self.content_id.strip().split(',')
            self.content_id = map(int, self.content_id)
        except Exception as e:
            logger.error('Please correct the input data and try again')
            raise Exception('Some content ids are not integers: %s.' % self.content_id)

        return self.content_id

    def _validate_contentid_file(self):
        '''
        Validate the file for invalid integers
        Skips blank lines
        '''
        if not os.path.isfile(self.contentid_file):
            raise Exception('Unable to find contentid file "%s"' % self.contentid_file)

        self.content_id = []

        with open(self.contentid_file) as fp:
            for line in fp:
                line = line.strip()
                if line:
                    try:
                        line = int(line)
                    except Exception as e:
                        logger.error('File should contain integer content ids one per line. Please correct the input data and try again')
                        raise Exception('Found non integer content id "%s" in contentid file "%s"' % (line, self.contentid_file))
                    self.content_id.append(line)

        if not self.content_id:
            raise Exception('Please make sure there is atleast one integer content ID in the file')

        return self.content_id

    def _validate_content_id(self):
        """
        Validates that the content ids are valid contents by checking
        gp_segment_configuration and that atleast the primary is up.
        Returns error if content id is not present in gp_segment_configuration
        or both primary and mirror are down
        """

        """Collect a list of all content ids that are in the gp_segment_configuration"""
        valid_content_ids = set([seg.getSegmentContentId() for seg in self.gparray.getDbList()])
        invalid_content_ids = set()

        for c in self.content_id:
            if c not in valid_content_ids:
                invalid_content_ids.add(c)

        if len(invalid_content_ids) > 0:
            raise Exception('The following content ids are not present in gp_segment_configuration: %s' % ', '.join(map(str, invalid_content_ids)))

        """
            Collect a list of all segments where either primary or mirror is up. Whichever segment is up
            will be acting as primary. If we don't find even one segment that is up for a particular
            content, then it mean both primary and mirror are down. Hence down_content_ids will only
            be populated if both the primary and mirror are down and not if only one of them is down.
        """
        up_content_ids = set()
        resync_content_ids = set()
        resync_content_ids_to_rebuild = set()
        resync_content_ids_notto_rebuild = set()

        for seg in self.gparray.getDbList():
            status = seg.getSegmentStatus()
            mode = seg.getSegmentMode()
            c = seg.getSegmentContentId()
            if status == 'u':
                up_content_ids.add(c)
            if mode == 'r' and status == 'u':
                resync_content_ids.add(c)
        for c in self.content_id:
            if c in resync_content_ids:
                resync_content_ids_to_rebuild.add(c)

        resync_content_ids_notto_rebuild = resync_content_ids.difference(resync_content_ids_to_rebuild)

        if len(resync_content_ids_to_rebuild) > 0:
            raise Exception('Can not rebuild persistent tables for content ids that are in resync mode: %s' % ', '.join(map(str, resync_content_ids_to_rebuild)))
        if len(resync_content_ids_notto_rebuild) > 0:
            warning_msgs=  ['****************************************************', 'There are some other content ids which are in resync.',
                            'Start rebuild process may change their current status.', '****************************************************']
            for warning_msg in warning_msgs:
                logger.warning(warning_msg)
            input = ask_yesno(None, 'Do you still wish to continue ?', 'N')
            if not input:
                raise Exception('Aborting rebuild due to user request')

        return self.content_id

    def validate(self):
        """
        We make sure that the user does not pass in both content_id and
        contentid_file in gppersistentrebuild and that alteast one of
        them is definitely passed in. All the content-ids are populated
        in self.content_id and that is validated against gp_segment_configuration
        """
        if self.content_id:
            self._parse_content_id()
        elif self.contentid_file:
            self._validate_contentid_file()
        return self._validate_content_id()

class DbIdInfo:
    """
    Stores all the information regarding a single dbid
    """
    def __init__(self, content, role, dbid, port, hostname, filespace_dirs, fs_to_ts_map, ts_to_dboid_map, is_down):
        self.content = content
        self.role = role
        self.dbid = dbid
        self.port = port
        self.hostname = hostname
        self.filespace_dirs = filespace_dirs
        self.fs_to_ts_map = fs_to_ts_map
        self.ts_to_dboid_map = ts_to_dboid_map
        self.is_down = is_down

    def __eq__(self, other):
        return  vars(self) == vars(other)

    def __str__(self):
        return '%s:%s:%s:%s:%s:%s:%s:%s:%s' % (self.content, self.role, self.dbid, self.port, self.hostname, self.filespace_dirs,
                                         self.fs_to_ts_map, self.ts_to_dboid_map, self.is_down)

class GetDbIdInfo:
    """
    Query the catalog tables to get information about
    filespaces/tablespaces and persistent table files
    for each dbid
    """
    def __init__(self, gparray, content_id):
        self.gparray = gparray
        self.content_id = content_id

    def _get_filespace_to_tablespace_map(self, segdb):
        '''
        Get a map of filespace oids to tablespace oids for a given segdb
        The key is a single integer representing the oid of a filespace
        The value is a list of oids which represent the oids of
        tablespaces
        '''
        fs_to_ts_map = {}
        fs_oids = segdb.getSegmentFilespaces().keys()
        FILESPACE_TO_TABLESPACE_MAP_QUERY = """SELECT spcfsoid, string_agg(oid, ' ')
                                               FROM pg_tablespace
                                               WHERE spcfsoid IN (%s) GROUP BY spcfsoid""" % ', '.join(map(str, fs_oids))
        with dbconn.connect(dbconn.DbURL(dbname=DEFAULT_DATABASE)) as conn:
            res = dbconn.execSQL(conn, FILESPACE_TO_TABLESPACE_MAP_QUERY)
            for r in res:
                fs_to_ts_map[r[0]] = [int(x) for x in r[1].split()]
        return fs_to_ts_map

    def _get_tablespace_to_dboid_map(self, ts_oids):
        '''
        Get a map of tablespaces oids to database oids for a given tablespace
        The key is a single integer representing the oid of a tbalespace
        The value is a list of oids which represent the oids of databases
        '''
        ts_to_dboid_map = {}
        TABLESPACE_TO_DBOID_MAP_QUERY = """SELECT dattablespace, string_agg(oid, ' ')
                                           FROM pg_database
                                           WHERE dattablespace IN (%s) GROUP BY dattablespace""" % ', '.join(map(str, ts_oids))
        with dbconn.connect(dbconn.DbURL(dbname=DEFAULT_DATABASE)) as conn:
            res = dbconn.execSQL(conn, TABLESPACE_TO_DBOID_MAP_QUERY)
            for r in res:
                ts_to_dboid_map[r[0]] = [int(x) for x in r[1].split()]
        return ts_to_dboid_map

    def get_info(self):
        '''
        This method gets the information for all the segdbs where we want to rebuild the
        persistent tables.
        It returns a list of DbIdInfo objects
        '''
        dbid_info = []

        for seg in self.gparray.getDbList():
            if seg.getSegmentContentId() in self.content_id:
                is_down = seg.isSegmentDown()
                role = seg.getSegmentRole()

                # We don't want to run the rebuild on the segments that
                # are down. This can cause issues, especially when the segment
                # in question has missing data/files.
                if is_down and role == 'm':
                    continue

                fs_to_ts_map = self._get_filespace_to_tablespace_map(seg)
                ts_oids = []
                for fsoid, ts in fs_to_ts_map.items():
                    ts_oids += ts
                ts_to_dboid_map = self._get_tablespace_to_dboid_map(ts_oids)
                di = DbIdInfo(content=seg.getSegmentContentId(),
                              role=seg.getSegmentRole(),
                              dbid=seg.getSegmentDbId(),
                              port=seg.getSegmentPort(),
                              hostname=seg.getSegmentHostName(),
                              filespace_dirs=seg.getSegmentFilespaces(),
                              fs_to_ts_map=fs_to_ts_map,
                              ts_to_dboid_map=ts_to_dboid_map,
                              is_down=is_down)
                dbid_info.append(di)

        return dbid_info

class ValidateMD5Sum:
    """Checks the md5 sum for a list of files"""
    def __init__(self, pool, batch_size=DEFAULT_BATCH_SIZE):
        self.batch_size = batch_size
        self.md5_prog = None
        self.md5_results_pat = None
        self.pool = pool

    def _get_md5_prog(self):
        """Get the appropriate md5 program for the platform"""
        md5_prog = ''
        operating_sys = platform.system()
        if operating_sys == 'Darwin':
            md5_prog = 'md5'
        elif operating_sys == 'Linux':
            md5_prog = 'md5sum'
        else:
            raise Exception('Cannot determine the md5 program since %s platform is not supported' % operating_sys)
        return md5_prog

    def _get_md5_results_pat(self):
        """
        We want to parse the results of the md5progs in order to extract the filename
        and its correspoding md5sum.
        On OSX, the md5 program will return output in the following format
        MD5 (<filename>) = <md5_hash>
        On Linux, the md5 program will return output in the following format
        <md5_hash> <filename>
        Hence this returns an re.pattern object so that we can extract the required
        information
        """
        operating_sys = platform.system()
        if operating_sys == 'Darwin':
            md5_pat = re.compile('MD5 \((.*)\) = (.*)')
        elif operating_sys == 'Linux':
            md5_pat = re.compile('(.*) (.*)')
        else:
            raise Exception('Cannot determine the pattern for results of md5 program since %s platform is not supported' % operating_sys)
        return md5_pat

    def init(self):
        """
        Initialize the class with the md5 program and the pattern
        based on the platform
        Ideally this should be called once per run of the program
        in order to be efficient. It is the callers reponsibilty
        to ensure that.
        """
        self.md5_prog = self._get_md5_prog()
        self.md5_results_pat = self._get_md5_results_pat()

    def _process_md5_results(self):
        """
        Returns a dictionary with the key as the filename
        and the value as the md5 hash value
        If there was any error, it raises an Exception
        """
        md5s = {}
        for item in self.pool.getCompletedItems():
            result = item.get_results()
            if not result.wasSuccessful():
                raise Exception('Unable to calculate md5sum for: %s' % (result.stderr.strip()))

            md5_results = result.stdout.strip().split('\n')
            for md5_result in md5_results:
                mat = self.md5_results_pat.match(md5_result.strip())
                if mat:
                    if platform.system() == 'Linux':
                        f, md5 = mat.group(2), mat.group(1)
                    else:
                        f, md5 = mat.group(1), mat.group(2)
                    md5s[f.strip()] = md5.strip()
        return md5s

    def validate(self, src_files):
        """Run the md5 program and calculate the md5sum for the src_files"""
        for f in src_files:
            cmd = Command('calculate md5sum for file', cmdStr='%s %s' % (self.md5_prog, f))
            self.pool.addCommand(cmd)
        self.pool.join()
        return self._process_md5_results()

class BackupPersistentTableFiles:
    """
    Backup all the global and the per database persistent table files
    """
    def __init__(self, dbid_info, timestamp, perdb_pt_filenames, global_pt_filenames, batch_size=DEFAULT_BATCH_SIZE, backup_dir=None, validate_only=False):
        self.dbid_info = dbid_info
        self.timestamp = timestamp
        self.batch_size = batch_size
        self.backup_dir = backup_dir
        self.md5_validator = None
        self.pool = None
        self.GLOBAL_PERSISTENT_FILES = global_pt_filenames
        self.PER_DATABASE_PERSISTENT_FILES = perdb_pt_filenames
        self.validate_only = validate_only

    def _cleanup_pool(self):
        if self.pool:
            self.pool.haltWork()
            self.pool.joinWorkers()
            self.pool = None

    def _copy_files(self, src_files, dest_files, dbid, actionType):
        """
        This actually does the copy of the files from src directory to backup directory
        In case of backup, the destination folder might not exist. Hence we create it.
        While restoring it, we always restore to datadirectory and it should be present,
        hence we do not bother to create it.
        """
        src_md5 = self.md5_validator.validate(src_files)

        with open(BACKUP_RESTORE_LOG, 'a') as fw:
            fw.write('************************************************\n')
            fw.write('DBID =  %s\t%s\n' % (dbid, actionType))
            for i in range(len(src_files)):
                dest_dir = os.path.dirname(dest_files[i])
                try:
                    if not os.path.isdir(dest_dir):
                        os.makedirs(dest_dir)
                except Exception, e:
                    raise Exception("Failed to create destination directory %s\n" % str(e))
                fw.write('%s    =>     %s\n' % (src_files[i], dest_files[i]))
                cmd = Command('copy files', cmdStr='cp %s %s' % (src_files[i], dest_files[i]))
                self.pool.addCommand(cmd)

        self.pool.join()
        self.pool.check_results()

        dest_md5 = self.md5_validator.validate(dest_files)

        self.md5_validate(src_files, dest_files, src_md5, dest_md5)

    def md5_validate(self, src_files, dest_files, src_md5, dest_md5):
        """
        This is to verify that src files are matching dest files based on their md5 hash code
        """
        unmatched_expected_src_md5 = {}
        unmatched_actual_dest_md5 = {}
        for i in range(len(src_files)):
            src_file = src_files[i]
            dest_file = dest_files[i]
            if src_md5[src_file] != dest_md5[dest_file]:
                unmatched_expected_src_md5[src_file] = src_md5[src_file]
                unmatched_actual_dest_md5[dest_file] = dest_md5[dest_file]

        if unmatched_expected_src_md5:
            raise Exception('MD5 sums do not match! Expected md5 = "%s", but actual md5 = "%s"' %
                           (unmatched_expected_src_md5, unmatched_actual_dest_md5))

    def build_PT_src_dest_pairs(self, src_dir, dest_dir, file_list):
        """
        src_dir: source directory to copy pt files from
        dest_dir: destination directory to backup pt files
        file_list: list of pt files to backup
        return: list of source files and destination files, if missing
                any source files, return None, None
        """

        if file_list is None or len(file_list) == 0:
            logger.error('Persistent source file list is empty or none')
            return None, None

        if not os.path.isdir(src_dir) or len(os.listdir(src_dir)) == 0:
            logger.error('Directory %s either does not exist or is empty' % src_dir)
            return None, None

        missed_files = []
        src_files, dest_files = [], []
        for f in file_list:
            file = os.path.join(src_dir, f)
            if not os.path.isfile(file):
                missed_files.append(file)
            else:
                src_files.append(file)
                dest_files.append(os.path.join(dest_dir, f))

                # large heap tables splited into parts as [table, table.1, table.2,...]
                for relfilenode in os.listdir(src_dir):
                    if fnmatch.fnmatch(relfilenode, f + ".*"):
                        src_files.append(os.path.join(src_dir, relfilenode))
                        dest_files.append(os.path.join(dest_dir, relfilenode))

        if len(missed_files) > 0:
            logger.error('Missing source files %s' % missed_files)
            return None, None

        return src_files, dest_files

    def build_Xactlog_src_dest_pairs(self, srcDir, destDir):
        """
        srcDir:  absolute path to source data directory
        destDir: absolute path to destination data directory
        srcFiles: list of absolute paths to source files
        destFiles: list of absolute paths to destination files

        This funtion takes whatever under srcDir, and put it under destDir:
        eg: srcDir:   pg_clog_bk_20150420234453/
            srcFile:  /data/gpdb/master/gpseg-1/pg_clog_bk_20150420234453/0000
            destDir:  pg_clog/
            destFile: /data/gpdb/master/gpseg-1/pg_clog/0000
        """
        srcFiles, destFiles = [], []
        for dirName, _, fileList in os.walk(srcDir):
            relDir = os.path.relpath(dirName, srcDir)
            for f in fileList:
                relFile = os.path.join(relDir, f)
                srcFile = os.path.join(dirName, f)
                destFile = os.path.join(destDir, relFile)
                srcFiles.append(srcFile)
                destFiles.append(destFile)
        return srcFiles, destFiles

    def _copy_global_pt_files(self, restore=False):
        """
        Copies the global persistent table files to required directory
        If there is any error, we add it to the list of failures and return
        the list
        """
        if restore:
            op = 'Restore of global persistent files'
        else:
            op = 'Backup of global persistent files'

        logger.info(op)

        failures = []
        for di in self.dbid_info:
            #Find out the system filespace
            fs_dir = di.filespace_dirs[SYSTEM_FSOID].rstrip(os.sep)

            data_dir = os.path.join(fs_dir, 'global')
            if self.backup_dir:
                bk_dir = os.path.join(self.backup_dir,
                                        '%s%s' % (DEFAULT_BACKUP_DIR_PREFIX, self.timestamp),
                                        str(di.dbid),
                                        os.path.basename(fs_dir),
                                        'global')
            else:
                bk_dir = os.path.join(fs_dir,
                                        '%s%s' % (DEFAULT_BACKUP_DIR_PREFIX, self.timestamp),
                                        'global')

            file_list = self.GLOBAL_PERSISTENT_FILES[di.dbid]

            if not restore:
                src_files, dest_files = self.build_PT_src_dest_pairs(data_dir, bk_dir, file_list)
            else:
                src_files, dest_files = self.build_PT_src_dest_pairs(bk_dir, data_dir, file_list)

            if src_files is None or len(src_files) == 0:
                raise Exception('Missing global persistent files from source directory.')

            logger.debug('DBID =  %s' % di.dbid)
            logger.debug('Source files = %s' % src_files)
            logger.debug('Destination files = %s' % dest_files)

            try:
                if not self.validate_only:
                    self._copy_files(src_files, dest_files, di.dbid, op)
            except Exception as e:
                failures.append((di, str(e)))
                logger.error('%s failed' % op)
                logger.error('DBID =  %s' % di.dbid)
                logger.error('%s' % str(e))

        if failures:
            raise Exception('%s failed' % op)

    def _copy_per_db_pt_files(self, restore=False):
        """
        Copies the per database persistent table files to required directory
        If there is any error, we add it to the list of failures and return
        the list
        """
        if restore:
            op = 'Restore of per database persistent files'
        else:
            op = 'Backup of per database persistent files'

        logger.info(op)

        failures = []
        for di in self.dbid_info:

            # visit each filespace
            sys_filespace = di.filespace_dirs[SYSTEM_FSOID].rstrip(os.sep)

            for fsoid, fsdir in di.filespace_dirs.items():
                if fsoid not in di.fs_to_ts_map:
                    continue

                fsdir = fsdir.rstrip(os.sep)

                # visit each tablespace
                for tsoid in di.fs_to_ts_map[fsoid]:
                    if tsoid not in di.ts_to_dboid_map:
                        continue

                    # visit each database
                    for dboid in di.ts_to_dboid_map[tsoid]:
                        if fsoid != SYSTEM_FSOID:
                            base_dir = tsoid
                        else:
                            base_dir = 'base'

                        if dboid in self.PER_DATABASE_PERSISTENT_FILES[di.dbid]:
                            file_list = self.PER_DATABASE_PERSISTENT_FILES[di.dbid][dboid]
                        else:
                            #This corresponds to template0. We cannot connect to template0
                            #hence the relfilenodeid will never change.
                            file_list = ['5094', '5095']

                        #finding out persistent table files for the database
                        data_dir = os.path.join(fsdir, str(base_dir), str(dboid))
                        if self.backup_dir:
                            bk_dir = os.path.join(self.backup_dir,
                                                    '%s%s' % (DEFAULT_BACKUP_DIR_PREFIX, self.timestamp),
                                                    str(di.dbid),
                                                    (os.path.basename(fsdir) if fsoid == SYSTEM_FSOID
                                                                            else os.path.relpath(fsdir, os.path.dirname(sys_filespace))),
                                                    str(base_dir),
                                                    str(dboid))
                        else:
                            bk_dir = os.path.join(fsdir,
                                                    '%s%s' % (DEFAULT_BACKUP_DIR_PREFIX, self.timestamp),
                                                    str(base_dir),
                                                    str(dboid))

                        logger.debug('Work on database location = %s' % data_dir)
                        logger.debug('Search listed persistent tables %s' % file_list)

                        if not restore:
                            src_files, dest_files = self.build_PT_src_dest_pairs(data_dir, bk_dir, file_list)
                        else:
                            src_files, dest_files = self.build_PT_src_dest_pairs(bk_dir, data_dir, file_list)

                        if src_files is None or len(src_files) == 0:
                            raise Exception('Missing per-database persistent files from source directory.')

                        logger.debug('Source files = %s' % src_files)
                        logger.debug('Destination files = %s' % dest_files)

                        try:
                            if not self.validate_only:
                                self._copy_files(src_files, dest_files, di.dbid, op)
                        except Exception as e:
                            failures.append((di, str(e)))
                            logger.error('%s failed' % op)
                            logger.error('DBID =  %s' % di.dbid)
                            logger.error('Filespace location = %s' % di.filespace_dirs[SYSTEM_FSOID])
                            logger.error('%s' % str(e))

        if failures:
            raise Exception(' %s failed' % op)

    def _copy_Xactlog_files(self, restore=False):
        """
        All transaction log directories to back up or restore:
        pg_clog: keeps track of the transaction id
        pg_xog:  keeps track of the checkpoint and WAL log
        pg_distributedxidmap:  Back up as maintains mapping from distributed to local xid
        pg_distributedlog:  Back up as maintains distributed transaction commit status
        Add error to the list of failures and return the list
        """
        if restore:
            op = 'Restore of transaction log files'
        else:
            op = 'Backup of transaction log files'

        logger.info(op)

        failures = 0
        for di in self.dbid_info:
            datadir = di.filespace_dirs[SYSTEM_FSOID].rstrip(os.sep)
            allSrcFiles, allDestFiles = [], []
            for xlog_dir_name in TRANSACTION_LOG_DIRS:
                xlog_dir = os.path.join(datadir, xlog_dir_name)
                if self.backup_dir:
                    xlog_backup_dir = os.path.join(self.backup_dir,
                                                   '%s%s' % (DEFAULT_BACKUP_DIR_PREFIX, self.timestamp),
                                                   str(di.dbid),
                                                   os.path.basename(datadir),
                                                   xlog_dir_name)
                else:
                    xlog_backup_dir = os.path.join(datadir,
                                                   '%s%s' % (DEFAULT_BACKUP_DIR_PREFIX, self.timestamp),
                                                   xlog_dir_name)
                if restore:
                    srcFiles, destFiles = self.build_Xactlog_src_dest_pairs(xlog_backup_dir, xlog_dir)
                else:
                    srcFiles, destFiles = self.build_Xactlog_src_dest_pairs(xlog_dir, xlog_backup_dir)

                if xlog_dir_name in NON_EMPTY_TRANSACTION_LOG_DIRS and len(srcFiles) == 0:
                    raise Exception('Source directory %s should not be empty' % (xlog_backup_dir if restore else xlog_dir))

                allSrcFiles.extend(srcFiles)
                allDestFiles.extend(destFiles)

            logger.debug('DBID =  %s' % di.dbid)
            logger.debug('Source files = %s' % allSrcFiles)
            logger.debug('Destination files = %s' % allDestFiles)

            try:
                if not self.validate_only:
                    self._copy_files(allSrcFiles, allDestFiles, di.dbid, op)
            except Exception as e:
                failures += 1
                logger.error('%s failed' % op)
                logger.error('DBID =  %s' % di.dbid)
                logger.error('%s' % str(e))

        if failures > 0:
            raise Exception(' %s failed' % op)

    def _copy_pg_control_file(self, restore=False, validate=False):
        """
        Backing up or restoring the pg_control file which determines
        checkpoint location in the transaction log file when running
        a database recovery
        """
        if restore:
            op = 'Restore of global pg_control file'
        else:
            op = 'Backup of global pg_control file'

        logger.info(op)

        failures = 0
        for di in self.dbid_info:
            datadir = di.filespace_dirs[SYSTEM_FSOID].rstrip(os.sep)
            pg_control_path = os.path.join(datadir, 'global', 'pg_control')

            if self.backup_dir:
               pg_control_backup_path = os.path.join(self.backup_dir,
                                                     '%s%s' % (DEFAULT_BACKUP_DIR_PREFIX, self.timestamp),
                                                     str(di.dbid),
                                                     os.path.basename(datadir),
                                                     'global',
                                                     'pg_control')
            else:
                pg_control_backup_path = os.path.join(datadir,
                                                      '%s%s' % (DEFAULT_BACKUP_DIR_PREFIX, self.timestamp),
                                                      'global',
                                                      'pg_control')
            srcFiles, destFiles = [], []
            if restore:
                if not os.path.isfile(pg_control_backup_path):
                    raise Exception('Global pg_control file is missing from backup directory %s' % pg_control_backup_path)
                srcFiles.append(pg_control_backup_path)
                destFiles.append(pg_control_path)
            else:
                if not os.path.isfile(pg_control_path):
                    raise Exception('Global pg_control file is missing from source directory %s' % pg_control_path)
                srcFiles.append(pg_control_path)
                destFiles.append(pg_control_backup_path)

            logger.debug('DBID =  %s' % di.dbid)
            logger.debug('Source files = %s' % srcFiles)
            logger.debug('Destination files = %s' % destFiles)

            try:
                if not self.validate_only:
                    self._copy_files(srcFiles, destFiles, di.dbid, op)
            except Exception as e:
                failures += 1
                logger.error('%s failed' % op)
                logger.error('DBID =  %s' % di.dbid)
                logger.error('%s' % str(e))

        if failures > 0:
            raise Exception(' %s failed' % op)

    def backup(self):
        try:
            self.pool = WorkerPool(self.batch_size)
            self.md5_validator = ValidateMD5Sum(self.pool, self.batch_size)
            self.md5_validator.init()

            self._copy_global_pt_files()
            self._copy_per_db_pt_files()
            self._copy_Xactlog_files()
            self._copy_pg_control_file()
        finally:
            self._cleanup_pool()

    def restore(self):
        try:
            self.pool = WorkerPool(self.batch_size)
            self.md5_validator = ValidateMD5Sum(self.pool, self.batch_size)
            self.md5_validator.init()

            self._copy_global_pt_files(restore=True)
            self._copy_per_db_pt_files(restore=True)
            self._copy_Xactlog_files(restore=True)
            self._copy_pg_control_file(restore=True)
        finally:
            self._cleanup_pool()


class RebuildTableOperation(Operation):
    """
    Run the sql functions to rebuild the persistent tables
    """
    def __init__(self, dbid_info, has_mirrors):
        self.dbid_info = dbid_info
        self.has_mirrors = has_mirrors

    def run(self):
        port = self.dbid_info.port
        hostname = self.dbid_info.hostname
        filerep_mirror = 'true' if self.has_mirrors else 'false'

        with dbconn.connect(dbconn.DbURL(dbname=DEFAULT_DATABASE, port=port), utility=True) as conn:
            dbconn.execSQL(conn, 'CHECKPOINT')
            conn.commit()
            logger.info('Finished checkpoint on %s' % self.dbid_info.filespace_dirs[SYSTEM_FSOID])
            dbconn.execSQL(conn, 'SELECT gp_persistent_reset_all()')
            conn.commit()
            logger.info('Completed gp_persistent_reset_all() on %s' % self.dbid_info.filespace_dirs[SYSTEM_FSOID])
            dbconn.execSQL(conn, 'SELECT gp_persistent_build_all(%s)' % filerep_mirror)
            conn.commit()
            logger.info('Completed gp_persistent_build_all() on %s' % self.dbid_info.filespace_dirs[SYSTEM_FSOID])
            dbconn.execSQL(conn, 'CHECKPOINT')
            conn.commit()
            logger.info('Finished checkpoint on %s' % self.dbid_info.filespace_dirs[SYSTEM_FSOID])

class ValidatePersistentBackup:
    """
    Validate that the backup for persistent table files
    are present before we acutally do the rebuild
    """
    def __init__(self, dbid_info, timestamp, batch_size=DEFAULT_BATCH_SIZE, backup_dir=None):
        self.dbid_info = dbid_info
        self.timestamp = timestamp
        self.batch_size = batch_size
        self.backup_dir = backup_dir
        self.pool = None

    def _cleanup_pool(self):
        if self.pool:
            self.pool.haltWork()
            self.pool.joinWorkers()
            self.pool = None

    def _process_results(self, di, pool):
        err = False
        datadir = di.filespace_dirs[SYSTEM_FSOID]

        for item in pool.getCompletedItems():
            results = item.get_results()

            for fsoid, fsdir in di.filespace_dirs.iteritems():
                if fsdir in item.cmdStr:
                    datadir = fsdir
                    break

            if not results.wasSuccessful():
                err = True
                logger.error('marking failure for content id %s:%s since it was not successful: %s' % (di.content, datadir, results))
            elif not results.stdout.strip():
                err = True
                logger.error('marking failure for content id %s:%s since backup was not found: %s' % (di.content, datadir, results))
        if err:
            raise Exception('Failed to validate backups')

    def validate_backups(self):
        try:
            self.pool = WorkerPool(self.batch_size)

            for di in self.dbid_info:
                for fsoid, fsdir in di.filespace_dirs.items():
                    if fsoid not in di.fs_to_ts_map:
                        continue

                    is_filespace_empty = True
                    for tsoid in di.fs_to_ts_map[fsoid]:
                        if tsoid in di.ts_to_dboid_map and di.ts_to_dboid_map[tsoid]:
                            is_filespace_empty = False
                            break

                    if not is_filespace_empty:
                        cmd = Command('Check if pt backup exists', cmdStr='find %s -name %s%s' %
                                     (self.backup_dir if self.backup_dir else fsdir, DEFAULT_BACKUP_DIR_PREFIX, self.timestamp))
                        self.pool.addCommand(cmd)

                self.pool.join()
                self._process_results(di, self.pool)
        finally:
            self._cleanup_pool()

    def validate_backup_dir(self):
        """
        Validate that at least root directory of backup_dir exists, if full path of backup_dir
        does not exist, then the permission should be allowed to create the full path.
        If backup_dir is under any of segment or master data directory, error out
        """
        root_backup_dir = os.path.join(os.sep, self.backup_dir.split(os.sep)[1])
        if not os.path.exists(root_backup_dir):
            raise Exception('Root backup directory is not valid, %s' % root_backup_dir)
        logger.debug('Root backup directory is valid, %s' % root_backup_dir)

        logger.debug('Validating read and write permission of backup directory %s' % self.backup_dir)
        paths = self.backup_dir.split(os.sep)
        for i in xrange(len(paths) - 1, 0, -1):
            parentpath = os.path.join(os.sep, os.sep.join(paths[1:i+1]))
            if os.path.exists(parentpath) and os.access(parentpath, os.R_OK | os.W_OK):
                logger.debug('Permission is allowed for backup directory under %s' % parentpath)
                break
            elif os.path.exists(parentpath) and not os.access(parentpath, os.R_OK | os.W_OK):
                raise Exception('Permission not allowed for Backup directory under path %s' % parentpath)

        logger.debug('Validating the backup directory is not under any segment data directory')
        for di in self.dbid_info:
            for _, fsdir in di.filespace_dirs.items():
                relpath = os.path.relpath(self.backup_dir, fsdir)
                if not relpath.startswith('..'):
                    raise Exception('Backup directory is a sub directory of segment data directory %s' % fsdir)
                else:
                    logger.debug('Backup directory %s is not under %s' % (self.backup_dir, fsdir))

        logger.info('Backup directory validated and good')

class RebuildTable:
    """
    This class performs the following final checks before starting the rebuild process
    1. Check if the backup is present on the segment. In case a segment went down,
       the mirror would take over and the backup might not be present on the mirror.
       Hence we do this check.
    2. Check if there are any contents that are down i.e Both primary and mirror are down.
    In either of the above two cases we chose to fail so that the user can fix the issue
    and rerun the tool.
    """
    def __init__(self, dbid_info, has_mirrors=False, batch_size=DEFAULT_BATCH_SIZE, backup_dir=None):
        self.gparray = None
        self.dbid_info = dbid_info
        self.has_mirrors = has_mirrors
        self.batch_size = batch_size
        self.backup_dir = backup_dir
        self.pool = None

    def _cleanup_pool(self):
        if self.pool:
            self.pool.haltWork()
            self.pool.joinWorkers()
            self.pool = None

    def _get_valid_dbids(self, content_ids):
        """Check to see if a content is down. i.e both primary and mirror"""
        valid_dbids = []
        for seg in self.gparray.getDbList():
            if seg.getSegmentContentId() in content_ids:
                if seg.getSegmentRole() == 'p' and seg.getSegmentStatus() == 'd':
                    raise Exception('Segment %s is down. Cannot continue with persistent table rebuild' % seg.getSegmentDataDirectory())
                elif seg.getSegmentRole() == 'p' and seg.getSegmentMode() == 'r':
                    raise Exception('Segment %s is in resync. Cannot continue with persistent table rebuild' % seg.getSegmentDataDirectory())
                elif seg.getSegmentRole() == 'p' and seg.getSegmentStatus() != 'd':
                    valid_dbids.append(seg.getSegmentDbId())

        return valid_dbids

    def _validate_backups(self):
        RunBackupRestore(self.dbid_info, TIMESTAMP, self.batch_size, self.backup_dir).validate_backups()

    def rebuild(self):
        """
        If any of the validations fail, we chose to error out.
        Otherwise we try to rebuild the persistent tables and return
        a list of successful dbidinfo and failed dbidinfo
        """
        self.gparray = GpArray.initFromCatalog(dbconn.DbURL(dbname=DEFAULT_DATABASE), utility=True)

        logger.info('Validating backups')
        self._validate_backups()

        logger.info('Validating dbids')
        content_ids = set([di.content for di in self.dbid_info])
        valid_dbids = self._get_valid_dbids(content_ids)
        valid_dbid_info = [di for di in self.dbid_info if di.dbid in valid_dbids]
        successes, failures = [], []
        rebuild_done = {}
        operation_list = []

        logger.info('Starting persistent table rebuild operation')
        for di in valid_dbid_info:
            if di.content != -1:
                operation_list.append(RemoteOperation(RebuildTableOperation(di, self.has_mirrors), di.hostname))
            else:
                operation_list.append(RemoteOperation(RebuildTableOperation(di, False), di.hostname))

        try:
            ParallelOperation(operation_list, self.batch_size).run()
        except Exception as e:
            pass

        for op in operation_list:
            di = op.operation.dbid_info
            try:
                op.get_ret()
            except Exception as e:
                logger.debug('Table rebuild failed for content %s:%s ' % (di.content, str(e)))
                failures.append((di, str(e)))
            else:
                successes.append(di)
        return successes, failures

class RunBackupRestore:
    """
    This is a wrapper class to invoke $GPHOME/sbin/gppersistent_backup.py
    """
    def __init__(self, dbid_info, timestamp, batch_size=DEFAULT_BATCH_SIZE, backup_dir=None, validate_only=False):
        self.dbid_info = dbid_info
        self.timestamp = timestamp
        self.batch_size = batch_size
        self.backup_dir = backup_dir
        self.pool = None
        self.validate_source_files_only = '--validate-source-file-only' if validate_only else ''

    def _get_host_to_dbid_info_map(self):
        host_to_dbid_info_map = defaultdict(list)
        for di in self.dbid_info:
            host_to_dbid_info_map[di.hostname].append(di)
        return host_to_dbid_info_map

    def _process_results(self, pool, err_msg):
        err = False
        for completed_item in pool.getCompletedItems():
            res = completed_item.get_results()
            if res.rc != 0:
                err = True
                logger.error('************************************************')
                logger.error('%s' % res.stdout.strip())
                logger.error('************************************************')

        if err:
            raise Exception(err_msg)

    def _run_backup_restore(self, host_to_dbid_info_map, restore=False, validate_backups=False, validate_backup_dir=False):
        self.pool = WorkerPool(self.batch_size)
        if restore:
            option = '--restore'
            err_msg = 'Restore of persistent table files failed'
        elif validate_backups:
            option = '--validate-backups'
            err_msg = 'Validate of backups of persistent table files failed'
        elif validate_backup_dir:
            option = '--validate-backup-dir'
            err_msg = 'Validate backup directory of persistent table files failed'
        else:
            option = '--backup'
            err_msg = 'Backup of persistent table files failed'

        verbose_logging = '--verbose' if gplog.logging_is_verbose() else ''

        try:
            for host in host_to_dbid_info_map:
                pickled_backup_dbid_info = base64.urlsafe_b64encode(pickle.dumps(host_to_dbid_info_map[host]))
                pickled_per_database_persistent_files = base64.urlsafe_b64encode(pickle.dumps(PER_DATABASE_PERSISTENT_FILES[host]))
                pickled_global_persistent_files = base64.urlsafe_b64encode(pickle.dumps(GLOBAL_PERSISTENT_FILES[host]))
                if self.backup_dir:
                    cmdStr = """$GPHOME/sbin/gppersistent_backup.py --timestamp %s %s %s --batch-size %s --backup-dir %s --perdbpt %s --globalpt %s %s %s""" %\
                            (self.timestamp, option, pickled_backup_dbid_info, self.batch_size, self.backup_dir, pickled_per_database_persistent_files,
                             pickled_global_persistent_files, self.validate_source_files_only, verbose_logging)
                else:
                    cmdStr = """$GPHOME/sbin/gppersistent_backup.py --timestamp %s %s %s --batch-size %s --perdbpt %s --globalpt %s %s %s""" %\
                            (self.timestamp, option, pickled_backup_dbid_info, self.batch_size, pickled_per_database_persistent_files,
                             pickled_global_persistent_files, self.validate_source_files_only, verbose_logging)

                cmd = Command('backup pt files on a host', cmdStr=cmdStr, ctxt=REMOTE, remoteHost=host)
                self.pool.addCommand(cmd)
            self.pool.join()
            self._process_results(self.pool, err_msg)
        finally:
            self.pool.haltWork()
            self.pool.joinWorkers()
            self.pool = None

    def backup(self):
        host_to_dbid_info_map = self._get_host_to_dbid_info_map()
        self._run_backup_restore(host_to_dbid_info_map)

    def restore(self):
        host_to_dbid_info_map = self._get_host_to_dbid_info_map()
        self._run_backup_restore(host_to_dbid_info_map, restore=True)

    def validate_backups(self):
        host_to_dbid_info_map = self._get_host_to_dbid_info_map()
        self._run_backup_restore(host_to_dbid_info_map, validate_backups=True)

    def validate_backup_dir(self):
        host_to_dbid_info_map = self._get_host_to_dbid_info_map()
        self._run_backup_restore(host_to_dbid_info_map, validate_backup_dir=True)

class RebuildPersistentTables(Operation):
    def __init__(self, content_id, contentid_file, backup, restore, batch_size, backup_dir):
        self.content_id = content_id
        self.contentid_file = contentid_file
        self.backup = backup
        self.restore = restore
        self.batch_size = batch_size
        self.backup_dir = backup_dir
        self.content_info = None
        self.has_mirrors = False
        self.has_standby = False
        self.gparray = None
        self.pool = None
        self.restore_state_file_location = '/tmp'
        self.gpperfmon_file = '/tmp/gpperfmon_guc'

    def _check_database_version(self):
        """
        Checks if the database version is greater than or equal to 4.1.0.0
        since the gp_persistent_reset_all and gp_persistent_build_all is
        not supported on earlier versions
        """
        if 'GPHOME' not in os.environ:
            raise Exception('GPHOME not set in the environment')
        gphome = os.environ['GPHOME']
        db_version = gpversion.GpVersion(GpVersion.local('get version', gphome))
        if db_version < gpversion.GpVersion('4.1.0.0'):
            raise Exception('This tool is not supported on Greenplum version lower than 4.1.0.0')

    def _stop_database(self):
        """
        Set the validateAfter to be False in case if there are any segments' postmaster
        process killed, cause gpstop will return non zero status code
        """
        cmd = GpStop('Stop the greenplum database', fast=True)
        cmd.run(validateAfter=False)

    def _start_database(self, admin_mode=False):
        """
        If admin_mode is set to True, it starts the database in
        admin mode. Since gpstart does not have an option to start
        in admin mode, we first start the entire database, then
        we stop the master only and restart it in utility mode
        so that it does not allow any connections
        """
        cmd = GpStart('Start the greenplum databse')
        cmd.run(validateAfter=True)
        if admin_mode:
            cmd = GpStop('Stop the greenplum database', masterOnly=True)
            cmd.run(validateAfter=True)
            cmd = GpStart('Start the greenplum master in admin mode', masterOnly=True)
            cmd.run(validateAfter=True)

    def _check_platform(self):
        """
        Solaris platform will be deprecated soon, hence we
        choose to support it only on Linux and OSX.
        """
        operating_sys = platform.system()
        if operating_sys != 'Linux' and operating_sys != 'Darwin':
            raise Exception('This tool is only supported on Linux and OSX platforms')

    def _validate_has_mirrors_and_standby(self):
        """
        Validate whether the system is configured with or without
        mirrors. This is required by gp_persistent_build_all function.
        If even a single segment does not have mirror, we consider that
        the entire system is configured without mirror. This does not
        apply to standby master
        """
        for seg in self.gparray.getDbList():
            if self.has_mirrors and self.has_standby:
                break
            elif seg.getSegmentContentId() != -1 and seg.isSegmentMirror():
                self.has_mirrors = True
            elif seg.getSegmentContentId() == -1 and seg.isSegmentStandby():
                self.has_standby = True

    def _check_md5_prog(self):
        md5prog = 'md5' if platform.system() == 'Darwin' else 'md5sum'
        if not findCmdInPath(md5prog):
            raise Exception('Unable to find %s program. Please make sure it is in PATH' % md5prog)

    def _get_persistent_table_filenames(self):
        GET_ALL_DATABASES = """select oid, datname from pg_database"""
        PER_DATABASE_PT_FILES_QUERY = """SELECT relfilenode FROM pg_class WHERE oid IN (5094, 5095)"""
        GLOBAL_PT_FILES_QUERY = """SELECT relfilenode FROM pg_class WHERE oid IN (5090, 5091, 5092, 5093)"""

        content_to_primary_dbid_host_map = dict()
        databases = defaultdict(list)
        for dbidinfo in self.dbid_info:
            if dbidinfo.role == 'm':
                continue
            hostname = dbidinfo.hostname
            port = dbidinfo.port
            dbid = dbidinfo.dbid
            content = dbidinfo.content
            globalfiles = []
            content_to_primary_dbid_host_map[content] = dbid, hostname
            with dbconn.connect(dbconn.DbURL(dbname=DEFAULT_DATABASE, hostname=hostname, port=port), utility=True) as conn:
                res = dbconn.execSQL(conn, GLOBAL_PT_FILES_QUERY)
                for r in res:
                    globalfiles.append(str(r[0]))
                res = dbconn.execSQL(conn, GET_ALL_DATABASES)
                for r in res:
                    databases[hostname,port].append((dbid, r[0], r[1]))

            if len(globalfiles) != 4:
                logger.error("Found: %s, expected: [gp_persistent_relation_node, gp_persistent_database_node,\
                              gp_persistent_tablespace_node, gp_persistent_filespace_node]" % globalfiles)
                raise Exception("Missing relfilenode entry of global pesistent tables in pg_class, dbid %s" % dbid)

            GLOBAL_PERSISTENT_FILES[hostname][dbid] = globalfiles

        """
        We have to connect to each database in all segments to get the
        relfilenode ids for per db persistent files.
        """
        for hostname, port in databases:
            dblist = databases[(hostname, port)]
            ptfiles_dboid = defaultdict(list)
            for dbid, dboid, database in dblist:
                if database == 'template0': #Connections to template0 are not allowed so we skip
                    continue
                with dbconn.connect(dbconn.DbURL(dbname=database, hostname=hostname, port=port), utility=True) as conn:
                    res = dbconn.execSQL(conn, PER_DATABASE_PT_FILES_QUERY)
                    for r in res:
                        ptfiles_dboid[int(dboid)].append(str(r[0]))

                if int(dboid) not in ptfiles_dboid or len(ptfiles_dboid[int(dboid)]) != 2:
                    logger.error("Found: %s, Expected: [gp_relation_node, gp_relation_node_index]" % \
                                  ptfiles_dboid[int(dboid)] if int(dboid) in ptfiles_dboid else "None")
                    raise Exception("Missing relfilenode entry of per database persistent tables in pg_class, dbid %s" % dbid)

                PER_DATABASE_PERSISTENT_FILES[hostname][dbid] = ptfiles_dboid


        """
        We also need to backup for mirrors and standby if they are configured
        """
        if self.has_mirrors or self.has_standby:
            for dbidinfo in self.dbid_info:
                if dbidinfo.role == 'm' and not dbidinfo.is_down: # Checking if the mirror is down
                    content = dbidinfo.content
                    mirror_dbid = dbidinfo.dbid
                    mirror_hostname = dbidinfo.hostname
                    primary_dbid, primary_hostname = content_to_primary_dbid_host_map[content]
                    GLOBAL_PERSISTENT_FILES[mirror_hostname][mirror_dbid] = GLOBAL_PERSISTENT_FILES[primary_hostname][primary_dbid]
                    PER_DATABASE_PERSISTENT_FILES[mirror_hostname][mirror_dbid] = PER_DATABASE_PERSISTENT_FILES[primary_hostname][primary_dbid]

        logger.debug('GLOBAL_PERSISTENT_FILES = %s' % GLOBAL_PERSISTENT_FILES)
        logger.debug('PER_DATABASE_PERSISTENT_FILES = %s' % PER_DATABASE_PERSISTENT_FILES)

    def print_warning(self):
        """
        Prints out a warning to the user indicating that this tool should
        only be run by Pivotal support. It also asks for confirmation
        before proceeding.
        """
        warning_msgs=  ['****************************************************', 'This tool should only be run by Pivotal support.',
                        'Please contact Pivotal support for more information.', '****************************************************']

        for warning_msg in warning_msgs:
            logger.warning(warning_msg)
        input = ask_yesno(None, 'Do you still wish to continue ?', 'N')
        if not input:
            raise Exception('Aborting rebuild due to user request')

    def dump_restore_info(self):
        """
        dump all object information into files, retrieve the information into object later on
        """
        for name, obj in [('dbid_info', self.dbid_info), ('global_pt_file', GLOBAL_PERSISTENT_FILES), ('per_db_pt_file', PER_DATABASE_PERSISTENT_FILES)]:
            restore_file = os.path.join(self.restore_state_file_location, name + '_' + TIMESTAMP)
            with open(restore_file, 'wb') as fw:
                pickle.dump(obj, fw)

    def load_restore_info(self, timestamp=None):
        """
        load the object information from dump file
        """
        pt_restore_files = [os.path.join(self.restore_state_file_location, name + '_' + timestamp) for name in ['dbid_info', 'global_pt_file', 'per_db_pt_file']]

        dbid_info_restore_file = pt_restore_files[0]
        with open(dbid_info_restore_file, 'rb') as fr:
            self.dbid_info = pickle.load(fr)

        global_pt_file_restore_file = pt_restore_files[1]
        with open(global_pt_file_restore_file, 'rb') as fr:
            global GLOBAL_PERSISTENT_FILES
            GLOBAL_PERSISTENT_FILES = pickle.load(fr)

        per_db_pt_file_restore_file = pt_restore_files[2]
        with open(per_db_pt_file_restore_file, 'rb') as fr:
            global PER_DATABASE_PERSISTENT_FILES
            PER_DATABASE_PERSISTENT_FILES = pickle.load(fr)

    def run(self):

        """
        Double warning to make sure that the customer
        does not run this tool accidentally
        """
        self.print_warning()
        self.print_warning()
        logger.info('Checking for platform')
        self._check_platform()

        logger.info('Checking for md5sum program')
        self._check_md5_prog()

        """
        If the restore fails, we do not attempt to restart the database since a restore is only done
        when the PT rebuild has not succeeded. It might be dangerous to start the database when the
        PT rebuild has failed in the middle and we cannot restore the original files safely.
        """
        if self.restore:
            logger.info('Loading global and per database persistent table files information from restore file,  %s' % self.restore)
            self.load_restore_info(self.restore)

            """
            Before stopping the database, pre check all required persistent relfilenode and transaction logs exist from backup
            """
            logger.info('Verifying backup directory for required persistent relfilenodes and transaction logs to restore')
            try:
                RunBackupRestore(self.dbid_info, self.restore, self.batch_size, self.backup_dir, validate_only=True).restore()
            except Exception as e:
                raise

            """
            We want to stop the database so that we can restore the persistent table files.
            """
            logger.info('Stopping Greenplum database')
            self._stop_database()

            logger.info('Restoring persistent table files, and all transaction logs from backup %s' % self.restore)
            try:
                RunBackupRestore(self.dbid_info, self.restore, self.batch_size, self.backup_dir).restore()
            except Exception as e:
                raise
            else:
                logger.info('Starting Greenplum database')
                self._start_database()
                return

        self.gparray = GpArray.initFromCatalog(dbconn.DbURL())

        logger.info('Checking for database version')
        self._check_database_version()

        logger.info('Validating if the system is configured with mirrors')
        self._validate_has_mirrors_and_standby()
        if self.has_mirrors:
            logger.info('System has been configured with mirrors')
        else:
            logger.info('System has been configured without mirrors')

        if self.has_standby:
            logger.info('System has been configured with standby')
        else:
            logger.info('System has been configured without standby')

        logger.info('Validating content ids')
        valid_contentids = ValidateContentID(self.content_id, self.contentid_file, self.gparray).validate()

        logger.info('Getting dbid information')
        self.dbid_info = GetDbIdInfo(self.gparray, valid_contentids).get_info()

        if self.backup_dir:
            logger.info('Validating backup directory')
            RunBackupRestore(self.dbid_info, TIMESTAMP, self.batch_size, self.backup_dir).validate_backup_dir()

        """
        We have to get the information about pt filenames from the master before we
        do any backup since the database will be down when we do a backup and this
        information is required in order to do the backup.
        """
        logger.info('Getting information about persistent table filenames')
        self._get_persistent_table_filenames()

        logger.info('Verifying data directory for required persistent relfilenodes and transaction logs to backup')
        try:
            RunBackupRestore(self.dbid_info, TIMESTAMP, self.batch_size, self.backup_dir, validate_only=True).backup()
        except Exception as e:
            raise

        """
        If we want to start persistent table rebuild instead of only making backup, first need to save
        the gpperfmon guc value into a file, then disable gpperfmon before shutdown cluster.
        """
        if not self.backup:
            self.dump_gpperfmon_guc()
            self.disable_gpperfmon()

        """
        We want to stop all transactions by pushing a checkpoint and stop the database so that we
        can backup the persistent table files.
        """
        logger.info('Pushing checkpoint')
        try:
            with dbconn.connect(dbconn.DbURL(dbname=DEFAULT_DATABASE, port=PGPORT)) as conn:
                dbconn.execSQL(conn, 'CHECKPOINT')
                conn.commit()
        except Exception as e:
            raise Exception('Failed to push a checkpoint, please contact support people')
        logger.info('Stopping Greenplum database')
        self._stop_database()

        """
        If a backup fails, we still attempt to restart the database since the original files are
        still present in their original location and we have not yet attempted to rebuild PT.
        """
        logger.info('Backing up persistent file, and all transaction log files')
        logger.info('Backup timestamp = %s' % TIMESTAMP)
        try:
            RunBackupRestore(self.dbid_info, TIMESTAMP, self.batch_size, self.backup_dir).backup()
            """
            After we have created the backup copies with timestamp, we save the restore information
            """
            logger.info('Dumpping restore information of global and per database persistent table files')
            self.dump_restore_info()
        except Exception as e:
            if not self.backup:
                logger.info('Setting back gpperfmon guc')
                self.restore_gpperfmon_guc()
            logger.info('Starting Greenplum database')
            self._start_database()
            raise
        else:
            if self.backup:
                logger.info('Starting Greenplum database')
                self._start_database()
                return
        finally:
            logger.info("To check list of files backed up, see pt_bkup_restore log under /tmp of segment host")

        """
        All the PT rebuild should be performed in utility mode in order to prevent
        user activity during the PT rebuild
        """
        logger.info('Starting database in admin mode')
        self._start_database(admin_mode=True)

        """
        Since the PT rebuild was done in admin mode, we need to restore the database
        back to the normal mode once the PT rebuild is complete.
        """
        logger.info('Starting rebuild of persistent tables')
        try:
            _, failures = RebuildTable(self.dbid_info, self.has_mirrors, self.batch_size, self.backup_dir).rebuild()
        finally:
            logger.info('Stopping Greenplum database that was started in admin mode')
            self._stop_database()

        if failures:
            """
            If the PT rebuild failed for any reason, we need to restore the original PT files and transaction
            log files.
            If the restore of the original PT files fails, we want to error out
            """
            logger.info('Restoring persistent table files, and all transaction log files')
            RunBackupRestore([f for f, e in failures], TIMESTAMP, self.batch_size, self.backup_dir).restore()

        """
        If we reach this point, either PT rebuild has completed successfully
        or we have succesfully replaced the original PT files and all transaction
        logs. Hence it is safe to restart the database
        """
        logger.info('Setting back gpperfmon guc')
        self.restore_gpperfmon_guc()

        logger.info('Starting Greenplum database')
        self._start_database()

        if failures:
            raise Exception('Persistent table rebuild was not completed succesfully and was restored back')
        else:
            logger.info('Completed rebuild of persistent tables')
            logger.info('To verify, run: $GPHOME/bin/lib/gpcheckcat -R persistent -A')

    def dump_gpperfmon_guc(self):
        """
        We want to save the gpperfmon guc value into a file, in case the rebuild process failed in the middle, so that
        the restore process can still pick up its original value and reset back.
        """
        GET_GPPERFMON_VALUE = """show gp_enable_gpperfmon;"""
        with dbconn.connect(dbconn.DbURL(dbname=DEFAULT_DATABASE)) as conn:
            res = dbconn.execSQL(conn, GET_GPPERFMON_VALUE)
            for r in res:
                gpperfmon_guc = r[0]
        logger.debug('Got gp_enable_gpperfmon guc value: %s' % gpperfmon_guc)
        logger.info('Dumping gp_enable_gpperfmon guc information into file: %s' % self.gpperfmon_file)
        with open(self.gpperfmon_file, 'w') as fw:
            fw.write('gp_enable_gpperfmon=%s'% gpperfmon_guc)

    def disable_gpperfmon(self):
        logger.info('Disabling gpperfmon')
        cmd = Command(name = 'Run gpconfig to set gpperfmon guc value off', cmdStr = 'gpconfig -c gp_enable_gpperfmon -v off')
        cmd.run(validateAfter = True)

    def restore_gpperfmon_guc(self):
        """Read the guc value from dump file and reset it from postgresql.conf on master """
        logger.debug('Retriving original gp_enable_gpperfmon guc value')
        with open(self.gpperfmon_file, 'r') as fr:
            content = fr.readlines();
            gpperfmon_guc_info = content[0].strip()
        postgres_config_file = os.path.join(os.environ.get('MASTER_DATA_DIRECTORY'), 'postgresql.conf')
        logger.debug('Resetting guc %s' % gpperfmon_guc_info)
        for line in fileinput.FileInput(postgres_config_file, inplace = 1):
            line = re.sub('(\s*)gp_enable_gpperfmon(\s*)=(\w+)', gpperfmon_guc_info, line)
            print str(re.sub('\n', '', line))
        logger.info('Completed reset of guc gp_enable_gpperfmon')

if __name__ == '__main__':
    pass
