#!/usr/bin/env python3
import os
import sys
import abc

from gppylib import recoveryinfo
from gppylib.commands.base import Command, WorkerPool, CommandResult, ExecutionError
from gppylib.db import dbconn
from recovery_base import RecoveryBase, set_recovery_cmd_results

#FIXME remove this class
class ValidationException(Exception):

    def __init__(self, msg):
        self.__msg = msg
        Exception.__init__(self, msg)

    def getMessage(self):
        return self.__msg


class SetupForIncrementalRecovery(Command):
    def __init__(self, name, recovery_info, logger):
        self.name = name
        self.recovery_info = recovery_info
        cmdStr = ''
        Command.__init__(self, self.name, cmdStr)
        self.logger = logger
        self.error_type = recoveryinfo.RecoveryErrorType.VALIDATION_ERROR

    def remove_postmaster_pid(self):
        cmd = Command(name='remove the postmaster.pid file',
                      cmdStr='rm -f %s/postmaster.pid' % self.recovery_info.target_datadir)
        cmd.run()
        return_code = cmd.get_return_code()
        if return_code != 0:
            raise ExecutionError("Failed while trying to remove postmaster.pid.", cmd)

    @set_recovery_cmd_results
    def run(self):

        # Do CHECKPOINT on source to force TimeLineID to be updated in pg_control.
        # pg_rewind wants that to make incremental recovery successful finally.
        self.logger.debug('Do CHECKPOINT on {} (port: {}) before running pg_rewind.'
                          .format(self.recovery_info.source_hostname, self.recovery_info.source_port))
        dburl = dbconn.DbURL(hostname=self.recovery_info.source_hostname,
                             port=self.recovery_info.source_port, dbname='template1')
        conn = dbconn.connect(dburl, utility=True)
        dbconn.execSQL(conn, "CHECKPOINT")
        conn.close()

        # If the postmaster.pid still exists and another process
        # is actively using that pid, pg_rewind will fail when it
        # tries to start the failed segment in single-user
        # mode. It should be safe to remove the postmaster.pid
        # file since we do not expect the failed segment to be up.
        self.remove_postmaster_pid()


class ValidationForFullRecovery(Command):
    def __init__(self, name, recovery_info, forceoverwrite, logger):
        self.name = name
        self.recovery_info = recovery_info
        self.forceoverwrite = forceoverwrite
        cmdStr = ''
        Command.__init__(self, self.name, cmdStr)
        self.logger = logger
        self.error_type = recoveryinfo.RecoveryErrorType.VALIDATION_ERROR

    @set_recovery_cmd_results
    def run(self):
        self.logger.info("Validate data directories for segment with dbid {}".
                         format(self.recovery_info.target_segment_dbid))
        if not self.forceoverwrite:
            self.validate_failover_data_directory()
        self.logger.info("Validation successful for segment with dbid: {}".format(
            self.recovery_info.target_segment_dbid))

    def validate_failover_data_directory(self):
        """
        Raises ValidationException when a validation problem is detected
        """

        if not os.path.exists(os.path.dirname(self.recovery_info.target_datadir)):
            self.make_or_update_data_directory()

        if not os.path.exists(self.recovery_info.target_datadir):
            return

        if len(os.listdir(self.recovery_info.target_datadir)) != 0:
            raise ValidationException("for segment with port {}: Segment directory '{}' exists but is not empty!"
                                      .format(self.recovery_info.target_port,
                                              self.recovery_info.target_datadir))

    def make_or_update_data_directory(self):
        if os.path.exists(self.recovery_info.target_datadir):
            os.chmod(self.recovery_info.target_datadir, 0o700)
        else:
            os.makedirs(self.recovery_info.target_datadir, 0o700)



#FIXME we may not need this class
class SegSetupRecovery(object):
    def __init__(self):
        pass

    def main(self):
        recovery_base = RecoveryBase(__file__)
        recovery_base.main(self.get_setup_cmds(recovery_base.seg_recovery_info_list, recovery_base.options.forceoverwrite,
                                               recovery_base.logger))

    def get_setup_cmds(self, seg_recovery_info_list, forceoverwrite, logger):
        cmd_list = []
        for seg_recovery_info in seg_recovery_info_list:
            if seg_recovery_info.is_full_recovery:
                cmd = ValidationForFullRecovery(name='Validate target segment dir for pg_basebackup',
                                                recovery_info=seg_recovery_info,
                                                forceoverwrite=forceoverwrite,
                                                logger=logger)
            else:
                cmd = SetupForIncrementalRecovery(name='Setup for pg_rewind', recovery_info=seg_recovery_info,
                                                  logger=logger)
            cmd_list.append(cmd)

        return cmd_list


if __name__ == '__main__':
    SegSetupRecovery().main()
