import functools
import json
import os
import sys

from gppylib import gplog, recoveryinfo
from gppylib.commands import unix
from gppylib.commands.base import Command, WorkerPool, CommandResult, ExecutionError
from gppylib.commands.gp import DEFAULT_SEGHOST_NUM_WORKERS
from gppylib.gpparseopts import OptParser, OptChecker


class RecoveryBase(object):
    def __init__(self, file_name):
        self.description = ("""
        Configure mirror segment directories for running basebackup/rewind into a pre-existing GPDB array.
        """)
        self.file_name = file_name
        self.logger = None
        self.seg_recovery_info_list = None
        self.options = None
        self.pool = None
        try:
            self.parseargs()
        except Exception as e:
            self._write_to_stderr_and_exit(e)

    def parseargs(self):
        parser = OptParser(option_class=OptChecker,
                           description=' '.join(self.description.split()),
                           version='%prog version $Revision: $')
        parser.set_usage('%prog is a utility script used by gprecoverseg, and gpaddmirrors and is not intended to be run separately.')
        parser.remove_option('-h')

        #TODO we may not need the verbose flag
        parser.add_option('-v','--verbose', action='store_true', help='debug output.', default=False)
        parser.add_option('-c', '--confinfo', type='string')
        parser.add_option('-b', '--batch-size', type='int', default=DEFAULT_SEGHOST_NUM_WORKERS, metavar='<batch_size>')
        parser.add_option('-f', '--force-overwrite', dest='forceoverwrite', action='store_true', default=False)
        parser.add_option('-l', '--log-dir', dest="logfileDirectory", type="string")
        parser.add_option('', '--era', dest="era", help="coordinator era", )

        # Parse the command line arguments
        self.options, _ = parser.parse_args()

        if not self.options.confinfo:
            raise Exception('Missing --confinfo argument.')
        if not self.options.logfileDirectory:
            raise Exception('Missing --log-dir argument.')

        self.logger = gplog.setup_tool_logging(os.path.split(self.file_name)[-1], unix.getLocalHostname(),
                                          unix.getUserName(),
                                          logdir=self.options.logfileDirectory)

        if self.options.batch_size <= 0:
            self.logger.warn('batch_size was less than zero.  Setting to 1.')
            self.options.batch_size = 1

        if self.options.verbose:
            gplog.enable_verbose_logging()

        self.seg_recovery_info_list = recoveryinfo.deserialize_list(self.options.confinfo)
        if len(self.seg_recovery_info_list) == 0:
            raise Exception('No segment configuration values found in --confinfo argument')

    def main(self, cmd_list):
        try:
            # TODO: should we output the name of the exact file?
            self.logger.info("Starting recovery with args: %s" % ' '.join(sys.argv[1:]))

            self.pool = WorkerPool(numWorkers=min(self.options.batch_size, len(cmd_list)))
            self.run_cmd_list(cmd_list, self.logger, self.options, self.pool)
            sys.exit(0)
        except Exception as e:
            self._write_to_stderr_and_exit(e)
        finally:
            if self.pool:
                self.pool.haltWork()

    def _write_to_stderr_and_exit(self, e):
        if self.logger:
            self.logger.error(str(e))
        print(e, file=sys.stderr)
        sys.exit(1)

    def run_cmd_list(self, cmd_list, logger, options, pool):
        for cmd in cmd_list:
            pool.addCommand(cmd)
        pool.join()

        errors = []
        for item in pool.getCompletedItems():
            if not item.get_results().wasSuccessful():
                err_str = item.get_results().stderr
                try:
                    error_obj = json.loads(err_str)
                except ValueError:
                    #TODO Do we need this except and can we set dbid to None or should we rely on item.recovery_info?
                    error_obj = recoveryinfo.RecoveryError(recoveryinfo.RecoveryErrorType.DEFAULT_ERROR, err_str, None,
                                                            None, None, None)
                errors.append(error_obj)
        if not errors:
            sys.exit(0)

        str_error = recoveryinfo.serialize_list(errors)
        print(str_error, file=sys.stderr)
        if options.verbose:
            logger.exception(str_error)
        logger.error(str_error)
        sys.exit(1)


def set_recovery_cmd_results(run_func):
    """
    Decorator function to run a command on the seg and set the results on the command object for that segment.
    :param run_func: The function to run
    :return:
    """
    @functools.wraps(run_func)
    def run_and_set_output(recovery_cmd):
        try:
            run_func(recovery_cmd)
        except Exception as e:
            serialized_error = str(recoveryinfo.RecoveryError(recovery_cmd.error_type, str(e),
                                                              recovery_cmd.recovery_info.target_segment_dbid,
                                                              recovery_cmd.recovery_info.target_datadir,
                                                              recovery_cmd.recovery_info.target_port,
                                                              recovery_cmd.recovery_info.progress_file))
            recovery_cmd.set_results(CommandResult(1, b'', serialized_error.encode(), True, False))
        else:
            recovery_cmd.set_results(CommandResult(0, b'', b'', True, False))

    return run_and_set_output
