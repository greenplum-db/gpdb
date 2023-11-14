# Line too long - pylint: disable=C0301
# Invalid name  - pylint: disable=C0103

"""
mainUtils.py
------------

This file provides a rudimentary framework to support top-level option
parsing, initialization and cleanup logic common to multiple programs.

The primary interface function is 'simple_main'.  For an example of
how it is expected to be used, see gprecoverseg.

It is anticipated that the functionality of this file will grow as we
extend common functions of our gp utilities.  Please keep this in mind
and try to avoid placing logic for a specific utility here.
"""

import errno, os, sys, shutil

gProgramName = os.path.split(sys.argv[0])[-1]
if sys.version_info < (2, 5, 0):
    sys.exit(
        '''Error: %s is supported on Python versions 2.5 or greater
        Please upgrade python installed on this machine.''' % gProgramName)

from gppylib import gplog
from gppylib.commands import gp, unix
from gppylib.commands.base import ExecutionError
from gppylib.system import configurationInterface, configurationImplGpdb, fileSystemInterface, \
    fileSystemImplOs, osInterface, osImplNative, faultProberInterface, faultProberImplGpdb
from optparse import OptionGroup, OptionParser, SUPPRESS_HELP


def getProgramName():
    """
    Return the name of the current top-level program from sys.argv[0]
    or the programNameOverride option passed to simple_main via mainOptions.
    """
    global gProgramName
    return gProgramName

class PIDLockHeld(Exception):
    def __init__(self, message, path):
        self.message = message
        self.path = path

class PIDLockFile:
    """
    Create a lock, utilizing the atomic nature of mkdir on Unix
    Inside of this directory, a file named PID contains exactly the PID, with
    no newline or space, of the process which created the lock.

    The process which created the lock can release the lock. The lock will
    be released by the process which created it on object deletion
    """

    def __init__(self, path):
        self.path = path
        self.PIDfile = os.path.join(path, "PID")
        self.PID = os.getpid()

    def acquire(self):
        try:
            os.makedirs(self.path)
            with open(self.PIDfile, mode='w') as p:
                p.write(str(self.PID))
        except EnvironmentError as e:
            if e.errno == errno.EEXIST:
                raise PIDLockHeld("PIDLock already held at %s" % self.path, self.path)
            else:
                raise
        except:
            raise

    def release(self):
        """
        If the PIDfile or directory have been removed, the lock no longer
        exists, so pass
        """
        try:
            # only delete the lock if we created the lock
            if self.PID == self.read_pid():
                # remove the dir and PID file inside of it
                shutil.rmtree(self.path)

                # Eventhough we remove the directory, it is not guaranteed that the directory is removed
                # at the disk level. So it is necessary to make this call to sync the changes at the disk level.
                # Refer https://stackoverflow.com/questions/7127075/what-exactly-is-file-flush-doing for more context.
                os.sync()

        except EnvironmentError as e:
            if e.errno == errno.ENOENT:
                pass
            else:
                raise
        except:
            raise

    def read_pid(self):
        """
        Return the PID of the process owning the lock as an int
        Return None if there is no lock
        """
        owner = ""
        try:
            with open(self.PIDfile) as p:
                owner = int(p.read())
        except EnvironmentError as e:
            if e.errno == errno.ENOENT:
                return None
            else:
                raise
        except:
            raise
        return owner

    def __enter__(self):
        self.acquire()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.release()
        return None

    def __del__(self):
        self.release()


class SimpleMainLock:
    """
    Tools like gprecoverseg prohibit running multiple instances at the same time
    via a simple lock file created in the COORDINATOR_DATA_DIRECTORY.  This class takes
    care of the work to manage this lock as appropriate based on the mainOptions
    specified.

    Note that in some cases, the utility may want to recursively invoke
    itself (e.g. gprecoverseg -r).  To handle this, the caller may specify
    the name of an environment variable holding the pid already acquired by
    the parent process.
    """

    def __init__(self, mainOptions):
        self.pidlockpath = mainOptions.get('pidlockpath', None)  # the directory we're using for locking
        self.parentpidvar = mainOptions.get('parentpidvar', None)  # environment variable holding parent pid
        self.parentpid = None  # parent pid which already has the lock
        self.ppath = None  # complete path to the lock file
        self.pidlockfile = None  # PIDLockFile object
        self.pidfilepid = None  # pid of the process which has the lock
        self.locktorelease = None  # PIDLockFile object we should release when done

        if self.parentpidvar is not None and self.parentpidvar in os.environ:
            self.parentpid = int(os.environ[self.parentpidvar])

        if self.pidlockpath is not None:
            self.ppath = os.path.join(gp.get_coordinatordatadir(), self.pidlockpath)
            self.pidlockfile = PIDLockFile(self.ppath)

    def acquire(self):
        """
        Attempts to acquire the lock this process needs to proceed.

        Returns None on successful acquisition of the lock or 
          the pid of the other process which already has the lock.
        """
        # nothing to do if utiliity requires no locking
        if self.pidlockfile is None:
            return None

        # look for a lock file
        try:
            self.pidfilepid = self.pidlockfile.read_pid()
        except ValueError:
            shutil.rmtree(self.ppath)

        if self.pidfilepid is not None:

            # we found a lock file
            # allow the process to proceed if the locker was our parent
            if self.pidfilepid == self.parentpid:
                return None

            # Check if the process that holds the lock exists.
            # If the process is already killed, remove the lock directory.
            if not unix.check_pid(self.pidfilepid):
                shutil.rmtree(self.ppath)

        # try and acquire the lock
        try:
            self.pidlockfile.acquire()

        except PIDLockHeld:
            self.pidfilepid = self.pidlockfile.read_pid()
            return self.pidfilepid

        # we have the lock
        # prepare for a later call to release() and take good
        # care of the process environment for the sake of our children
        self.locktorelease = self.pidlockfile
        self.pidfilepid = self.pidlockfile.read_pid()
        if self.parentpidvar is not None:
            os.environ[self.parentpidvar] = str(self.pidfilepid)

        return None

    def release(self):
        """
        Releases the lock this process acquired.
        """
        if self.locktorelease is not None:
            self.locktorelease.release()
            self.locktorelease = None


#
# exceptions we handle specially by the simple_main framework.
#

class ProgramArgumentValidationException(Exception):
    """
    Throw this out to main to have the message possibly
    printed with a help suggestion.
    """

    def __init__(self, msg, shouldPrintHelp=False):
        "init"
        Exception.__init__(self, msg)
        self.__shouldPrintHelp = shouldPrintHelp
        self.__msg = msg

    def shouldPrintHelp(self):
        "shouldPrintHelp"
        return self.__shouldPrintHelp

    def getMessage(self):
        "getMessage"
        return self.__msg


class ExceptionNoStackTraceNeeded(Exception):
    """
    Our code throws this exception when we encounter a condition
    we know can arise which demands immediate termination.
    """
    pass


class UserAbortedException(Exception):
    """
    UserAbortedException should be thrown when a user decides to stop the
    program (at a y/n prompt, for example).
    """
    pass


def simple_main(createOptionParserFn, createCommandFn, mainOptions=None):
    """
     createOptionParserFn : a function that takes no arguments and returns an OptParser
     createCommandFn : a function that takes two arguments (the options and the args (those that are not processed into
                       options) and returns an object that has "run" and "cleanup" functions.  Its "run" function must
                       run and return an exit code.  "cleanup" will be called to clean up before the program exits;
                       this can be used to clean up, for example, to clean up a worker pool

     mainOptions can include: forceQuietOutput (map to bool),
                              programNameOverride (map to string)
                              suppressStartupLogMessage (map to bool)
                              useHelperToolLogging (map to bool)
                              setNonuserOnToolLogger (map to bool, defaults to false)
                              pidlockpath (string)
                              parentpidvar (string)

    """
    simple_main_internal(createOptionParserFn, createCommandFn, mainOptions)


def simple_main_internal(createOptionParserFn, createCommandFn, mainOptions):

    """
    if -d <coordinator_data_dir> option is provided in that case doing parsing after creating
    lock file would not be a good idea therefore handling -d option before lock.
    """
    parser = createOptionParserFn()
    (parserOptions, parserArgs) = parser.parse_args()

    if parserOptions.ensure_value("coordinatorDataDirectory", None) is not None:
        parserOptions.coordinator_data_directory = os.path.abspath(parserOptions.coordinatorDataDirectory)
        gp.set_coordinatordatadir(parserOptions.coordinator_data_directory)

    """
    If caller specifies 'pidlockpath' in mainOptions then we manage the
    specified pid file within the COORDINATOR_DATA_DIRECTORY before proceeding
    to execute the specified program and we clean up the pid file when
    we're done.
    """
    sml = None
    if mainOptions is not None and 'pidlockpath' in mainOptions:
        sml = SimpleMainLock(mainOptions)
        otherpid = sml.acquire()
        if otherpid is not None:
            logger = gplog.get_default_logger()
            logger.error("Lockfile %s indicates that an instance of %s is already running with PID %s" % (sml.ppath, getProgramName(), otherpid))
            logger.error("If this is not the case, remove the lockfile directory at %s" % (sml.ppath))
            return

    # at this point we have whatever lock we require
    try:
        simple_main_locked(parser, parserOptions, parserArgs, createCommandFn, mainOptions)
    finally:
        if sml is not None:
            sml.release()


def simple_main_locked(parser, parserOptions, parserArgs, createCommandFn, mainOptions):
    """
    Not to be called externally -- use simple_main instead
    """
    logger = gplog.get_default_logger()

    configurationInterface.registerConfigurationProvider(
        configurationImplGpdb.GpConfigurationProviderUsingGpdbCatalog())
    fileSystemInterface.registerFileSystemProvider(fileSystemImplOs.GpFileSystemProviderUsingOs())
    osInterface.registerOsProvider(osImplNative.GpOsProviderUsingNative())
    faultProberInterface.registerFaultProber(faultProberImplGpdb.GpFaultProberImplGpdb())

    commandObject = None

    forceQuiet = mainOptions is not None and mainOptions.get("forceQuietOutput")

    if mainOptions is not None and mainOptions.get("programNameOverride"):
        global gProgramName
        gProgramName = mainOptions.get("programNameOverride")
    suppressStartupLogMessage = mainOptions is not None and mainOptions.get("suppressStartupLogMessage")

    useHelperToolLogging = mainOptions is not None and mainOptions.get("useHelperToolLogging")
    nonuser = True if mainOptions is not None and mainOptions.get("setNonuserOnToolLogger") else False
    exit_status = 1

    try:
        execname = getProgramName()
        hostname = unix.getLocalHostname()
        username = unix.getUserName()

        if useHelperToolLogging:
            gplog.setup_helper_tool_logging(execname, hostname, username)
        else:
            gplog.setup_tool_logging(execname, hostname, username,
                                     logdir=parserOptions.ensure_value("logfileDirectory", None), nonuser=nonuser)

        if forceQuiet:
            gplog.quiet_stdout_logging()
        else:
            if parserOptions.ensure_value("verbose", False):
                gplog.enable_verbose_logging()
            if parserOptions.ensure_value("quiet", False):
                gplog.quiet_stdout_logging()

        if not suppressStartupLogMessage:
            logger.info("Starting %s with args: %s" % (gProgramName, ' '.join(sys.argv[1:])))

        commandObject = createCommandFn(parserOptions, parserArgs)
        exitCode = commandObject.run()
        exit_status = exitCode

    except ProgramArgumentValidationException as e:
        if e.shouldPrintHelp():
            parser.print_help()
        logger.error("%s: error: %s" % (gProgramName, e.getMessage()))
        exit_status = 2
    except ExceptionNoStackTraceNeeded as e:
        logger.error("%s error: %s" % (gProgramName, e))
        exit_status = 2
    except UserAbortedException as e:
        logger.info("User abort requested, Exiting...")
        exit_status = 4
    except ExecutionError as e:
        logger.fatal("Error occurred: %s\n Command was: '%s'\n"
                     "rc=%d, stdout='%s', stderr='%s'" % \
                     (e.summary, e.cmd.cmdStr, e.cmd.results.rc, e.cmd.results.stdout,
                      e.cmd.results.stderr))
        exit_status = 2
    except Exception as e:
        if parserOptions is None:
            logger.exception("%s failed.  exiting...", gProgramName)
        else:
            if parserOptions.ensure_value("verbose", False):
                logger.exception("%s failed.  exiting...", gProgramName)
            else:
                logger.fatal("%s failed. (Reason='%s') exiting..." % (gProgramName, e))
        exit_status = 2
    except KeyboardInterrupt:
        exit_status = 2
    finally:
        if commandObject:
            commandObject.cleanup()
    sys.exit(exit_status)


def addStandardLoggingAndHelpOptions(parser, includeNonInteractiveOption, includeUsageOption=False):
    """
    Add the standard options for help and logging
    to the specified parser object. Returns the logging OptionGroup so that
    callers may modify as needed.
    """
    parser.set_usage('%prog [--help] [options] ')
    parser.remove_option('-h')

    addTo = parser
    addTo.add_option('-h', '-?', '--help', action='help',
                     help='show this help message and exit')
    if includeUsageOption:
        parser.add_option('--usage', action="briefhelp")

    addTo = OptionGroup(parser, "Logging Options")
    parser.add_option_group(addTo)
    addTo.add_option('-v', '--verbose', action='store_true',
                     help='debug output.')
    addTo.add_option('-q', '--quiet', action='store_true',
                     help='suppress status messages')
    addTo.add_option("-l", None, dest="logfileDirectory", metavar="<directory>", type="string",
                     help="Logfile directory")

    if includeNonInteractiveOption:
        addTo.add_option('-a', dest="interactive", action='store_false', default=True,
                         help="quiet mode, do not require user input for confirmations")
    return addTo


def addCoordinatorDirectoryOptionForSingleClusterProgram(addTo):
    """
    Add the -d coordinator directory option to the specified parser object
    which is intended to provide the value of the coordinator data directory.

    For programs that operate on multiple clusters at once, this function/option
    is not appropriate.
    """
    addTo.add_option('-d', '--master_data_directory', '--coordinator_data_directory', type='string',
                     dest="coordinatorDataDirectory",
                     metavar="<coordinator data directory>",
                     help="Optional. The coordinator host data directory. If not specified, the value set" \
                          "for $COORDINATOR_DATA_DIRECTORY will be used.")

def parseStatusLine(line, isStart = False, isStop = False):
    """
    Function to parse status line of the result out, for gpstart and gpstop.
    Currently the parsing for both the utilities is implemented at two different places
    __processStartOrConvertCommands() and  _process_segment_stop()
    This is an attempt to consolidate
    """

    if isStart:
        tag = "STARTED:"
    elif isStop:
        tag = "STOPPED:"
    else:
        raise Exception("parseStatusLine: Invalid input")

    fields = line.split('--')
    index = 1
    started_tag_index = index + 1
    while not fields[started_tag_index].startswith(tag):
        started_tag_index += 1

    dir = "--".join(fields[index:started_tag_index])
    dir = dir.split(':')[1]

    index = started_tag_index
    started = fields[index].split(':')[1]
    index += 1

    if isStart:
        reasonCode = gp.SEGSTART_ERROR_UNKNOWN_ERROR
        if fields[index].startswith("REASONCODE:"):
            reasonCode = int(fields[index].split(":")[1])
            index += 1
    else:
        reasonCode = gp.SEGSTART_SUCCESS

    # The funny join and splits are because Reason could have colons or -- in the text itself
    reasonStr = "--".join(fields[index:])
    reasonArr = reasonStr.split(':')
    reasonArr = reasonArr[1:]
    reasonStr = ":".join(reasonArr)
    return reasonCode, reasonStr, started, dir
