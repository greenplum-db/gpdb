#!/usr/bin/env python3

from gppylib.commands.pg import PgBaseBackup, PgRewind
from recovery_base import RecoveryBase
from gppylib.commands.base import set_cmd_results, Command


class FullRecovery(Command):
    def __init__(self, name, recovery_info, forceoverwrite, logger):
        self.name = name
        self.recovery_info = recovery_info
        self.replicationSlotName = 'internal_wal_replication_slot'
        self.forceoverwrite = forceoverwrite
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
        try:
            self.logger.info("Running pg_basebackup with progress output temporarily in %s" % self.recovery_info.progress_file)
            cmd.run(validateAfter=True)

        except Exception as e:
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


class IncrementalRecovery(Command):
    def __init__(self, name, recovery_info, logger):
        self.name = name
        self.recovery_info = recovery_info
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


#TODO we may not need this class
class SegRecovery(object):
    def __init__(self):
        pass

    def main(self):
        RecoveryBase().main(__file__, self.get_recovery_cmds)

    def get_recovery_cmds(self, seg_recovery_info_list, forceoverwrite, logger):
        cmd_list = []
        for seg_recovery_info in seg_recovery_info_list:
            if seg_recovery_info.is_full_recovery:
                cmd = FullRecovery(name='Run pg_basebackup',
                                   recovery_info=seg_recovery_info,
                                   forceoverwrite=forceoverwrite,
                                   logger=logger)
            else:
                cmd = IncrementalRecovery(name='Run pg_rewind',
                                          recovery_info=seg_recovery_info,
                                          logger=logger)
            cmd_list.append(cmd)
        return cmd_list


if __name__ == '__main__':
    SegRecovery().main()
