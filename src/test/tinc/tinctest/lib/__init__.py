"""
Copyright (c) 2004-Present Pivotal Software, Inc.

This program and the accompanying materials are made available under
the terms of the under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
"""

from gppylib.commands.base import Command, REMOTE
from gppylib.commands.gp import GpLogFilter
import inspect, math, os, time
import logging, sys
from time import localtime, strftime

import tinctest

from tinctest.main import TINCException

CURRENT_TIME_STRING = strftime('%Y%m%d_%H%M%S', localtime())

def local_path(filename):
    """Return the absolute path of the input file."""
    frame = inspect.stack()[1]
    source_file = inspect.getsourcefile(frame[0])
    source_dir = os.path.dirname(source_file)
    return os.path.join(source_dir, filename)

class Gpdiff(Command):
    """This is a wrapper for gpdiff.pl."""
    def __init__(self, out_file, ans_file, gp_ignore = True, ignore_header = True, ignore_plans = False, match_sub = []):
        cmd_str = 'gpdiff.pl -U 10 -w -B -I NOTICE:'
        if ignore_header:
            cmd_str += ' -I GP_IGNORE -gpd_ignore_headers'
        elif gp_ignore:
            cmd_str += ' -I GP_IGNORE'
        cmd_str += ' -gpd_init %s/global_init_file' % (os.path.abspath( os.path.dirname( __file__ ) ))
        if ignore_plans:
            cmd_str += ' -gpd_ignore_plans'
        if match_sub:
            cmd_str += ' -gpd_init '
            cmd_str += ' -gpd_init '.join(match_sub)
        cmd_str += ' %s %s' % (out_file, ans_file)
        Command.__init__(self, 'run gpdiff', cmd_str)

    @staticmethod 
    def are_files_equal(out_file, ans_file, gp_ignore = True, ignore_header = True, ignore_plans = False, match_sub = []):
        """Return True/False after comparing out_file and ans_file."""
        diff_file = out_file.replace('.out','.diff')
        cmd = Gpdiff(out_file, ans_file, gp_ignore, ignore_header, ignore_plans, match_sub)
        tinctest.logger.debug("Running gpdiff command - %s" %cmd)
        cmd.run()
        result = cmd.get_results()
        if result.rc != 0:
            tinctest.logger.error("gpdiff failed. Look into the diff file %s for more information" %diff_file)
            with open(diff_file, 'w') as dfile:
                dfile.write(result.stdout)
            return False
        return True

        

def collect_gpdb_logs(array, start_time, errors_only=False, master_only=False):
    """ 
    This retrieves log messages from all segments that happened within the last
    'duration' seconds.  The tuples returned are (dbid, hostname, datadir, logdata), 
    sorted by dbid.
    """
    log_chunks = []
    for seg in array.getDbList():
        if master_only and seg.getSegmentContentId() != -1:
            continue
        duration = time.time() - start_time
        cmd = GpLogFilter('collect log chunk',
                          '\\`ls -rt %s | tail -1\\`' % os.path.join(seg.getSegmentDataDirectory(), 'pg_log', '*.csv'),
                          duration='0:0:%s' % (int(math.ceil(duration)) + 3),
                          trouble=errors_only,
                          ctxt=REMOTE,
                          remoteHost=seg.getSegmentHostName())
        cmd.run()
        log_data = cmd.get_results().stdout
        if log_data:
            log_chunks.append((seg.getSegmentDbId(), 
                               seg.getSegmentHostName(),
                               seg.getSegmentDataDirectory(),
                               log_data))

    log_chunks.sort(key=lambda chunk: chunk[0])

    return log_chunks

# New level for sending status to all handlers
_LOGGER_CUSTOM_STATUS_LEVEL = 51
logging.addLevelName(_LOGGER_CUSTOM_STATUS_LEVEL, "STATUS")
# New level for sending def traces to all handlers; Setting it to 11 so that it shows up in debug file
_LOGGER_CUSTOM_TRACE_LEVEL = 11
logging.addLevelName(_LOGGER_CUSTOM_TRACE_LEVEL, "TRACE")

class TINCTestLogger(logging.Logger):
    """
    Create custom logger to allow tracing of enter/exit of definitions, as well as sending of status to all handles.
    """
    def trace_in(self, message = '', *args, **kwargs):
        frame, file_name, line_number, function_name, lines, index = inspect.getouterframes(inspect.currentframe())[1]
        new_message = "Entering " + file_name + " function " + function_name + " line " + str(line_number)
        if message != '':
            new_message += " with argument(s) : " + message
        self.log(_LOGGER_CUSTOM_TRACE_LEVEL, new_message, *args, **kwargs)

    def trace_out(self, message = '', *args, **kwargs):
        frame, file_name, line_number, function_name, lines, index = inspect.getouterframes(inspect.currentframe())[1]
        new_message = "Exiting " + file_name + " function " + function_name + " line " + str(line_number)
        if message != '':
            new_message += " with return value(s) : " + message
        self.log(_LOGGER_CUSTOM_TRACE_LEVEL, new_message, *args, **kwargs)

    def status(self, message = '', *args, **kwargs):
        self.log(_LOGGER_CUSTOM_STATUS_LEVEL, message, *args, **kwargs)

    def separator(self):
        self.status("=" * 80)

def get_logger():

    # Get the logger, and set the formatting
    logger = TINCTestLogger('TINC')
    logger.setLevel(logging.DEBUG)

    # New format of log:
    # time::[severity]::file:line#::process_id#::thread_id#:: message
    formatstr = '%(asctime)s::[%(levelname)s]::%(filename)s:%(lineno)s::pid%(process)s::%(threadName)s:: %(message)s'
    formatter = logging.Formatter(formatstr, '%Y%m%d:%H:%M:%S')

    logdir = os.path.join(os.path.expanduser('.'), 'log')
    if not os.path.exists(logdir):
        os.mkdir(logdir)

    # Setup debug log file
    debug_logfile = '%s/tinctest_%s_debug.log' % (logdir, CURRENT_TIME_STRING)
    debug_file_handler = logging.FileHandler(debug_logfile, 'w')
    debug_file_handler.setLevel(logging.DEBUG)
    debug_file_handler.setFormatter(formatter)

    # Add the file handler to logger
    logger.addHandler(debug_file_handler)
    
    return logger

def run_shell_command(cmdstr, cmdname = 'shell command', results={'rc':0, 'stdout':'', 'stderr':''}):
    cmd = Command(cmdname, cmdstr)
    tinctest.logger.info('Executing command: %s : %s' %(cmdname, cmdstr))
    cmd.run()
    result = cmd.get_results()
    results['rc'] = result.rc
    results['stdout'] = result.stdout
    results['stderr'] = result.stderr
    tinctest.logger.info('Finished command execution with return code %s ' %(str(result.rc)))
    tinctest.logger.debug('stdout: ' + result.stdout)
    tinctest.logger.debug('stderr: ' + result.stderr)
    if result.rc != 0:
        return False
    return True

