#!/usr/bin/env python3

from gppylib.commands.pg import PgBaseBackup, PgRewind
from recovery_base import RecoveryBase
from gppylib.commands.base import set_cmd_results, Command
from gppylib.commands.gp import SegmentStart
from gppylib.gparray import Segment


class FullRecovery(Command):
    def __init__(self, name, recovery_info, forceoverwrite, logger, era):
        self.name = name
        self.recovery_info = recovery_info
        self.replicationSlotName = 'internal_wal_replication_slot'
        self.forceoverwrite = forceoverwrite
        self.era = era
        # TODO test for this cmdstr. also what should this cmdstr be ?
        cmdStr = ''

        #cmdstr = 'TODO? : {} {}'.format(str(recovery_info), self.verbose)
        Command.__init__(self, self.name, cmdStr)
        #TODO this logger has to come after the init and is duplicated in all the 4 classes
        self.logger = logger

    @set_cmd_results
    def run(self):
        # Create a mirror based on the primary
        cmd = PgBaseBackup(self.recovery_info.target_datadir,
                           self.recovery_info.source_hostname,
                           str(self.recovery_info.source_port),
                           create_slot=False,
                           replication_slot_name=self.replicationSlotName,
                           forceoverwrite=self.forceoverwrite,
                           target_gp_dbid=self.recovery_info.target_segment_dbid,
                           progress_file=self.recovery_info.progress_file)
        self.logger.info("Running pg_basebackup with progress output temporarily in %s" % self.recovery_info.progress_file)
        try:
            cmd.run(validateAfter=True)
        except Exception as e: #TODO should this be ExecutionError?
            self.logger.info("Running pg_basebackup failed: {}".format(str(e)))

            #  If the cluster never has mirrors, cmd will fail
            #  quickly because the internal slot doesn't exist.
            #  Re-run with `create_slot`.
            #  GPDB_12_MERGE_FIXME could we check it before? or let
            #  pg_basebackup create slot if not exists.
            cmd = PgBaseBackup(self.recovery_info.target_datadir,
                               self.recovery_info.source_hostname,
                               str(self.recovery_info.source_port),
                               create_slot=True,
                               replication_slot_name=self.replicationSlotName,
                               forceoverwrite=True,
                               target_gp_dbid=self.recovery_info.target_segment_dbid,
                               progress_file=self.recovery_info.progress_file)
            self.logger.info("Re-running pg_basebackup, creating the slot this time")
            cmd.run(validateAfter=True)

        self.logger.info("Successfully ran pg_basebackup for dbid: {}".format(
            self.recovery_info.target_segment_dbid))
        start_segment(self.recovery_info, self.logger, self.era)


class IncrementalRecovery(Command):
    def __init__(self, name, recovery_info, logger, era):
        self.name = name
        self.recovery_info = recovery_info
        self.era = era
        # TODO test for this cmdstr. also what should this cmdstr be ?
        cmdStr = ''

        #cmdstr = 'TODO? : {} {}'.format(str(recovery_info), self.verbose)
        Command.__init__(self, self.name, cmdStr)
        self.logger = logger

    @set_cmd_results
    def run(self):
        # Note the command name, we use the dbid later to
        # correlate the command results with GpMirrorToBuild
        # object.
        self.logger.info("Running pg_rewind with progress output temporarily in %s" % self.recovery_info.progress_file)
        cmd = PgRewind('rewind dbid: {}'.format(self.recovery_info.target_segment_dbid),
                       self.recovery_info.target_datadir, self.recovery_info.source_hostname,
                       self.recovery_info.source_port, self.recovery_info.progress_file)
        cmd.run(validateAfter=True)
        self.logger.info("Successfully ran pg_rewind for dbid: {}".format(
            self.recovery_info.target_segment_dbid))

        start_segment(self.recovery_info, self.logger, self.era)


def start_segment(recovery_info, logger, era):
    seg = Segment(None, None, None, None, None, None, None, None,
                  recovery_info.target_port, recovery_info.target_datadir)
    segStartCmd = SegmentStart(
        name="Starting new segment with dbid %s:" % (str(recovery_info.target_segment_dbid))
        , gpdb=seg
        , numContentsInCluster=0
        , era=era
        , mirrormode="mirror"
        , utilityMode=True)
    #TODO is this log msg for start segment ebnough?
    logger.info(str(segStartCmd))
    segStartCmd.run(validateAfter=True)


#TODO we may not need this class
class SegRecovery(object):
    def __init__(self):
        pass

    def main(self):
        recovery_base = RecoveryBase(__file__)
        recovery_base.main(self.get_recovery_cmds(recovery_base.seg_recovery_info_list, recovery_base.options.forceoverwrite,
                                                  recovery_base.logger, recovery_base.options.era))

    # TODO should we pass the recovery_base obj instead ?
    def get_recovery_cmds(self, seg_recovery_info_list, forceoverwrite, logger, era):
        cmd_list = []
        for seg_recovery_info in seg_recovery_info_list:
            if seg_recovery_info.is_full_recovery:
                cmd = FullRecovery(name='Run pg_basebackup',
                                   recovery_info=seg_recovery_info,
                                   forceoverwrite=forceoverwrite,
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
