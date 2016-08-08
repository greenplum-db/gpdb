import logging
import os
import sys

from time import localtime, strftime

from gppylib.commands.base import Command

def get_logger(logger_name, logger_level):
    logger = logging.getLogger(logger_name)
    logger.setLevel(logger_level)
    formatstr = '%(asctime)s:tinc:-[%(levelname)-s]:-%(message)s'
    formatter = logging.Formatter(formatstr, '%Y%m%d:%H:%M:%S')
        
    empty_formatter = logging.Formatter()

    logdir = os.path.join(os.path.expanduser('.'), 'log')
    if not os.path.exists(logdir):
        os.mkdir(logdir)
    logfile = '%s/qautils_%s.log' % (logdir, strftime('%Y%m%d_%H%M%S', localtime()))
    fhandler = logging.FileHandler(logfile, 'w')
    fhandler.setLevel(logger_level)
    fhandler.setFormatter(formatter)
    logger.addHandler(fhandler)

    shandler = logging.StreamHandler(sys.stdout)
    shandler.setFormatter(empty_formatter)
    #logger.addHandler(shandler)
    return logger

def run_shell_command(cmdstr, cmdname = 'shell command'):
    cmd = Command(cmdname, cmdstr)
    cmd.run()
    result = cmd.get_results()
    if result.rc != 0:
        return False
    return True
