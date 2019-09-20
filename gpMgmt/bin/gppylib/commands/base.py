#!/usr/bin/env python
#
# Copyright (c) Greenplum Inc 2008. All Rights Reserved.
#
"""
base.py

common base for the commands execution framework.  Units of work are defined as Operations
as found in other modules like unix.py.  These units of work are then packaged up and executed
within a GpCommand.  A GpCommand is just a common infrastructure for executing an Operation.

The general idea is that the application developer breaks the problem down into a set of
GpCommands that need to be executed.  This class also provides a queue and set of workers
for executing this set of commands.


"""

from Queue import Queue, Empty
from threading import Thread

import os
import signal
try:
    import subprocess32 as subprocess
except:
    import subprocess
import sys
import time

from gppylib import gplog
from gppylib import gpsubprocess
from pygresql.pg import DB

logger = gplog.get_default_logger()

GPHOME = os.environ.get('GPHOME')

# Maximum retries if sshd rejects the connection due to too many
# unauthenticated connections.
SSH_MAX_RETRY = 10
# Delay before retrying ssh connection, in seconds
SSH_RETRY_DELAY = .5


class WorkerPool(object):
    """TODO:"""

    halt_command = 'halt command'

    def __init__(self, numWorkers=16, items=None, daemonize=False, logger=gplog.get_default_logger()):
        if numWorkers <= 0:
            raise Exception("WorkerPool(): numWorkers should be greater than 0.")
        self.workers = []
        self.should_stop = False
        self.work_queue = Queue()
        self.completed_queue = Queue()
        self._assigned = 0
        self.daemonize = daemonize
        self.logger = logger

        if items is not None:
            for item in items:
                self.addCommand(item)

        for i in range(0, numWorkers):
            w = Worker("worker%d" % i, self)
            self.workers.append(w)
            w.start()
        self.numWorkers = numWorkers

    ###
    def getNumWorkers(self):
        return self.numWorkers

    def getNextWorkItem(self):
        return self.work_queue.get(block=True)

    def addFinishedWorkItem(self, command):
        self.completed_queue.put(command)
        self.work_queue.task_done()

    def markTaskDone(self):
        self.work_queue.task_done()

    def addCommand(self, cmd):
        self.logger.debug("Adding cmd to work_queue: %s" % cmd.cmdStr)
        self.work_queue.put(cmd)
        self._assigned += 1

    def _join_work_queue_with_timeout(self, timeout):
        """
        Queue.join() unfortunately doesn't take a timeout (see
        https://bugs.python.org/issue9634). Fake it here, with a solution
        inspired by notes on that bug report.

        XXX This solution uses undocumented Queue internals (though they are not
        underscore-prefixed...).
        """
        done_condition = self.work_queue.all_tasks_done
        done_condition.acquire()
        try:
            while self.work_queue.unfinished_tasks:
                if (timeout <= 0):
                    # Timed out.
                    return False

                start_time = time.time()
                done_condition.wait(timeout)
                timeout -= (time.time() - start_time)
        finally:
            done_condition.release()

        return True

    def join(self, timeout=None):
        """
        Waits (up to an optional timeout) for the worker queue to be fully
        completed, and returns True if the pool is now done with its work.

        A None timeout indicates that join() should wait forever; the return
        value is always True in this case. Zero and negative timeouts indicate
        that join() will query the queue status and return immediately, whether
        the queue is done or not.
        """
        if timeout is None:
            self.work_queue.join()
            return True

        return self._join_work_queue_with_timeout(timeout)

    def joinWorkers(self):
        for w in self.workers:
            w.join()

    def _pop_completed(self):
        """
        Pops an item off the completed queue and decrements the assigned count.
        If the queue is empty, throws Queue.Empty.
        """
        item = self.completed_queue.get(False)
        self._assigned -= 1
        return item

    def getCompletedItems(self):
        completed_list = []
        try:
            while True:
                item = self._pop_completed() # will throw Empty
                if item is not None:
                    completed_list.append(item)
        except Empty:
            return completed_list

    def check_results(self):
        """ goes through all items in the completed_queue and throws an exception at the
            first one that didn't execute successfully

            throws ExecutionError
        """
        try:
            while True:
                item = self._pop_completed() # will throw Empty
                if not item.get_results().wasSuccessful():
                    raise ExecutionError("Error Executing Command: ", item)
        except Empty:
            return

    def empty_completed_items(self):
        while not self.completed_queue.empty():
            self._pop_completed()

    def isDone(self):
        # TODO: not sure that qsize() is safe
        return (self.assigned == self.completed_queue.qsize())

    @property
    def assigned(self):
        """
        A read-only count of the number of commands that have been added to the
        pool. This count is only decremented when items are removed from the
        completed queue via getCompletedItems(), empty_completed_items(), or
        check_results().
        """
        return self._assigned

    @property
    def completed(self):
        """
        A read-only count of the items in the completed queue. Will be reset to
        zero after a call to empty_completed_items() or getCompletedItems().
        """
        return self.completed_queue.qsize()

    def haltWork(self):
        self.logger.debug("WorkerPool haltWork()")
        self.should_stop = True
        for w in self.workers:
            w.haltWork()
            self.work_queue.put(self.halt_command)


def join_and_indicate_progress(pool, outfile=sys.stdout, interval=1):
    """
    Waits for a WorkerPool to complete its work, flushing dots to stdout every
    second. If any dots are printed (i.e. the work takes longer than the
    printing interval), a newline is also printed upon completion.

    The file to print to and the interval between printings can be overridden.
    """
    printed = False

    while not pool.join(interval):
        outfile.write('.')
        outfile.flush()
        printed = True

    if printed:
        outfile.write('\n')


class OperationWorkerPool(WorkerPool):
    """ TODO: This is a hack! In reality, the WorkerPool should work with Operations, and
        Command should be a subclass of Operation. Till then, we'll spoof the necessary Command
        functionality within Operation. """

    def __init__(self, numWorkers=16, operations=None):
        if operations is not None:
            for operation in operations:
                self._spoof_operation(operation)
        super(OperationWorkerPool, self).__init__(numWorkers, operations)

    def check_results(self):
        raise NotImplementedError("OperationWorkerPool has no means of verifying success.")

    def _spoof_operation(self, operation):
        operation.cmdStr = str(operation)


class Worker(Thread):
    """TODO:"""
    pool = None
    cmd = None
    name = None
    logger = None

    def __init__(self, name, pool):
        self.name = name
        self.pool = pool
        self.logger = logger
        Thread.__init__(self)
        self.daemon = pool.daemonize

    def run(self):
        while True:
            try:
                try:
                    self.cmd = self.pool.getNextWorkItem()
                except TypeError:
                    # misleading exception raised during interpreter shutdown
                    return

                # we must have got a command to run here
                if self.cmd is None:
                    self.logger.debug("[%s] got a None cmd" % self.name)
                    self.pool.markTaskDone()
                elif self.cmd is self.pool.halt_command:
                    self.logger.debug("[%s] got a halt cmd" % self.name)
                    self.pool.markTaskDone()
                    self.cmd = None
                    return
                elif self.pool.should_stop:
                    self.logger.debug("[%s] got cmd and pool is stopped: %s" % (self.name, self.cmd))
                    self.pool.markTaskDone()
                    self.cmd = None
                else:
                    self.logger.debug("[%s] got cmd: %s" % (self.name, self.cmd.cmdStr))
                    self.cmd.run()
                    self.logger.debug("[%s] finished cmd: %s" % (self.name, self.cmd))
                    self.pool.addFinishedWorkItem(self.cmd)
                    self.cmd = None

            except Exception, e:
                self.logger.exception(e)
                if self.cmd:
                    self.logger.debug("[%s] finished cmd with exception: %s" % (self.name, self.cmd))
                    self.pool.addFinishedWorkItem(self.cmd)
                    self.cmd = None

    def haltWork(self):
        self.logger.debug("[%s] haltWork" % self.name)

        # this was originally coded as
        #
        #    if self.cmd is not None:
        #        self.cmd.interrupt()
        #        self.cmd.cancel()
        #
        # but as observed in MPP-13808, the worker thread's run() loop may set self.cmd to None
        # past the point where the calling thread checks self.cmd for None, leading to a curious
        # "'NoneType' object has no attribute 'cancel' exception" which may prevent the worker pool's
        # haltWorkers() from actually halting all the workers.
        #
        c = self.cmd
        if c is not None and isinstance(c, Command):
            c.interrupt()
            c.cancel()


"""
TODO: consider just having a single interface that needs to be implemented for
      describing work to allow the Workers to use it.  This would allow the user
      to better provide logic necessary.  i.e.  even though the user wants to
      execute a unix command... how the results are interpretted are highly
      application specific.   So we should have a separate level of abstraction
      for executing UnixCommands and DatabaseCommands from this one.

      other things to think about:
      -- how to support cancel
      -- how to support progress
      -- undo?
      -- blocking vs. unblocking

"""


# --------------------------------NEW WORLD-----------------------------------

class CommandResult():
    """ Used as a way to package up the results from a GpCommand

    """

    # rc,stdout,stderr,completed,halt

    def __init__(self, rc, stdout, stderr, completed, halt):
        self.rc = rc
        self.stdout = stdout
        self.stderr = stderr
        self.completed = completed
        self.halt = halt

    def printResult(self):
        res = "cmd had rc=%d completed=%s halted=%s\n  stdout='%s'\n  " \
              "stderr='%s'" % (self.rc, str(self.completed), str(self.halt), self.stdout, self.stderr)
        return res

    def wasSuccessful(self):
        if self.halt:
            return False
        if not self.completed:
            return False
        if self.rc != 0:
            return False
        return True

    def __str__(self):
        return self.printResult()

    def split_stdout(self, how=':'):
        """
        TODO: AK: This doesn't belong here if it pertains only to pg_controldata.

        MPP-16318: Skip over discrepancies in the pg_controldata stdout, as it's
        not this code's responsibility to judge the pg_controldata stdout. This is
        especially true for 'immediate' shutdown, in which case, we won't even
        care for WARNINGs or other pg_controldata discrepancies.
        """
        for line in self.stdout.split('\n'):
            ret = line.split(how, 1)
            if len(ret) == 2:
                yield ret


class ExecutionError(Exception):
    def __init__(self, summary, cmd):
        self.summary = summary
        self.cmd = cmd

    def __str__(self):
        # TODO: improve dumping of self.cmd
        return "ExecutionError: '%s' occured.  Details: '%s'  %s" % \
               (self.summary, self.cmd.cmdStr, self.cmd.get_results().printResult())


# specify types of execution contexts.
LOCAL = 1
REMOTE = 2

gExecutionContextFactory = None


#
# @param factory needs to have a createExecutionContext(self, execution_context_id, remoteHost, stdin) function
#
def setExecutionContextFactory(factory):
    global gExecutionContextFactory
    gExecutionContextFactory = factory


def createExecutionContext(execution_context_id, remoteHost, stdin, gphome=None):
    if gExecutionContextFactory is not None:
        return gExecutionContextFactory.createExecutionContext(execution_context_id, remoteHost, stdin)
    elif execution_context_id == LOCAL:
        return LocalExecutionContext(stdin)
    elif execution_context_id == REMOTE:
        if remoteHost is None:
            raise Exception("Programmer Error.  Specified REMOTE execution context but didn't provide a remoteHost")
        return RemoteExecutionContext(remoteHost, stdin, gphome)


class ExecutionContext():
    """ An ExecutionContext defines where and how to execute the Command and how to
    gather up information that are the results of the command.

    """

    def __init__(self):
        pass

    def execute(self, cmd):
        pass

    def interrupt(self):
        pass

    def cancel(self):
        pass


class LocalExecutionContext(ExecutionContext):
    proc = None
    halt = False
    completed = False

    def __init__(self, stdin):
        ExecutionContext.__init__(self)
        self.stdin = stdin
        pass

    def execute(self, cmd, wait=True):
        # prepend env. variables from ExcecutionContext.propagate_env_map
        # e.g. Given {'FOO': 1, 'BAR': 2}, we'll produce "FOO=1 BAR=2 ..."

        # also propagate env from command instance specific map
        keys = sorted(cmd.propagate_env_map.keys(), reverse=True)
        for k in keys:
            cmd.cmdStr = "%s=%s && %s" % (k, cmd.propagate_env_map[k], cmd.cmdStr)

        # executable='/bin/bash' is to ensure the shell is bash.  bash isn't the
        # actual command executed, but the shell that command string runs under.
        self.proc = gpsubprocess.Popen(cmd.cmdStr, env=None, shell=True,
                                       executable='/bin/bash',
                                       stdin=subprocess.PIPE,
                                       stderr=subprocess.PIPE,
                                       stdout=subprocess.PIPE, close_fds=True)
        cmd.pid = self.proc.pid
        if wait:
            (rc, stdout_value, stderr_value) = self.proc.communicate2(input=self.stdin)
            self.completed = True
            cmd.set_results(CommandResult(
                rc, "".join(stdout_value), "".join(stderr_value), self.completed, self.halt))

    def cancel(self):
        if self.proc:
            try:
                os.kill(self.proc.pid, signal.SIGTERM)
            except OSError:
                pass

    def interrupt(self):
        self.halt = True
        if self.proc:
            self.proc.cancel()


class RemoteExecutionContext(LocalExecutionContext):
    trail = set()
    """
    Leaves a trail of hosts to which we've ssh'ed, during the life of a particular interpreter.
    """

    def __init__(self, targetHost, stdin, gphome=None):
        LocalExecutionContext.__init__(self, stdin)
        self.targetHost = targetHost
        if gphome:
            self.gphome = gphome
        else:
            self.gphome = GPHOME

    def execute(self, cmd):
        # prepend env. variables from ExcecutionContext.propagate_env_map
        # e.g. Given {'FOO': 1, 'BAR': 2}, we'll produce "FOO=1 BAR=2 ..."
        self.__class__.trail.add(self.targetHost)

        # also propagate env from command instance specific map
        keys = sorted(cmd.propagate_env_map.keys(), reverse=True)
        for k in keys:
            cmd.cmdStr = "%s=%s && %s" % (k, cmd.propagate_env_map[k], cmd.cmdStr)

        # Escape " for remote execution otherwise it interferes with ssh
        cmd.cmdStr = cmd.cmdStr.replace('"', '\\"')
        cmd.cmdStr = "ssh -o StrictHostKeyChecking=no -o ServerAliveInterval=60 " \
                     "{targethost} \"{gphome} {cmdstr}\"".format(targethost=self.targetHost,
                                                                 gphome=". %s/greenplum_path.sh;" % self.gphome,
                                                                 cmdstr=cmd.cmdStr)
        LocalExecutionContext.execute(self, cmd)
        if (cmd.get_results().stderr.startswith('ssh_exchange_identification: Connection closed by remote host')):
            self.__retry(cmd)
        pass

    def __retry(self, cmd, count=0):
        if count == SSH_MAX_RETRY:
            return
        time.sleep(SSH_RETRY_DELAY)
        LocalExecutionContext.execute(self, cmd)
        if (cmd.get_results().stderr.startswith('ssh_exchange_identification: Connection closed by remote host')):
            self.__retry(cmd, count + 1)

class Command(object):
    """ TODO:
    """
    name = None
    cmdStr = None
    results = None
    exec_context = None
    propagate_env_map = {}  # specific environment variables for this command instance

    def __init__(self, name, cmdStr, ctxt=LOCAL, remoteHost=None, stdin=None, gphome=None):
        self.name = name
        self.cmdStr = cmdStr
        self.exec_context = createExecutionContext(ctxt, remoteHost, stdin=stdin,
                                                   gphome=gphome)
        self.remoteHost = remoteHost
        self.logger = gplog.get_default_logger()

    def __str__(self):
        if self.results:
            return "%s cmdStr='%s'  had result: %s" % (self.name, self.cmdStr, self.results)
        else:
            return "%s cmdStr='%s'" % (self.name, self.cmdStr)

    # Start a process that will execute the command but don't wait for
    # it to complete.  Return the Popen object instead.
    def runNoWait(self):
        faultPoint = os.getenv('GP_COMMAND_FAULT_POINT')
        if not faultPoint or (self.name and not self.name.startswith(faultPoint)):
            self.exec_context.execute(self, wait=False)
            return self.exec_context.proc

    def run(self, validateAfter=False):
        self.logger.debug("Running Command: %s" % self.cmdStr)
        faultPoint = os.getenv('GP_COMMAND_FAULT_POINT')
        if not faultPoint or (self.name and not self.name.startswith(faultPoint)):
            self.exec_context.execute(self)
        else:
            # simulate error
            self.results = CommandResult(1, 'Fault Injection', 'Fault Injection', False, True)

        if validateAfter:
            self.validate()
        pass

    def set_results(self, results):
        self.results = results

    def get_results(self):
        return self.results

    def get_stdout(self, strip=True):
        if self.results is None:
            raise Exception("command not run yet")
        return self.results.stdout if not strip else self.results.stdout.strip()

    def get_stdout_lines(self):
        return self.results.stdout.splitlines()

    def get_stderr_lines(self):
        return self.results.stderr.splitlines()

    def get_return_code(self):
        if self.results is None:
            raise Exception("command not run yet")
        return self.results.rc

    def get_stderr(self):
        if self.results is None:
            raise Exception("command not run yet")
        return self.results.stderr

    def cancel(self):
        if self.exec_context and isinstance(self.exec_context, ExecutionContext):
            self.exec_context.cancel()

    def interrupt(self):
        if self.exec_context and isinstance(self.exec_context, ExecutionContext):
            self.exec_context.interrupt()

    def was_successful(self):
        if self.results is None:
            return False
        else:
            return self.results.wasSuccessful()

    def validate(self, expected_rc=0):
        """Plain vanilla validation which expects a 0 return code."""
        if self.results.rc != expected_rc:
            self.logger.debug(self.results)
            raise ExecutionError("non-zero rc: %d" % self.results.rc, self)


class SQLCommand(Command):
    """Base class for commands that execute SQL statements.  Classes
    that inherit from SQLCOmmand should set cancel_conn to the pygresql
    connection they wish to cancel and check self.cancel_flag."""

    def __init__(self, name):
        Command.__init__(self, name, cmdStr=None)
        self.cancel_flag = False
        self.cancel_conn = None

    def run(self, validateAfter=False):
        raise ExecutionError("programmer error.  implementors of SQLCommand must implement run()", self)

    def interrupt(self):
        # No execution context for SQLCommands
        pass

    def cancel(self):
        # assignment is an atomic operation in python
        self.cancel_flag = True

        # if self.conn is not set we cannot cancel.
        if self.cancel_conn:
            DB(self.cancel_conn).cancel()


def run_remote_commands(name, commands):
    """
    """
    cmds = {}
    pool = WorkerPool()
    for host, cmdStr in commands.items():
        cmd = Command(name=name, cmdStr=cmdStr, ctxt=REMOTE, remoteHost=host)
        pool.addCommand(cmd)
        cmds[host] = cmd
    pool.join()
    pool.check_results()
    return cmds
