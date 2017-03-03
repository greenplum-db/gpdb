# Line too long - pylint: disable=C0301
# Invalid name  - pylint: disable=C0103

"""
mainUtils.py
------------

This file provides a rudimentary framework to support top-level option
parsing, initialization and cleanup logic common to multiple programs.

It also implements workarounds to make other modules we use like
GpCoverage() work properly.

The primary interface function is 'simple_main'.  For an example of
how it is expected to be used, see gprecoverseg.

It is anticipated that the functionality of this file will grow as we
extend common functions of our gp utilities.  Please keep this in mind
and try to avoid placing logic for a specific utility here.
"""

import os, sys, signal, errno, yaml

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
from gppylib.gpcoverage import GpCoverage
from lockfile.pidlockfile import PIDLockFile, LockTimeout


def getProgramName():
    """
    Return the name of the current top-level program from sys.argv[0]
    or the programNameOverride option passed to simple_main via mainOptions.
    """
    global gProgramName
    return gProgramName


class SimpleMainLock:
    """
    Tools like gprecoverseg prohibit running multiple instances at the same time
    via a simple lock file created in the MASTER_DATA_DIRECTORY.  This class takes
    care of the work to manage this lock as appropriate based on the mainOptions
    specified.

    Note that in some cases, the utility may want to recursively invoke
    itself (e.g. gprecoverseg -r).  To handle this, the caller may specify
    the name of an environment variable holding the pid already acquired by
    the parent process.
    """
    def __init__(self, mainOptions):
        self.pidfilename   = mainOptions.get('pidfilename', None)       # the file we're using for locking
        self.parentpidvar  = mainOptions.get('parentpidvar', None)      # environment variable holding parent pid
        self.parentpid     = None                                       # parent pid which already has the lock
        self.ppath         = None                                       # complete path to the lock file
        self.pidlockfile   = None                                       # PIDLockFile object
        self.pidfilepid    = None                                       # pid of the process which has the lock
        self.locktorelease = None                                       # PIDLockFile object we should release when done

        if self.parentpidvar is not None and self.parentpidvar in os.environ:
            self.parentpid = int(os.environ[self.parentpidvar])

        if self.pidfilename is not None:
            self.ppath       = os.path.join(gp.get_masterdatadir(), self.pidfilename)
            self.pidlockfile = PIDLockFile( self.ppath )


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
        self.pidfilepid = self.pidlockfile.read_pid()
        if self.pidfilepid is not None:

            # we found a lock file
            # allow the process to proceed if the locker was our parent
            if self.pidfilepid == self.parentpid:
                return None

            # cleanup stale locks
            try:
                os.kill(self.pidfilepid, signal.SIG_DFL)
            except OSError, exc:
                if exc.errno == errno.ESRCH:
                    self.pidlockfile.break_lock()
                    self.pidfilepid = None

        # try and acquire the lock
        try:
            self.pidlockfile.acquire(1)

        except LockTimeout:
            self.pidfilepid = self.pidlockfile.read_pid()
            return self.pidfilepid

        # we have the lock
        # prepare for a later call to release() and take good
        # care of the process environment for the sake of our children
        self.locktorelease = self.pidlockfile
        self.pidfilepid    = self.pidlockfile.read_pid()
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


def simple_main( createOptionParserFn, createCommandFn, mainOptions=None) :
    """
     createOptionParserFn : a function that takes no arguments and returns an OptParser
     createCommandFn : a function that takes two argument (the options and the args (those that are not processed into
                       options) and returns an object that has "run" and "cleanup" functions.  Its "run" function must
                       run and return an exit code.  "cleanup" will be called to clean up before the program exits;
                       this can be used to clean up, for example, to clean up a worker pool

     mainOptions can include: forceQuietOutput (map to bool),
                              programNameOverride (map to string)
                              suppressStartupLogMessage (map to bool)
                              useHelperToolLogging (map to bool)
                              setNonuserOnToolLogger (map to bool, defaults to false)
                              pidfilename (string)
                              parentpidvar (string)

    """
    coverage = GpCoverage()
    coverage.start()
    try:
        simple_main_internal(createOptionParserFn, createCommandFn, mainOptions)
    finally:
        coverage.stop()
        coverage.generate_report()


def simple_main_internal(createOptionParserFn, createCommandFn, mainOptions):
    """
    If caller specifies 'pidfilename' in mainOptions then we manage the
    specified pid file within the MASTER_DATA_DIRECTORY before proceeding
    to execute the specified program and we clean up the pid file when
    we're done.
    """
    sml = None
    if mainOptions is not None and 'pidfilename' in mainOptions:
        sml      = SimpleMainLock(mainOptions)
        otherpid = sml.acquire()
        if otherpid is not None:
            logger = gplog.get_default_logger()
            logger.error("An instance of %s is already running (pid %s)" % (getProgramName(), otherpid))
            return

    # at this point we have whatever lock we require
    try:
        simple_main_locked(createOptionParserFn, createCommandFn, mainOptions)
    finally:
        if sml is not None:
            sml.release()


def simple_main_locked(createOptionParserFn, createCommandFn, mainOptions):
    """
    Not to be called externally -- use simple_main instead
    """
    logger = gplog.get_default_logger()

    configurationInterface.registerConfigurationProvider( configurationImplGpdb.GpConfigurationProviderUsingGpdbCatalog())
    fileSystemInterface.registerFileSystemProvider( fileSystemImplOs.GpFileSystemProviderUsingOs())
    osInterface.registerOsProvider( osImplNative.GpOsProviderUsingNative())
    faultProberInterface.registerFaultProber( faultProberImplGpdb.GpFaultProberImplGpdb())

    commandObject = None
    parser = None

    forceQuiet = mainOptions is not None and mainOptions.get("forceQuietOutput")
    options = None

    if mainOptions is not None and mainOptions.get("programNameOverride"):
        global gProgramName
        gProgramName = mainOptions.get("programNameOverride")
    suppressStartupLogMessage = mainOptions is not None and mainOptions.get("suppressStartupLogMessage")

    useHelperToolLogging = mainOptions is not None and mainOptions.get("useHelperToolLogging")
    nonuser = True if mainOptions is not None and mainOptions.get("setNonuserOnToolLogger") else False
    exit_status = 1
    
    # NOTE: if this logic is changed then also change test_main in testUtils.py
    try:
        execname = getProgramName()
        hostname = unix.getLocalHostname()
        username = unix.getUserName()

        parser = createOptionParserFn()
        (options, args) = parser.parse_args()

        if useHelperToolLogging:
            gplog.setup_helper_tool_logging(execname, hostname, username)
        else:
            gplog.setup_tool_logging(execname, hostname, username,
                                        logdir=options.ensure_value("logfileDirectory", None), nonuser=nonuser )

        if forceQuiet:
            gplog.quiet_stdout_logging()
        else:
            if options.ensure_value("verbose", False):
                gplog.enable_verbose_logging()
            if options.ensure_value("quiet", False):
                gplog.quiet_stdout_logging()

        if options.ensure_value("masterDataDirectory", None) is not None:
            options.master_data_directory = os.path.abspath(options.masterDataDirectory)

        if not suppressStartupLogMessage:
            logger.info("Starting %s with args: %s" % (gProgramName, ' '.join(sys.argv[1:])))

        commandObject = createCommandFn(options, args)
        exitCode = commandObject.run()
        exit_status = exitCode

    except ProgramArgumentValidationException, e:
        if e.shouldPrintHelp():
            parser.print_help()
        logger.error("%s: error: %s" %(gProgramName, e.getMessage()))
        exit_status = 2
    except ExceptionNoStackTraceNeeded, e:
        logger.error( "%s error: %s" % (gProgramName, e))
        exit_status = 2
    except UserAbortedException, e:
        logger.info("User abort requested, Exiting...")
        exit_status = 4
    except ExecutionError, e:
        logger.fatal("Error occurred: %s\n Command was: '%s'\n"
                     "rc=%d, stdout='%s', stderr='%s'" %\
                     (e.summary,e.cmd.cmdStr, e.cmd.results.rc, e.cmd.results.stdout,
                     e.cmd.results.stderr ))
        exit_status = 2
    except Exception, e:
        if options is None:
            logger.exception("%s failed.  exiting...", gProgramName)
        else:
            if options.ensure_value("verbose", False):
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
    to the specified parser object.
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
        addTo.add_option('-a', dest="interactive" , action='store_false', default=True,
                    help="quiet mode, do not require user input for confirmations")


def addMasterDirectoryOptionForSingleClusterProgram(addTo):
    """
    Add the -d master directory option to the specified parser object
    which is intended to provide the value of the master data directory.

    For programs that operate on multiple clusters at once, this function/option
    is not appropriate.
    """
    addTo.add_option('-d', '--master_data_directory', type='string',
                        dest="masterDataDirectory",
                        metavar="<master data directory>",
                        help="Optional. The master host data directory. If not specified, the value set"\
                            "for $MASTER_DATA_DIRECTORY will be used.")
    


#
# YamlMain
# 

def get_yaml(targetclass):
    "get_yaml"

    # doc    - class's doc string
    # pos    - where YAML starts in doc
    # ystr   - YAML string extracted from doc

    if not hasattr(targetclass, '_yaml') or targetclass._yaml is None:
        doc = targetclass.__doc__
        pos = doc.find('%YAML')
        assert pos >= 0, "targetclass doc string is missing %YAML plan"
        ystr  = doc[pos:].replace('\n    ','\n')
        targetclass._yaml = yaml.load(ystr)
    return targetclass._yaml


class YamlMain:
    "YamlMain"

    def __init__(self):
        "Parse arguments based on yaml docstring"
        self.current       = None
        self.plan          = None
        self.scenario_name = None
        self.logger        = None
        self.logfilename   = None
        self.errmsg        = None

        self.parser = YamlOptions(self).parser
        self.options, self.args = self.parser.parse_args()
        self.options.quiet = self.options.q
        self.options.verbose = self.options.v


    #
    # simple_main interface
    #
    def __call__(self, *args):
        "Allows us to use self as the create_parser and create_program functions in call to simple_main"
        return self

    def parse_args(self):
        "Called by simple_main to obtain results from parser returned by create_parser"
        return self.options, self.args

    def run(self):
        "Called by simple_main to execute the program returned by create_program"
        self.plan = Plan(self)
        self.scenario_name = self.plan.name
        self.logger        = self.plan.logger
        self.logfilename   = self.plan.logfilename
        self.errmsg        = self.plan.errmsg
        self.current       = []
        self.plan.run()

    def cleanup(self):
        "Called by simple_main to cleanup after program returned by create_program finishes"
        pass

    def simple(self):
        "Delegates setup and control to mainUtils.simple_main"
        simple_main(self, self)


#
# option parsing
#

class YamlOptions:
    "YamlOptions"

    def __init__(self, target):
        """
        Scan the class doc string of the given object, looking for the %YAML
        containing the option specification.  Parse the YAML and setup the
        corresponding OptionParser object.
        """
        # target - options object (input)
        # gname  - option group name

        self.y      = get_yaml(target.__class__)
        self.parser = OptionParser( description=self.y['Description'], version='%prog version $Revision$')
        self.parser.remove_option('-h')
        self.parser.set_usage(self.y['Usage'])
        self.opty   = self.y['Options']
        for gname in self.opty.get('Groups', []):
            self._register_group(gname)
            

    def _register_group(self, gname):
        """
        Register options for the specified option group name to the OptionParser
        using an OptionGroup unless the group name starts with 'Help' in which
        case we just register the options with the top level OptionParser object.
        """
        # gname    - option group name (input)
        # gy       - option group YAML object
        # grp      - option group object
        # tgt      - where to add options (parser or option group)
        # optkey   - comma separated list of option flags
        # optval   - help string or dict with detailed option settings
        # listargs - list of option flags (e.g. ['-h', '--help'])
        # dictargs - key/value arguments to add_option

        gy = self.opty.get(gname, None)
        if gname.startswith('Help'): 
            grp = None
            tgt = self.parser
        else:
            grp = OptionGroup(self.parser, gname)
            tgt = grp

        for optkey, optval in gy.items():
            listargs = optkey.split(',')
            if type(optval) == type(''):
                # short form: optval is just a help string
                dictargs = {
                    'action': 'store_true',
                    'help':   optval
                }
            else:
                # optval is the complete option specification
                dictargs = optval

            # hide hidden options
            if dictargs.get('help','').startswith('hidden'):
                dictargs['help'] = SUPPRESS_HELP

            #print 'adding', listargs, dictargs
            tgt.add_option(*listargs, **dictargs)

        if grp is not None:
            self.parser.add_option_group(grp)



#
# plan execution
#

class Task:
    "Task"

    def __init__(self, key, name, subtasks=None):
        self.Key      = key    		# task key
        self.Name     = name    	# task name
        self.SubTasks = subtasks	# subtasks, if any
        self.Func     = None            # task function, set by _task


    def _print(self, main, prefix):
        print '%s %s %s:' % (prefix, self.Key, self.Name)

    def _debug(self, main, prefix):
        main.logger.debug('Execution Plan:%s %s %s%s' % (prefix, self.Key, self.Name, ':' if self.SubTasks else ''))

    def _run(self, main, prefix):
        main.logger.debug(' Now Executing:%s %s %s' % (prefix, self.Key, self.Name))
        if self.Func:
            self.Func()
        

class Exit(Exception):
    def __init__(self, rc, code=None, call_support=False):
        Exception.__init__(self)
        self.code         = code
        self.prm          = sys._getframe(1).f_locals
        self.rc           = rc
        self.call_support = call_support


class Plan:
    "Plan"

    def __init__(self, main):
        """
        Create cached yaml from class doc string of the given object, 
        looking for the %YAML indicating the beginning of the object's YAML plan and parse it.
        Build the plan stages and tasks for the specified scenario.
        """
        # main - object with yaml scenarios (input)
        # sy   - Stage yaml

        self.logger      = gplog.get_default_logger()
        self.logfilename = gplog.get_logfile()

        self.main        = main
        self.y           = get_yaml(main.__class__)
        self.name        = main.options.scenario
        if not self.name:
            self.name    = self.y['Default Scenario']
        self.scenario    = self.y['Scenarios'][self.name]
        self.errors      = self.y['Errors']
        self.Tasks       = [ self._task(ty) for ty in self.scenario ]


    def _task(self, ty):
        "Invoked by __init__ to build a top-level task from the YAML"

        # ty   - Task yaml (input)
        # tyk  - Task yaml key
        # tyv  - Task yaml value
        # sty  - Sub Task yaml
        # t    - Task (returned)

        for tyk, tyv in ty.items():
            key, workers = tyk.split(None, 1)
            subtasks = [ self._subtask(sty) for sty in tyv ]
            t = Task(key, workers, subtasks)
            return t

    def _subtask(self, sty):
        "Invoked by _stage to build a task from the YAML"

        # sty  - Sub Task yaml (input)
        # st   - Sub Task (returned)

        key, rest = sty.split(None, 1)
        st = Task(key, rest)
        fn = st.Name.lower().replace(' ','_')
        try:
            st.Func = getattr(self.main, fn)
        except AttributeError, e:
            raise Exception("Failed to lookup '%s' for sub task '%s': %s" % (fn, st.Name, str(e)))
        return st



    def _dotasks(self, subtasks, prefix, action):
        "Apply an action to each subtask recursively"

        # st   - Sub Task

        for st in subtasks or []:
            self.main.current.append(st)
            action(st, self.main, prefix)
            self._dotasks(st.SubTasks, '  '+prefix, action)
            self.main.current.pop()


    def _print(self):
        "Print in YAML form."

        print '%s:' % self.name
        self._dotasks(self.Tasks, ' -', lambda t,m,p:t._print(m,p))


    def run(self):
        "Run the stages and tasks."

        self.logger.debug('Execution Plan: %s' % self.name)
        self._dotasks(self.Tasks, ' -', lambda t,m,p:t._debug(m,p))
                
        self.logger.debug(' Now Executing: %s' % self.name)
        try:
            self._dotasks(self.Tasks, ' -', lambda t,m,p:t._run(m,p))
        except Exit, e:
            self.exit(e.code, e.prm, e.rc, e.call_support)


    def errmsg(self, code, prm={}):
        "Return a formatted error message"
        return self.errors[code] % prm
        

    def exit(self, code=None, prm={}, rc=1, call_support=False):
        "Terminate the application"
        if code:
            msg = self.errmsg(code, prm)
            self.logger.error(msg)
        if call_support:
            self.logger.error('Please send %s to Greenplum support.' % self.logfilename)
        self.logger.debug('exiting with status %(rc)s' % locals())
        sys.exit(rc)

