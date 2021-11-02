import os
import sys

from gppylib import gplog, recoveryinfo
from gppylib.commands import unix
from gppylib.commands.base import Command, WorkerPool, CommandResult, ExecutionError
from gppylib.commands.gp import DEFAULT_SEGHOST_NUM_WORKERS
from gppylib.gpparseopts import OptParser, OptChecker


class RecoveryBase(object):
    def __init__(self):
        self.description = ("""
        Configure mirror segment directories for running basebackup/rewind into a pre-existing GPDB array.
        """)

    def parseargs(self):
        parser = OptParser(option_class=OptChecker,
                           description=' '.join(self.description.split()),
                           version='%prog version $Revision: $')
        parser.set_usage('%prog is a utility script used by gprecoverseg, and gpaddmirrors and is not intended to be run separately.')
        parser.remove_option('-h')

        parser.add_option('-v','--verbose', action='store_true', help='debug output.', default=False)
        parser.add_option('-c', '--confinfo', type='string')
        parser.add_option('-b', '--batch-size', type='int', default=DEFAULT_SEGHOST_NUM_WORKERS, metavar='<batch_size>')
        parser.add_option('-f', '--force-overwrite', dest='forceoverwrite', action='store_true', default=False)
        parser.add_option('-l', '--log-dir', dest="logfileDirectory", type="string")

        # Parse the command line arguments
        options, _ = parser.parse_args()
        return options

    def main(self, file_name, get_cmd_list):
        pool = None
        logger = None
        try:
            options = self.parseargs()
            exec_name = os.path.split(file_name)[-1]
            logger = gplog.setup_tool_logging(exec_name, unix.getLocalHostname(),
                                              unix.getUserName(),
                                              logdir=options.logfileDirectory)

            if not options.confinfo:
                raise Exception('Missing --confinfo argument.')

            if options.batch_size <= 0:
                logger.warn('batch_size was less than zero.  Setting to 1.')
                options.batch_size = 1

            if options.verbose:
                gplog.enable_verbose_logging()

            # TODO: should we output the name of the exact file?
            logger.info("Starting recovery with args: %s" % ' '.join(sys.argv[1:]))

            seg_recovery_info_list = recoveryinfo.deserialize_recovery_info_list(options.confinfo)
            if len(seg_recovery_info_list) == 0:
                raise Exception('No segment configuration values found in --confinfo argument')

            cmd_list = get_cmd_list(seg_recovery_info_list, options.forceoverwrite, logger)

            pool = WorkerPool(numWorkers=min(options.batch_size, len(cmd_list)))
            self.run_cmd_list(cmd_list, logger, options, pool)

            sys.exit(0)
        except Exception as e:
            if logger:
                logger.error(str(e))
            print >> sys.stderr, e
            sys.exit(1)
        finally:
            if pool:
                pool.haltWork()

    # TODO: how to log multiple errors. New workflow reports errors from all segments.
    def run_cmd_list(self, cmd_list, logger, options, pool):
        for cmd in cmd_list:
            pool.addCommand(cmd)
        pool.join()

        errors = []
        for item in pool.getCompletedItems():
            if not item.get_results().wasSuccessful():
                errors.append(str(item.get_results().stderr).replace("\n", " "))

        if not errors:
            sys.exit(0)

        str_error = "\n".join(errors)
        print >> sys.stderr, str_error
        if options.verbose:
            logger.exception(str_error)
        logger.error(str_error)
        sys.exit(1)
