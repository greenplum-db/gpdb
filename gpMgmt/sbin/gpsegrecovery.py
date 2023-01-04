#!/usr/bin/env python3

from gppylib.recoveryinfo import RecoveryErrorType
from gppylib.commands.pg import PgBaseBackup, PgRewind, RsyncCopy, PgReplicationSlot
from recovery_base import RecoveryBase, set_recovery_cmd_results
from gppylib.commands.base import Command, LOCAL
from gppylib.commands.gp import SegmentStart
from gppylib.gparray import Segment
from gppylib.commands.gp import ModifyConfSetting
from contextlib import closing
from gppylib.db import dbconn


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
        self.logger.info(
            "Running pg_basebackup with progress output temporarily in %s" % self.recovery_info.progress_file)
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

class DiffRecovery(Command):
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
        self.logger.info("Running rsync with progress output temporarily in %s" % self.recovery_info.progress_file)
        self.error_type = RecoveryErrorType.SYNC_ERROR

        """ drop replication slot 'internal_wal_replication_slot' """
        replication_slot = PgReplicationSlot(self.recovery_info.source_hostname, self.recovery_info.source_port,
                                             'internal_wal_replication_slot')
        replication_slot.drop_slot()

        """start backup with pg_start_backup()"""
        self.pgStartBackup()

        """ create replication slot pg_create_physical_replication_slot('internal_wal_replication_slot', true, false)"""
        replication_slot.create_slot()

        """ rsync data and tablespace directories including all the WAL files """
        cmd = RsyncCopy('syncing dbid: {}'.format(self.recovery_info.target_segment_dbid),
                        self.recovery_info.target_datadir, self.recovery_info.source_hostname,
                        self.recovery_info.source_datadir, self.recovery_info.progress_file)
        cmd.run(validateAfter=True)

        """ rsync tablespace directories which are out of data-directory """
        self.syncTableSpaces()

        """ stop backup with pg_stop_backup() """
        self.pgStopBackup()

        """ Write the postresql.auto.conf and internal.auto.conf files """
        self.writeConfFiles()

        """ sync pg_wal directory and pg_control file just before starting the segment """
        self.syncWalsandControlFile()

        self.logger.info(
            "Successfully ran diff recovery for dbid: {}".format(self.recovery_info.target_segment_dbid))

        """ Updating port number on conf after recovery """
        self.error_type = RecoveryErrorType.UPDATE_ERROR
        update_port_in_conf(self.recovery_info, self.logger)

        self.error_type = RecoveryErrorType.START_ERROR
        start_segment(self.recovery_info, self.logger, self.era)

    def pgStartBackup(self):
        sql = "SELECT pg_start_backup('diff_backup');"
        try:
            dburl = dbconn.DbURL(hostname=self.recovery_info.source_hostname, port=self.recovery_info.source_port)
            with closing(dbconn.connect(dburl, utility=True, encoding='UTF8')) as conn:
                dbconn.query(conn, sql)
        except Exception as ex:
            raise Exception("Failed to start backup for host:{}, port:{} : {}".
                            format(self.recovery_info.source_hostname, self.recovery_info.source_port, str(ex)))

        self.logger.debug("Successfully started backup for host:{}, port:{}".
                          format(self.recovery_info.source_hostname, self.recovery_info.source_port))

    def pgStopBackup(self):
        sql = "SELECT pg_stop_backup();"
        try:
            dburl = dbconn.DbURL(hostname=self.recovery_info.source_hostname, port=self.recovery_info.source_port)
            with closing(dbconn.connect(dburl, utility=True, encoding='UTF8')) as conn:
                dbconn.query(conn, sql)
        except Exception as ex:
            raise Exception("Failed to stop backup for host:{}, port:{} : {}".
                            format(self.recovery_info.source_hostname, self.recovery_info.source_port, str(ex)))

        self.logger.debug("Successfully stopped backup for host:{}, port:{}".
                          format(self.recovery_info.source_hostname, self.recovery_info.source_port))

    def writeConfFiles(self):
        self.logger.debug("Writing recovery.conf and internal.auto.conf files")
        cmd = PgBaseBackup(self.recovery_info.target_datadir,
                           self.recovery_info.source_hostname,
                           str(self.recovery_info.source_port),
                           justWriteConfFiles=True,
                           target_gp_dbid=self.recovery_info.target_segment_dbid,
                           progress_file=self.recovery_info.progress_file)
        self.logger.info("Running pg_basebackup to just write configuration files")
        cmd.run(validateAfter=True)

    def syncWalsandControlFile(self):
        self.logger.debug("Syncing wals and pg_control")
        cmdStr = 'rsync -aLKs -e "ssh -o BatchMode=yes -o StrictHostKeyChecking=no" %s:%s/pg_wal/ %s/pg_wal/' % (
            self.recovery_info.source_hostname, self.recovery_info.source_datadir, self.recovery_info.target_datadir)
        cmd = Command("sync wals", cmdStr, LOCAL)
        cmd.run(validateAfter=True)

        cmdStr = 'rsync -aLKs -e "ssh -o BatchMode=yes -o StrictHostKeyChecking=no" %s:%s/global/pg_control %s/global/pg_control' % (
            self.recovery_info.source_hostname, self.recovery_info.source_datadir, self.recovery_info.target_datadir)
        cmd = Command("sync pg_control", cmdStr, LOCAL)
        cmd.run(validateAfter=True)

    def getSegmentTablespaceLocations(self):
        sql = "SELECT tblspc_loc FROM ( SELECT oid FROM pg_tablespace WHERE spcname NOT IN ('pg_default', 'pg_global')) AS q,LATERAL gp_tablespace_location(q.oid);"
        try:
            dburl = dbconn.DbURL(hostname=self.recovery_info.source_hostname, port=self.recovery_info.source_port)
            with closing(dbconn.connect(dburl, utility=True, encoding='UTF8')) as conn:
                res = dbconn.query(conn, sql).fetchall()
        except Exception as ex:
            raise Exception("Failed to query pg_tablsepace for segment with host:{}, port:{}: {}".format(self.recovery_info.source_hostname, self.recovery_info.source_port, str(ex)))

        self.logger.debug("Successfully got tablespace locations for segment with host:{}, port:{}".
                          format(self.recovery_info.source_hostname, self.recovery_info.source_port))
        return res

    def syncTableSpaces(self):
        self.logger.debug("Syncing tablespaces which are outside of source data_dir")

        # get the tablespaces locations
        tablespaces = self.getSegmentTablespaceLocations()

        for tablespace_location in tablespaces:
            if not tablespace_location[0].startswith(self.recovery_info.target_datadir):
                cmdStr = 'rsync -aLKs -e "ssh -o BatchMode=yes -o StrictHostKeyChecking=no" %s:%s %s' % (self.recovery_info.source_hostname, tablespace_location[0], tablespace_location[0])
                cmd = Command("sync tablespace", cmdStr, LOCAL)
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
        , utilityMode=True)
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
            elif seg_recovery_info.is_diff_recovery:
                cmd = DiffRecovery(name='Run rsync',
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
