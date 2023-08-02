#!/usr/bin/env python3

import os
import signal

from gppylib.recoveryinfo import RecoveryErrorType
from gppylib.commands.pg import PgBaseBackup, PgRewind, PgReplicationSlot
from gppylib.commands.unix import Rsync
from recovery_base import RecoveryBase, set_recovery_cmd_results
from gppylib.commands.base import Command, LOCAL
from gppylib.commands.gp import SegmentStart
from gppylib.gparray import Segment
from gppylib.commands.gp import ModifyConfSetting
from gppylib.db.catalog import RemoteQueryCommand
from gppylib.operations.get_segments_in_recovery import is_seg_in_backup_mode
from gppylib.operations.segment_tablespace_locations import get_segment_tablespace_locations
from gppylib.commands.unix import terminate_proc_tree


class FullRecovery(Command):
    def __init__(self, name, recovery_info, forceoverwrite, logger, era):
        self.name = name
        self.recovery_info = recovery_info
        self.replicationSlotName = 'internal_wal_replication_slot'
        self.forceoverwrite = forceoverwrite
        self.era = era
        # FIXME test for this cmdstr. also what should this cmdstr be ?
        cmdStr = ''
        #cmdstr = 'TODO? : {} {}'.format(str(recovery_info), self.verbose)
        Command.__init__(self, self.name, cmdStr)
        #FIXME this logger has to come after the init and is duplicated in all the 4 classes
        self.logger = logger
        self.error_type = RecoveryErrorType.DEFAULT_ERROR

    @set_recovery_cmd_results
    def run(self):
        self.error_type = RecoveryErrorType.BASEBACKUP_ERROR
        cmd = PgBaseBackup(self.recovery_info.target_datadir,
                           self.recovery_info.source_hostname,
                           str(self.recovery_info.source_port),
                           create_slot=True,
                           replication_slot_name=self.replicationSlotName,
                           forceoverwrite=self.forceoverwrite,
                           target_gp_dbid=self.recovery_info.target_segment_dbid,
                           progress_file=self.recovery_info.progress_file)
        self.logger.info("Running pg_basebackup with progress output temporarily in %s" % self.recovery_info.progress_file)
        cmd.run(validateAfter=True)
        self.error_type = RecoveryErrorType.DEFAULT_ERROR
        self.logger.info("Successfully ran pg_basebackup for dbid: {}".format(
            self.recovery_info.target_segment_dbid))

        # Updating port number on conf after recovery
        self.error_type = RecoveryErrorType.UPDATE_ERROR
        update_port_in_conf(self.recovery_info, self.logger)

        self.error_type = RecoveryErrorType.START_ERROR
        start_segment(self.recovery_info, self.logger, self.era)


class IncrementalRecovery(Command):
    def __init__(self, name, recovery_info, logger, era):
        self.name = name
        self.recovery_info = recovery_info
        self.era = era
        cmdStr = ''
        Command.__init__(self, self.name, cmdStr)
        self.logger = logger
        self.error_type = RecoveryErrorType.DEFAULT_ERROR

    @set_recovery_cmd_results
    def run(self):
        self.logger.info("Running pg_rewind with progress output temporarily in %s" % self.recovery_info.progress_file)
        self.error_type = RecoveryErrorType.REWIND_ERROR
        cmd = PgRewind('rewind dbid: {}'.format(self.recovery_info.target_segment_dbid),
                       self.recovery_info.target_datadir, self.recovery_info.source_hostname,
                       self.recovery_info.source_port, self.recovery_info.progress_file)
        cmd.run(validateAfter=True)
        self.logger.info("Successfully ran pg_rewind for dbid: {}".format(self.recovery_info.target_segment_dbid))

        # Updating port number on conf after recovery
        self.error_type = RecoveryErrorType.UPDATE_ERROR
        update_port_in_conf(self.recovery_info, self.logger)

        self.error_type = RecoveryErrorType.START_ERROR
        start_segment(self.recovery_info, self.logger, self.era)


class DifferentialRecovery(Command):
    def __init__(self, name, recovery_info, logger, era):
        self.name = name
        self.recovery_info = recovery_info
        self.era = era
        self.logger = logger
        self.error_type = RecoveryErrorType.DEFAULT_ERROR
        self.replication_slot_name = 'internal_wal_replication_slot'
        self.replication_slot = PgReplicationSlot(self.recovery_info.source_hostname, self.recovery_info.source_port,
                                                  self.replication_slot_name)

    @set_recovery_cmd_results
    def run(self):

        self.logger.info("Running differential recovery with progress output temporarily in {}".format(
            self.recovery_info.progress_file))
        self.error_type = RecoveryErrorType.DIFFERENTIAL_ERROR

        """ Drop replication slot 'internal_wal_replication_slot' """
        if self.replication_slot.slot_exists() and not self.replication_slot.drop_slot():
            raise Exception("Failed to drop replication slot")

        """ start backup with label differential_backup """
        self.pg_start_backup()

        try:
            if not self.replication_slot.create_slot():
                raise Exception("Failed to create replication slot")

            """ rsync pg_data and tablespace directories including all the WAL files """
            self.sync_pg_data()

            """ rsync tablespace directories which are out of pg-data-directory """
            self.sync_tablespaces()

        finally:
            # Backup is completed, now run pg_stop_backup which will also remove backup_label file from
            # primary data_dir
            if is_seg_in_backup_mode(self.recovery_info.source_hostname, self.recovery_info.source_port):
                self.pg_stop_backup()

        """ Write the postresql.auto.conf and internal.auto.conf files """
        self.write_conf_files()

        """ sync pg_wal directory and pg_control file just before starting the segment """
        self.sync_wals_and_control_file()

        self.logger.info(
            "Successfully ran differential recovery for dbid {}".format(self.recovery_info.target_segment_dbid))

        """ Updating port number on conf after recovery """
        self.error_type = RecoveryErrorType.UPDATE_ERROR
        update_port_in_conf(self.recovery_info, self.logger)

        self.error_type = RecoveryErrorType.START_ERROR
        start_segment(self.recovery_info, self.logger, self.era)

    def sync_pg_data(self):
        self.logger.debug('Syncing pg_data of dbid {}'.format(self.recovery_info.target_segment_dbid))

        """
            rsync_exclude_list:
            1. List containing directories and files that should be excluded while taking backup. This list is prepared
               based on excludeDirContents and excludeFiles consts from postgres source file src/backend/backup/basebackup.c
            
            2. backup_label is present in excludeFiles list in src/backend/backup/basebackup.c as in case of 
               pg_basebackup, pg_start_backup() is queried in non-exclusive mode. pg_start_backup() in non-exclusive 
               mode does not create backup_label file so the backup_label (if present) belongs to other user who ran 
               pg_start_backup() in exclusive mode. backup_label is required by mirror to retrieve the wal location it 
               has to replay wals from so it's sent to mirror with tar files.
            
            3. backup_label has not been added in rsync_exclude_list that means it is sent to mirror during pg_data sync. 
               Reasons behind not excluding backup_label from pg_data sync:
                1. pg_start_backup() is executed in exclusive mode only by differential recovery which creates
                   backup_label on primary.
                2. So It's safe to copy backup_label file to mirror knowing that it is created by differential recovery 
                   as no other greenplum utility executes pg_start_backup() in exclusive mode at present.
                   
            4. In future we will have to add backup_label in rsync_exclude_list if pg_start_backup needs be started in
               non-exclusive mode for differential recovery.
        """
        rsync_exclude_list = [
            "/log",  # logs are segment specific so it can be skipped
            "pgsql_tmp",
            "postgresql.auto.conf.tmp",
            "current_logfiles.tmp",
            "postmaster.pid",
            "postmaster.opts",
            "pg_dynshmem",
            "pg_notify/*",
            "pg_replslot/*",
            "pg_serial/*",
            "pg_stat_tmp/*",
            "pg_snapshots/*",
            "pg_subtrans/*",
            "backups/*",
            "/db_dumps",  # as we exclude during pg_basebackup
            "/promote",  # Need to check why do we exclude it during pg_basebackup
        ]
        """
            Rsync options used:
                srcFile: source datadir
                dstFile: destination datadir or file that needs to be synced
                srcHost: source host
                exclude_list: to exclude specified files and directories to copied or synced with target
                delete: delete the files on target which does not exist on source
                checksum: to skip files being synced based on checksum, not modification time and size
                progress: to show the progress of rsync execution, like % transferred
        """

        # os.path.join(dir, "") will append a '/' at the end of dir. When using "/" at the end of source,
        # rsync will copy the content of the last directory. When not using "/" at the end of source, rsync
        # will copy the last directory and the content of the directory.
        cmd = Rsync(name="Sync pg data_dir", srcFile=os.path.join(self.recovery_info.source_datadir, ""),
                    dstFile=self.recovery_info.target_datadir,
                    srcHost=self.recovery_info.source_hostname, exclude_list=rsync_exclude_list,
                    delete=True, checksum=True, progress=True, progress_file=self.recovery_info.progress_file)
        cmd.run(validateAfter=True)

    def pg_start_backup(self):
        sql = "SELECT pg_start_backup('differential_backup');"
        try:
            RemoteQueryCommand("Start backup", sql, self.recovery_info.source_hostname,
                               self.recovery_info.source_port).run()
        except Exception as e:
            raise Exception("Failed to query pg_start_backup() for segment with host {} and port {} : {}".format(
                self.recovery_info.source_hostname,
                self.recovery_info.source_port, str(e)))
        self.logger.debug("Successfully ran pg_start_backup for segment on host {}, port {}".
                          format(self.recovery_info.source_hostname, self.recovery_info.source_port))

    def pg_stop_backup(self):
        sql = "SELECT pg_stop_backup();"
        try:
            RemoteQueryCommand("Stop backup", sql, self.recovery_info.source_hostname,
                               self.recovery_info.source_port).run()
        except Exception as e:
            raise Exception("Failed to query pg_stop_backup() for segment with host {} and port {} : {}".
                            format(self.recovery_info.source_hostname, self.recovery_info.source_port, str(e)))

        self.logger.debug("Successfully ran pg_stop_backup for segment on host {}, port {}".
                          format(self.recovery_info.source_hostname, self.recovery_info.source_port))

    def write_conf_files(self):
        self.logger.debug(
            "Writing recovery.conf and internal.auto.conf files for dbid {}".format(
                self.recovery_info.target_segment_dbid))
        cmd = PgBaseBackup(self.recovery_info.target_datadir,
                           self.recovery_info.source_hostname,
                           str(self.recovery_info.source_port),
                           writeconffilesonly=True,
                           replication_slot_name=self.replication_slot_name,
                           target_gp_dbid=self.recovery_info.target_segment_dbid,
                           recovery_mode=False)
        self.logger.debug("Running pg_basebackup to only write configuration files")
        cmd.run(validateAfter=True)

    def sync_wals_and_control_file(self):
        self.logger.debug("Syncing pg_wal directory of dbid {}".format(self.recovery_info.target_segment_dbid))
        # os.path.join(dir, "") will append a '/' at the end of dir. When using "/" at the end of source,
        # rsync will copy the content of the last directory. When not using "/" at the end of source, rsync
        # will copy the last directory and the content of the directory.
        cmd = Rsync(name="Sync pg_wal files", srcFile=os.path.join(self.recovery_info.source_datadir, "pg_wal", ""),
                    dstFile=os.path.join(self.recovery_info.target_datadir, "pg_wal", ""), progress=True, checksum=True,
                    srcHost=self.recovery_info.source_hostname,
                    progress_file=self.recovery_info.progress_file)
        cmd.run(validateAfter=True)

        self.logger.debug("Syncing pg_control file of dbid {}".format(self.recovery_info.target_segment_dbid))
        cmd = Rsync(name="Sync pg_control file",
                    srcFile=os.path.join(self.recovery_info.source_datadir, "global", "pg_control"),
                    dstFile=os.path.join(self.recovery_info.target_datadir, "global", "pg_control"), progress=True,
                    checksum=True,
                    srcHost=self.recovery_info.source_hostname, progress_file=self.recovery_info.progress_file)
        cmd.run(validateAfter=True)

    def sync_tablespaces(self):
        self.logger.debug(
            "Syncing tablespaces of dbid {} which are outside of data_dir".format(
                self.recovery_info.target_segment_dbid))

        # get the tablespace locations
        tablespaces = get_segment_tablespace_locations(self.recovery_info.source_hostname,
                                                       self.recovery_info.source_port)

        for tablespace_location in tablespaces:
            if tablespace_location[0].startswith(self.recovery_info.target_datadir):
                continue
            # os.path.join(dir, "") will append a '/' at the end of dir. When using "/" at the end of source,
            # rsync will copy the content of the last directory. When not using "/" at the end of source, rsync
            # will copy the last directory and the content of the directory.
            cmd = Rsync(name="Sync tablespace",
                        srcFile=os.path.join(tablespace_location[0], ""),
                        dstFile=tablespace_location[0],
                        srcHost=self.recovery_info.source_hostname,
                        progress=True,
                        checksum=True,
                        progress_file=self.recovery_info.progress_file)
            cmd.run(validateAfter=True)


def start_segment(recovery_info, logger, era):
    seg = Segment(None, None, None, None, None, None, None, None,
                  recovery_info.target_port, recovery_info.target_datadir)
    cmd = SegmentStart(
        name="Starting new segment with dbid %s:" % (str(recovery_info.target_segment_dbid))
        , gpdb=seg
        , numContentsInCluster=0
        , era=era
        , mirrormode="mirror"
        , utilityMode=False)
    logger.info(str(cmd))
    cmd.run(validateAfter=True)


def update_port_in_conf(recovery_info, logger):
    logger.info("Updating %s/postgresql.conf" % recovery_info.target_datadir)
    modifyConfCmd = ModifyConfSetting('Updating %s/postgresql.conf' % recovery_info.target_datadir,
                                      "{}/{}".format(recovery_info.target_datadir, 'postgresql.conf'),
                                      'port', recovery_info.target_port, optType='number')
    modifyConfCmd.run(validateAfter=True)


#FIXME we may not need this class
class SegRecovery(object):
    def __init__(self):
        pass

    def main(self):
        recovery_base = RecoveryBase(__file__)

        def signal_handler(sig, frame):
            recovery_base.logger.warning("Recieved termination signal, stopping gpsegrecovery")

            while not recovery_base.pool.isDone():

                # gpsegrecovery will be the parent for all the child processes (pg_basebackup/pg_rewind/rsync)
                terminate_proc_tree(pid=os.getpid(), include_parent=False)

        signal.signal(signal.SIGTERM, signal_handler)

        recovery_base.main(self.get_recovery_cmds(recovery_base.seg_recovery_info_list, recovery_base.options.forceoverwrite,
                                                  recovery_base.logger, recovery_base.options.era))

    def get_recovery_cmds(self, seg_recovery_info_list, forceoverwrite, logger, era):
        cmd_list = []
        for seg_recovery_info in seg_recovery_info_list:
            if seg_recovery_info.is_full_recovery:
                cmd = FullRecovery(name='Run pg_basebackup',
                                   recovery_info=seg_recovery_info,
                                   forceoverwrite=forceoverwrite,
                                   logger=logger,
                                   era=era)
            elif seg_recovery_info.is_differential_recovery:
                cmd = DifferentialRecovery(name='Run rsync',
                                           recovery_info=seg_recovery_info,
                                           logger=logger,
                                           era=era)
            else:
                cmd = IncrementalRecovery(name='Run pg_rewind',
                                          recovery_info=seg_recovery_info,
                                          logger=logger,
                                          era=era)

            cmd_list.append(cmd)
        return cmd_list


if __name__ == '__main__':
    SegRecovery().main()
