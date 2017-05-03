"""
This file is a wrapper around figleaf and will start/stop coverage as
needed.  It also includes a method for generating the HTML reports.
"""

import os
import random
import figleaf
import pickle
from glob import glob
from gppylib import gplog
from gppylib.commands.base import Command, LOCAL, REMOTE, ExecutionContext, RemoteExecutionContext, WorkerPool
from gppylib.commands.unix import RemoveFiles, Scp
from gppylib.operations import Operation
from gppylib.operations.unix import ListFiles, ListRemoteFiles, MakeDir

logger = gplog.get_default_logger()
COVERAGE_FILENAME = 'cover.out'


# ------------------------------------------------------------------------------
class GpWriteFigleafCoverageHtml(Command):
    """Command to write out figleaf html reports to disk based on the
    coverage information that has been collected."""

    def __init__(self, name, filename, directory, ctxt=LOCAL, remoteHost=None):
        gphome = os.getenv("GPHOME", None)
        if not gphome:
            raise Exception('GPHOME environment variable not set.')
        cmdStr = "%s -d %s %s" % (os.path.normpath(gphome + '/lib/python/figleaf/figleaf2html'), directory, filename)
        Command.__init__(self, name, cmdStr, ctxt, remoteHost)

    @staticmethod
    def local(name, coverfile, directory):
        cmd = GpWriteFigleafCoverageHtml(name, coverfile, directory)
        cmd.run(validateAfter=True)


# ------------------------------------------------------------------------------
# TODO: We should not allow this class to be instantiated. It offers static
# functionality, and its exposed methods should reflect that.
class GpFigleafCoverage:
    """
    Distributed code coverage, built atop figleaf.

    Figleaf code coverage is a two-phase process: recording and reporting. Recording simply involves
    starting and stopping instrumentation. This results in a pickled data file in a designated location 
    on disk. (The distributed adaptation here of figleaf relies on this point.) Lastly, we invoke 
    figleaf2html via the Command above to produce html from the recorded data.

    Like figleaf, GpFigleafCoverage is a similar two-phase process: enable recording and enable reporting. 

    To enable recording, gppylib must be *reactive* to coverage requests; in other words, the entry points to gppylib
    must invoke GpFigleafCoverage. Currently, there are two such entry points: gppylib.mainUtils.simple_main and 
    sbin/gpoperation.py. Moreover, gppylib must be *proactive* to propagate requests to subprocesses or remote processes. 
    This is accomplished below by hooking gppylib.commands.base.ExecutionContext, and its inherited classes, in order
    to propagate a couple of key environment variables needed below: USE_FIGLEAF, FIGLEAF_DIR, and FIGLEAF_PID.

    To enable reporting, we must aggregate the data that the various python interpreters across subprocesses 
    and remote processes had generated. This Operaiton will rely on the knowledge of how figleaf resultant data is stored
    on disk. For more detail, see FinalizeCoverage below.

    It will help to explain how recording and reporting come together. GpFigleafCoverage recording is expected to produce,
    and its reporting is dependent upon, the following directory structure:

    <base>/*.out,*.html                 - Global coverage data, aggregated across multiple runs
    <base>/<pid>/*.out,*.html           - Coverage data pertaining to <pid>, where <pid> is the 
                                          process id of the originating python program, on the master
    <base>/<pid>/<comp>/*.out,*html     - Coverage data pertaining to some subprocess or remote process 
                                          that is invoked as a subcomponent of the overall program given by <pid>

    For clarity, the rest of the code will adopt the following coding convention:
    base_dir := <base>
    pid_dir := <base>/<pid>
    comp_dir := <base>/<pid>/<comp>
    """

    # TODO: change directory structure to something more human-readable
    # How about <base>/<program_name><pid>/<program_name><rand>/*.out,*.html ?

    def __init__(self):
        try:
            self.directory = os.getenv('FIGLEAF_DIR', None)
            if self.directory is None:
                self.directory = os.path.normpath(os.path.expanduser("~") + '/.figleaf')
            self.my_pid = str(os.getpid())
            self.main_pid = os.getenv('FIGLEAF_PID', self.my_pid)
            randstring = ''.join(random.choice('0123456789') for x in range(20))
            self.filename = os.path.join(self.directory, self.main_pid, randstring, COVERAGE_FILENAME)
            self.running = False
        except Exception, e:
            logger.exception('Error initializing code coverage')

    def start(self):
        """Starts coverage collection if the environment variable USE_FIGLEAF is set."""
        try:
            if os.getenv('USE_FIGLEAF', None):
                logger.info('Code coverage will be generated')
                MakeDir(os.path.dirname(self.filename)).run()
                self.running = True
                ExecutionContext.propagate_env_map.update({'FIGLEAF_DIR': os.getenv('FIGLEAF_DIR', self.directory),
                                                           'USE_FIGLEAF': 1,
                                                           'FIGLEAF_PID': self.main_pid})
                figleaf.start()
        except Exception, e:
            logger.error('Error starting code coverage: %s' % e)

    def stop(self):
        """Stops code coverage."""
        try:
            if self.running:
                logger.info('Stopping code coverage')
                figleaf.stop()
                figleaf.write_coverage(self.filename)
                self.running = False
                for k in ['FIGLEAF_DIR', 'USE_FIGLEAF', 'FIGLEAF_PID']:
                    del ExecutionContext.propagate_env_map[k]
        except Exception, e:
            logger.error('Error stopping code coverage: %s' % e)

    def generate_report(self):
        """Generates the html reports and puts them in the directory specified."""
        if os.getenv('USE_FIGLEAF', None):
            try:
                directory = os.path.dirname(self.filename)
                logger.info('Generating code coverage HTML reports to %s' % directory)
                GpWriteFigleafCoverageHtml.local('Generate HTML', self.filename, directory)

                if self.main_pid == self.my_pid:
                    FinalizeCoverage(trail=RemoteExecutionContext.trail,
                                     pid=self.main_pid,
                                     base_dir=self.directory).run()
            except Exception, e:
                logger.exception('Error generating HTML code cover reports.')

    def delete_files(self):
        """Deletes code coverage files."""
        if os.getenv('USE_FIGLEAF', None):
            logger.info('Deleting coverage files...')
            try:
                RemoveFiles.local('Remove coverage file', self.filename)
                directory = os.path.dirname(self.filename)
                RemoveFiles.local('Remove html files', directory + '/*.html')
            except:
                logger.error('Failed to clean up coverage files')


# The coverage tool to use
# if os.getenv('USE_FIGLEAF', None):
GP_COVERAGE_CLASS = GpFigleafCoverage


# else:
#    GP_COVERAGE_CLASS=<some other coverage class>


# ------------------------------------------------------------------------------
class GpCoverage(GP_COVERAGE_CLASS):
    """Class the controls code coverage.  Right now this inherits from
    GpFigleafCoverage, but in the future we may find a better code coverage
    tool and switch to that.  With this class, we can do that without 
    touching any of the management utilities or modules."""
    pass


# ------------------------------------------------------------------------------
class FinalizeCoverage(Operation):
    """
    This aggregates coverage data from across the cluster for this current process (which is soon to complete.)
    Then, we update the global coverage data that persists from run to run at <base_dir>/*.out,*.html.
    """

    def __init__(self, trail, pid, base_dir):
        self.trail = trail
        self.pid = pid
        self.base_dir = base_dir

    def execute(self):
        pid_dir = os.path.join(self.base_dir, self.pid)

        # update the pid-level coverage statistics, which reside within pid_dir
        # this requires: collect coverage data, merge data, save, and generate html
        CollectCoverage(trail=self.trail, pid_dir=pid_dir).run()
        partial_coverages = LoadPartialCoverages(pid_dir=pid_dir).run()
        cumulative_coverage = {}
        for partial_coverage in partial_coverages:
            MergeCoverage(input=partial_coverage, output=cumulative_coverage).run()
        SaveCoverage(obj=cumulative_coverage,
                     path=os.path.join(pid_dir, COVERAGE_FILENAME)).run()
        GpWriteFigleafCoverageHtml.local('Generate HTML', os.path.join(pid_dir, COVERAGE_FILENAME), pid_dir)

        # update the global coverage statistics, which reside within self.base_dir
        overall_coverage = LoadCoverage(os.path.join(self.base_dir, COVERAGE_FILENAME)).run()
        MergeCoverage(input=cumulative_coverage, output=overall_coverage).run()
        SaveCoverage(obj=overall_coverage,
                     path=os.path.join(self.base_dir, COVERAGE_FILENAME)).run()
        GpWriteFigleafCoverageHtml.local('Generate HTML', os.path.join(self.base_dir, COVERAGE_FILENAME), self.base_dir)

        # ------------------------------------------------------------------------------


class CollectCoverage(Operation):
    """ 
    Simply copy over <base>/<pid>/<comp> dirs back to the master. This may 
    be an unnecessary step IF <base> is an NFS mount. 
    """

    def __init__(self, trail, pid_dir):
        self.trail = trail
        self.pid_dir = pid_dir

    def execute(self):
        pool = WorkerPool()
        given = set(ListFiles(self.pid_dir).run())
        try:
            for host in self.trail:
                available = ListRemoteFiles(self.pid_dir, host).run()
                to_copy = [dir for dir in available if dir not in given]
                for dir in to_copy:
                    comp_dir = os.path.join(self.pid_dir, dir)
                    pool.addCommand(Scp('collect coverage',
                                        srcFile=comp_dir,
                                        srcHost=host,
                                        dstFile=comp_dir,
                                        recursive=True))
            pool.join()
        finally:
            pool.haltWork()


# ------------------------------------------------------------------------------
class LoadCoverage(Operation):
    """ Unpickles and returns an object residing at a current path """

    def __init__(self, path):
        self.path = path

    def execute(self):
        try:
            with open(self.path, 'r') as f:
                obj = pickle.load(f)
            return obj
        except (IOError, OSError):
            logger.exception('Failed to un-pickle coverage off disk.')
        return {}


# ------------------------------------------------------------------------------
class SaveCoverage(Operation):
    """ Pickles a given object to disk at a designated path """

    def __init__(self, path, obj):
        self.path = path
        self.obj = obj

    def execute(self):
        with open(self.path, 'w') as f:
            pickle.dump(self.obj, f)


# ------------------------------------------------------------------------------
class LoadPartialCoverages(Operation):
    """ Returns an array of unpickled coverage objects from <base>/<pid>/*/<COVERAGE_FILENAME> """

    def __init__(self, pid_dir):
        self.pid_dir = pid_dir

    def execute(self):
        coverage_files = glob(os.path.join(self.pid_dir, '*', COVERAGE_FILENAME))
        return [LoadCoverage(path).run() for path in coverage_files]


# ------------------------------------------------------------------------------
# TODO: Support a parallel merge? Or would there be no point with the Python GIL?
class MergeCoverage(Operation):
    """
    Figleaf coverage data is pickled on disk as a dict of filenames to sets of numbers,
    where each number denotes a covered line number.
    e.g.    {   "gparray.py"    :   set(0, 1, 2, ...),
                "operations/dump.py"    :   set(175, 13, 208, ...),
                ... }
    Here, we merge such an input dict into an output dict. As such, we'll be able to pickle
    the result back to disk and invoke figleaf2html to get consolidated html reports.
    """

    def __init__(self, input, output):
        self.input, self.output = input, output

    def execute(self):
        for filename in self.input:
            if filename not in self.output:
                self.output[filename] = self.input[filename]
            else:
                self.output[filename] |= self.input[filename]  # set union
