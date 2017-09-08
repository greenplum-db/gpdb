#!/usr/bin/env python

import os, sys

if sys.hexversion < 0x2040400:
    sys.stderr.write("pysync.py needs python version at least 2.4.4.\n")
    sys.stderr.write("You are using %s\n" % sys.version)
    sys.stderr.write("Here is a guess at where the python executable is--\n")
    os.system("/bin/sh -c 'type python>&2'");
    sys.exit(1)

import cPickle
import inspect
import hashlib
import signal
import socket
import subprocess
import threading
import zlib
import pysync_remote
from pysync_remote import Options
from pysync_remote import ProgressUpdate, ProgressCounters
from pysync_remote import statToTuple
from gppylib.commands.gp import PySync

# MPP-13617
import re

RE1 = re.compile('\\[([^]]+)\\]:(.+)')

bootstrapSource = """
import os,sys
exec(sys.stdin.read(int(sys.stdin.readline())))
"""


class PysyncProxy:
    '''
    The PysyncProxy class is used to initiate a third-party synchronization operation.
    An instance of PysyncProxy is used to start a LocalPysync instance on a remote host
    to be used as the source of the synchronization operation.  The "remote" LocalPysync
    instance then runs RemotePysync on the destination as usual.  Progress information
    is fed from the destination host, through the remote LocalPysync instance an to this
    instance for reporting.
    
    Lines written by LocalPysync to stdout are recorded in the list self.stdout; lines 
    written by LocalPysync to stderr are recorded in self.stderr.  Progress information
    is handled only by the functions set for the recordProgressCallback and 
    recordRawProgressCallback properties.
    '''

    class _Quit(SystemExit):
        def __init__(self, *info):
            SystemExit.__init__(self, *info)

    def __init__(self, sourceHost, sourceDir, destHost, destDir, syncOptions, verbose=False,
                 progressBytes=None, progressTime=None,
                 recordProgressCallback=None, recordRawProgressCallback=None, progressTimestamp=False):
        '''
        Initialize a new PysyncProxy instance.
        
        sourceHost - the host from which data is to be copied.
        sourceDir - the directory on sourceHost from which data is to be copied.
        destHost - the host to which data is to be copied.
        destDir - the directory on sourceHost to which data is to be copied.
        syncOptions - a list of command-line options as described by LocalPysync.usage();
                other options may be added based on the following arguments.
        verbose - indicates whether or not debugging output is generated.
        progressBytes - the number of bytes moved for a volume-based progress message;
                maps to the LocalPysync --progress-bytes option.
        progressTime - the amount of time for a time-based progress message; maps to
                the LocalPysync --progress-time option.
        recordProgressCallback - function to call to present a printable progress
                message generated by RemotePysync; the function must accept a single
                argument of type str.  If not set, progress messages are ignored.
        recordRawProgressCallback - function to call to handle raw progress information
                generated by RemotePysync; the function must accept a single argument
                of type pysync_remote.ProgressUpdate.  If not set, raw progress 
                information is ignored.
        progressTimestamp - indicates whether or not RemotePysync should include the
                observation timestamp on messages it creates.
        '''

        self.ppid = 0
        self.sourceHost = sourceHost
        self.sourceDir = sourceDir
        self.destHost = destHost
        self.destDir = destDir
        self.recordProgressCallback = recordProgressCallback
        self.recordRawProgressCallback = recordRawProgressCallback

        self.syncOptions = syncOptions
        if verbose:
            self.syncOptions += ["-v"]
        if progressBytes:
            self.syncOptions += ["--progress-bytes", progressBytes]
        if progressTime:
            self.syncOptions += ["--progress-time", progressTime]

        self.syncOptions += ["--proxy"]
        if not progressTimestamp:
            self.syncOptions += ["--omit-progress-timestamp"]

        self.stderr = []
        self.stdout = []
        self.cmd = None
        self.returncode = None

    def run(self):
        '''
        Initiate and wait for completion of a directory synchronization operation.
        
        Stderr output is appended to the self.stderr list.  Stdout output is appended
        to the self.stdout list.  Progress messages are written to stdout unless a 
        callback is set.
        '''

        pysyncCmd = PySync('pysync', self.sourceDir, self.destHost, self.destDir,
                           options=' '.join(self.syncOptions))
        self.cmd = '. %s/greenplum_path.sh && %s' % (os.environ.get('GPHOME'), pysyncCmd.cmdStr)

        # save of ppid to allow the process to be stopped.
        self.ppid = os.getppid()
        pidFilename = '/tmp/pysync.py.%s.%s.ppid' % (self.destHost, self.destDir.replace('/', '_'))
        pidFile = open(pidFilename, 'w')
        pidFile.write('%d' % (self.ppid))
        pidFile.close()

        code = 0
        self.p = None
        stderrThread = None
        try:
            try:
                args = []
                args.append("ssh")
                args.extend(["-o", "BatchMode=yes"])
                args.extend(["-o", "StrictHostKeyChecking=no"])
                args.append(self.sourceHost)
                args.append(self.cmd)
                self.p = subprocess.Popen(args, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                stderrThread = ReaderThread("pysync_stderr", self.p.stderr, self.stderr)
                stderrThread.start()
                code = self._work()
            except OSError, e:
                self.stderr.append(str(e))
                raise
        finally:
            os.remove(pidFilename)
            if self.p:
                timer = threading.Timer(2.0, (lambda: os.kill(self.p.pid, signal.SIGHUP)))
                timer.start()
                self.returncode = self.p.wait()
                timer.cancel()
            if stderrThread:
                stderrThread.join(2.0)

        return code

    def _work(self):
        '''
        Wait for and process commands from the LocalPysync instance connected
        to the Popened SSH process.
        
        Command processing continues until EOF is reached on Popen.stdout (the 
        command input stream from LocalPysync) or a "quit" command is proocessed.
        
        Because standard command output may be interleaved with serialized command
        objects, command objects are prefixed with "pKl:<length>\n".  Non-command
        object lines are appended to the self.stdout buffer.
        '''
        while True:
            try:
                # check if parent still alive
                os.kill(self.ppid, 0)
            except:
                # parent gone, exit
                return 2

            # Get the length of the next serialized command
            a = self.p.stdout.readline()
            if len(a) == 0:
                # End the command loop if EOF
                self.stderr.append("[FATAL]:-Unexpected EOF on LocalPysync output stream")
                return 3

            # If not a pickled command object, just record it
            if not a.startswith("pKl:"):
                self.stdout.append(a.rstrip())
                continue

            size = int(a[4:])

            # Read the serialized command and process it.
            data = self.p.stdout.read(size)
            assert len(data) == size
            try:
                self._doCommand(cPickle.loads(data))
            except PysyncProxy._Quit, e:
                return e.code

    def _doCommand(self, what):
        '''
        Perform the command requested by the remote side and prepare any
        result.
        '''
        if what[0] == 'recordProgress':
            if self.recordProgressCallback:
                self.recordProgressCallback(what[1].rstrip())
            return None

        elif what[0] == 'recordRawProgress':
            if self.recordRawProgressCallback:
                self.recordRawProgressCallback(what[1])
            return None

        elif what[0] == 'quit':
            raise PysyncProxy._Quit(what[1])

        else:
            assert 0


class ReaderThread(threading.Thread):
    '''
    Appends all output read from a file handle to the lines buffer.
    '''

    def __init__(self, name, file, lines):
        self.file = file
        self.lines = lines
        threading.Thread.__init__(self, name=name)
        self.setDaemon(True)

    def run(self):
        for line in self.file:
            self.lines.append(line.rstrip())


class LocalPysync:
    '''
    The LocalPysync class initiates a directory synchronization task by starting
    the pysync_remote module on a target system then processes commands from that
    system to accomplish directry synchronization.  Once the pysync_remote module
    is started on the remote system, this LocalPysync instance acts as the remote
    system's agent.
    
    When invoked through PysyncProxy, stdout is used to return pickled objects
    representing status information from this LocalPysync instance.
    '''

    NUMBER_SCALES = {'M': 1024 * 1024, 'G': 1024 * 1024 * 1024, 'T': 1024 * 1024 * 1024 * 1024}

    class _Quit(SystemExit):
        def __init__(self, *info):
            SystemExit.__init__(self, *info)

    def __init__(self, argv, recordProgressCallback=None, recordRawProgressCallback=None, progressTimestamp=False):
        '''
        Initialize a new LocalPysync instance.
        
        argv - a command-line style list of arguments as described by self.usage()
        recordProgressCallback - function to call to present a printable progress
                message generated by RemotePysync; the function must accept a single
                argument of type str.
        recordRawProgressCallback - function to call to handle raw progress information
                generated by RemotePysync; the function must accept a single argument
                of type pysync_remote.ProgressUpdate.
        progressTimestamp - indicates whether or not RemotePysync should include the
                observation timestamp on messages it creates.
        '''

        self.options = Options()
        self.usingProxy = False
        self.sshargs = []
        self.cache = [None]
        self.exclude = set()
        self.include = set()
        self.recordProgressCallback = recordProgressCallback
        if self.recordProgressCallback:
            self.options.sendProgress = True
        self.recordRawProgressCallback = recordRawProgressCallback
        if self.recordRawProgressCallback:
            self.options.sendRawProgress = True
        self.options.progressTimestamp = progressTimestamp

        a = argv[1:]
        while a:
            if a[0] == '-v':
                self.options.verbose = True

            elif a[0] == '-?':
                self.usage(argv)

            elif a[0] == '-compress':
                self.options.compress = True

            elif a[0] == '-n':
                self.options.minusn = True

            elif a[0] == '--insecure':
                self.options.insecure = True

            elif a[0] == '--ssharg':
                a.pop(0)
                self.sshargs.append(a[0])

            elif a[0] == '--delete':
                self.options.delete = True

            elif a[0] == '-x':
                a.pop(0)
                name = a[0]
                if name[0] == '/':
                    raise Exception('Please do not use absolute path with -x.')
                if name[0:2] != './':
                    name = os.path.join('.', name)
                self.exclude.add(name)

            elif a[0] == '-i':
                a.pop(0)
                name = a[0]
                if name[0] == '/':
                    raise Exception('Please do not use absolute path with -i.')
                if name[0:2] != './':
                    name = os.path.join('.', name)
                self.include.add(name)

            elif a[0] == '--progress-bytes':
                a.pop(0)
                try:
                    scale = a[0][-1]
                    if scale == '%':
                        # Ensure number part is convertable; otherwise pass the whole value
                        factor = float(a[0][:-1])
                        self.options.progressBytes = a[0]
                    elif scale.upper() in LocalPysync.NUMBER_SCALES:
                        # Real numeric value followed by a supported scale identifier
                        progressBytes = int(float(a[0][:-1]) * LocalPysync.NUMBER_SCALES[scale.upper()])
                        self.options.progressBytes = progressBytes
                    else:
                        # If the value isn't a percent or scaled, it must be an integer number of bytes
                        progressBytes = int(a[0])
                        self.options.progressBytes = self.options.progressBytes
                except ValueError:
                    raise ValueError("--progress-bytes value is not supported", a[0])
                if type(
                        self.options.progressBytes) != str and progressBytes < pysync_remote.SyncProgress.MINIMUM_VOLUME_INTERVAL:
                    raise ValueError(
                        "--progress-bytes value must be at least %d" % pysync_remote.SyncProgress.MINIMUM_VOLUME_INTERVAL,
                        a[0])

            elif a[0] == '--progress-time':
                a.pop(0)
                try:
                    progressSeconds = int(60 * float(a[0]))
                    self.options.progressTime = progressSeconds
                except ValueError:
                    raise ValueError("--progress-time value is not supported", a[0])
                if progressSeconds < pysync_remote.SyncProgress.MINIMUM_TIME_INTERVAL:
                    raise ValueError("--progress-time value must be at least %f" % (
                        pysync_remote.SyncProgress.MINIMUM_TIME_INTERVAL / 60))

            elif a[0] == '--proxy':
                self.usingProxy = True
                self.options.sendProgress = True
                self.recordProgressCallback = self._recordProgress
                self.options.sendRawProgress = True
                self.recordRawProgressCallback = self._recordRawProgress

            elif a[0] == '--omit-progress-timestamp':
                self.options.progressTimestamp = False

            else:
                break
            a.pop(0)
        if len(a) != 2:
            self.usage(argv)

        self.sourceDir = os.path.abspath(a[0])
        if not os.path.exists(self.sourceDir):
            raise ValueError("Source path \"%s\" not found" % self.sourceDir)
        if not os.path.isdir(self.sourceDir):
            raise ValueError("Source path \"%s\" is not a directory" % self.sourceDir)
        if not os.access(self.sourceDir, os.F_OK | os.R_OK | os.X_OK):
            raise ValueError("Source path) \"%s\" is not accessible" % self.sourceDir)

        dest = a[1]

        # MPP-13617
        m = re.match(RE1, dest)
        if m:
            self.userAndHost, self.destDir = m.groups()
        else:
            i = dest.find(':')
            if i == -1:
                self.usage(argv)
            self.userAndHost, self.destDir = dest[:i], dest[i + 1:]

        self.connectAddress = None
        self.sendData = None

        hostname = self.userAndHost[self.userAndHost.find('@') + 1:]
        try:
            addrinfo = socket.getaddrinfo(hostname, None)
        except:
            print 'dest>>%s<<' % dest, ' hostname>>%s<<' % hostname
            raise

        if addrinfo:
            self.options.addrinfo = addrinfo[0]
        else:
            raise Exception("Unable to determine address for %s" % self.userAndHost)

    def usage(self, argv):
        sys.stderr.write("""usage:
    python """ + argv[0] + """ [-v] [-?] [-n] 
                [--ssharg arg] [-x exclude_file] [-i include_file] [--insecure] [--delete]
                [--progress-time seconds] [--progress-bytes { n[.n]{% | G | T} }
                [--proxy] [--omit-progress-timestamp]
                sourcedir [user@]host:destdir
        -v: verbose output
        -?: Print this message.
        --ssharg arg: pass arg to ssh.  Use many times to pass many args.
        -n: Do not do any work. Just print how many bytes will need to be
            transferred over the network per file and a total.
        -x name: Do not transfer named file or directory.  Don't be too
            creative with the name.  For example, "directory/./file" will not
            work--use "directory/file".  Name is relative to sourcedir.
        -i name: Only transfer named file or directory. Don't be too
            creative with the name.  For example, "directory/./file" will not
            work--use "directory/file".  Name is relative to sourcedir.
        --insecure: Do not check SHA256 digest after transfering data.
            This makes pysync.py run faster, but a bad guy can forge TCP
            packets and put junk of his choice into your files.
        --delete: Delete things in dst that do not exist in src.
        --progress-time minutes: the number of minutes to elapse before a
            time-based progress message is issued.  Progress messages may
            appear more frequently than specified due to the --progress-bytes 
            value.
        --progress-bytes count:  the number of bytes processed before a
            volume-based progress message is issued.  The count may be a
            number followed by 'G' or 'T' or number followed by '%'.  If
            specified as a percent, the count is calculated as the specified
            percent of the total bytes expected to be processed.
        --proxy: Internal option indicating a call from PysyncProxy.
        --omit-progress-timestamp: Omit the timestamp from progress messages.
""")
        sys.exit(1)

    def readFile(self, filename, offset, size):
        '''
        Read a chunk of the specified size at the specified offset from the
        file identified.  The last chunk read is cached for possible re-reading.
        
        The file is opened only for the duration of the seek and read operations.
        '''
        key = (filename, offset, size)
        if self.cache[0] == key:
            return self.cache[1]
        absfilename = os.path.join(self.sourceDir, filename)
        f = open(absfilename, 'rb')
        f.seek(offset)
        a = f.read(size)
        f.close()
        assert len(a) == size
        self.cache = (key, a)
        return a

    def getList(self):
        '''
        Gets a map of {name:stat} pairs to be processed.  The stat value
        is generally the tuple returned from pysync_remote.statToTuple.
        Hard links (an entry with an inode equal to another in the list)
        are represented by a ('L', linked_name) tuple.
        '''
        list = dict()
        inomap = dict()
        for root, dirs, files in os.walk(self.sourceDir):
            for i in dirs + files:
                absname = os.path.join(root, i)
                relname = '.' + absname[len(self.sourceDir):]
                if relname in self.exclude:
                    if i in dirs:
                        dirs.remove(i)
                    continue
                if len(self.include) > 0:
                    """ Check if the file or dir is in the include list """
                    if relname in self.include:
                        pass
                    else:
                        """ Make sure we include any files or dirs under a dir in the include list."""
                        foundPrefix = False
                        for j in self.include:
                            if relname.startswith(j + '/') == True:
                                foundPrefix = True
                                continue
                        if foundPrefix == False:
                            if i in dirs:
                                dirs.remove(i)
                            continue

                s = os.lstat(absname)
                if s.st_ino in inomap:
                    list[relname] = ('L', inomap[s.st_ino])
                    continue
                inomap[s.st_ino] = relname
                list[relname] = statToTuple(s, absname)
        return list

    def doCommand(self, what):
        '''
        Perform the command requested by the remote side and prepare any
        result.
        '''
        if what[0] == 'connect':
            self.connectAddress = what[1]
        elif what[0] == 'getOptions':
            return self.options
        elif what[0] == 'getDestDir':
            return self.destDir
        elif what[0] == 'getList':
            return self.getList()
        elif what[0] == 'getDigest':
            m = hashlib.sha256()
            m.update(self.readFile(what[1], what[2], what[3]))
            return m.digest()
        elif what[0] == 'getData':
            self.sendData = self.readFile(what[1], what[2], what[3])
            if self.options.compress:
                self.sendData = zlib.compress(self.sendData, 1)
            return len(self.sendData)
        elif what[0] == 'recordProgress':
            if self.recordProgressCallback:
                self.recordProgressCallback(what[1].rstrip())
            else:
                sys.stdout.write(what[1].rstrip())
                sys.stdout.write('\n')
            return None
        elif what[0] == 'recordRawProgress':
            if self.recordRawProgressCallback:
                self.recordRawProgressCallback(what[1])
            else:
                sys.stdout.write("raw: " + str(what[1]))
                sys.stdout.write('\n')
            return None
        elif what[0] == 'quit':
            raise LocalPysync._Quit(what[1])
        else:
            assert 0

    def _recordProgress(self, message):
        '''
        Send progress information to associated PysyncProxy instance.
        '''
        if message:
            self._sendCommand('recordProgress', message)

    def _recordRawProgress(self, progressUpdate):
        '''
        Send raw progress data to associated PysyncProxy instance.
        '''
        if progressUpdate:
            self._sendCommand('recordRawProgress', progressUpdate)

    def _sendCommand(self, *args):
        '''
        Serialize the command & arguments using cPickle and send write to stdout.
        This method is used for communication with the initiating PysyncProxy 
        instance.
        '''
        a = cPickle.dumps(args)
        sys.stdout.write('pKl:%d\n%s' % (len(a), a))
        sys.stdout.flush()

    def work(self):
        '''
        Wait for and process commands from the RemotePysync instance connected
        to the Popened SSH process.
        
        Command processing continues until EOF is reached on Popen.stdout (the 
        command input stream from RemotePysync) or a "quit" command is proocessed.
        Command response objects are serialized and written to Popen.stdin (the 
        command output stream to RemotePysync).
        '''
        while True:
            try:
                # check if parent still alive
                os.kill(os.getppid(), 0)
            except:
                # parent gone, exit
                return 2

            # Get the length of the next serialized command
            a = self.p.stdout.readline()
            if len(a) == 0:
                # End the command loop if EOF
                print >> sys.stderr, "[FATAL]:-Unexpected EOF on RemotePysync output stream"
                return 3
            size = int(a)

            # Read the serialized command and process it.
            data = self.p.stdout.read(size)
            assert len(data) == size
            try:
                answer = cPickle.dumps(self.doCommand(cPickle.loads(data)))
            except LocalPysync._Quit, e:
                return e.code

            # Send the serialized command response
            self.p.stdin.write("%d\n%s" % (len(answer), answer))
            self.p.stdin.flush()

            # If the command was a connect order, open a socket to
            # the remote side for data transfer
            if self.connectAddress != None:
                self.socket = socket.socket(self.options.addrinfo[0])
                self.socket.connect(self.connectAddress)
                self.connectAddress = None

            # If the command was a getData order, send the prepared
            # data over the socket.
            if self.sendData != None:
                self.socket.sendall(self.sendData)
                self.sendData = None

    def run(self):
        '''
        Start the pysync_remote module on the remote host and call self.work() to process 
        commands presented by the remote host.
        '''
        # save of ppid to allow the process to be stopped.
        os.system('echo %d > /tmp/pysync.py.%s.ppid' % (os.getppid(), self.destDir.replace('/', '_')))
        PATH = os.environ.get('PATH') or '.'
        LIBPATH = os.environ.get('LD_LIBRARY_PATH') or '.'

        cmd = ('''. %s/greenplum_path.sh && bash -c "python -u -c '%s'"'''
               % (os.environ.get('GPHOME'),
                  bootstrapSource))
        args = []
        args.append('ssh')
        args.extend(["-o", "BatchMode=yes"])
        args.extend(["-o", "StrictHostKeyChecking=no"])
        args.extend(self.sshargs)
        args.append(self.userAndHost)
        args.append(cmd)
        code = 0
        self.p = None
        try:
            try:
                pysyncSource = inspect.getsource(pysync_remote)
                self.p = subprocess.Popen(args, stdin=subprocess.PIPE, stdout=subprocess.PIPE)
                self.p.stdin.write("%d\n%s" % (len(pysyncSource), pysyncSource))
                code = self.work()
            except OSError, e:
                sys.stderr.write(str(e))
                raise
        finally:
            os.remove('/tmp/pysync.py.%s.ppid' % (self.destDir.replace('/', '_')))
            if self.p:
                timer = threading.Timer(2.0, (lambda: os.kill(self.p.pid, signal.SIGHUP)))
                timer.start()
                rc = self.p.wait()
                timer.cancel()

        if self.usingProxy:
            self._sendCommand('quit', code)

        return code


if os.environ.get('GPHOME') is None:
    print >> sys.stderr, '[FATAL]:- Please specify environment variable GPHOME'
    sys.exit(1)

if __name__ == '__main__':
    sys.exit(LocalPysync(sys.argv, progressTimestamp=True).run())
