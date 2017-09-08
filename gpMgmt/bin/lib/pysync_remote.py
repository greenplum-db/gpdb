'''
The pysync_remote module is a companion to the pysync module containing the
components required for the remote end of a synchronization pair.
'''
from __future__ import with_statement
import sys, os

if sys.hexversion < 0x2040400:
    sys.stderr.write("pysync_remote.py needs python version at least 2.4.4 on the remote computer.\n")
    sys.stderr.write("You are using %s\n" % sys.version)
    sys.stderr.write("Here is a guess at where the python executable is--\n")
    os.system("/bin/sh -c 'type python>&2'");
    sys.exit(1)

import copy
import cPickle
from datetime import datetime
import errno
import hashlib
import socket
import stat
import threading
import time
import zlib

# "Simulate" importing this module as 'pysync_remote' to mirror the namespace
# used by the local pysync component so pickled object deserialization works.
if __name__ == '__main__':
    sys.modules['pysync_remote'] = sys.modules['__main__']

# The number of file bytes processed at a time
CHUNK_SIZE = 4000000


class Progress:
    def __init__(self, byteMax, fileMax):
        self.status = '';
        self.fname = '';
        self.byteMax = byteMax;
        self.byteNow = 0;
        self.fileMax = fileMax;
        self.fileNow = 0;
        self.tstamp = 0;

    def _printStatus(self):
        pct = 0
        if self.byteMax > 0: pct = int(self.byteNow * 100 / self.byteMax);
        sys.stderr.write('[%s %d%%] %s %s byte:%d/%d file:%d/%d\n' % (
            datetime.now().isoformat(' '),
            pct, self.status, self.fname, self.byteNow,
            self.byteMax, self.fileNow, self.fileMax))

    def update(self, status, fname, byteNow, fileNow):
        changed = (status != self.status
                   or fname != self.fname
                   or fileNow != self.fileNow
                   or (time.time() - self.tstamp > 5))
        self.status = status
        self.fname = fname
        self.byteNow = byteNow
        self.fileNow = fileNow
        if changed:
            self.tstamp = time.time()
            self._printStatus()


class Options:
    '''
    A container for the options collected by LocalPysync and use by RemotePysync.
    '''

    def __init__(self):
        self.compress = False
        self.verbose = False
        self.insecure = False
        self.delete = False
        self.minusn = False
        self.progressTime = 0
        self.progressBytes = 0
        self.progressTimestamp = False
        self.sendProgress = True
        self.sendRawProgress = False
        self.addrinfo = None


def statToTuple(s, name):
    '''
    Converts the os.stat() structure to a tuple used internally.  The tuple
    content varies by type but is generally of the form:
    
        (entry_type, protection_bits [, variable_content])
    
    entry_type:
        -    regular files
        l    symbolic link
        d    directory
        c    character special file
        b    block special file
        p    FIFO
        s    socket link
        
    These entry types are coordinated with code in RemotePysync.createFiles().
    '''
    if stat.S_ISREG(s.st_mode):
        return ['-', s.st_mode, s.st_size, None]
    if stat.S_ISLNK(s.st_mode):
        return ('l', 0, os.readlink(name))
    if stat.S_ISDIR(s.st_mode):
        return ('d', s.st_mode)
    if stat.S_ISCHR(s.st_mode):
        return ('c', s.st_mode, s.st_rdev)
    if stat.S_ISBLK(s.st_mode):
        return ('b', s.st_mode, s.st_rdev)
    if stat.S_ISFIFO(s.st_mode):
        return ('p', s.st_mode)
    if stat.S_ISSOCK(s.st_mode):
        return ('s', s.st_mode)
    raise Exception('Pysync can not handle %s.' % name)


def formatSiBinary(number):
    '''
    Format a number using SI/IEC 60027-2 prefixes as described in
    http://physics.nist.gov/cuu/Units/binary.html.
    
    The result is of the form "D.dd P" where D.dd is the input
    number in P units with no more than 2 decimal places.  D will
    be longer than 4 digits only when number exceeds the scale of
    the largest SI/IEC binary prefix supported.
    '''
    number = float(number)
    factor = 1024
    prefixes = ('', 'Ki', 'Mi', 'Gi', 'Ti', 'Pi', 'Ei', 'Zi', 'Yi')
    for prefix in prefixes[:-1]:
        if number < factor:
            return '%.2f %s' % (number, prefix)
        number /= factor
    else:
        return '%.2f %s' % (number, prefixes[-1])


class ProgressUpdate():
    '''
    A container used to report raw synchronization progress information collected
    by SyncProgress and RemotePysync to LocalPysync.
    '''

    def __init__(self, isFinal=False, totalExpectedBytes=0, totalExpectedFiles=0, progressCounters=None):
        # An indication of whether or not this ProgressUpdate is the final
        self.isFinal = isFinal

        # The total number of file bytes expected to be processed
        self.totalExpectedBytes = totalExpectedBytes

        # The total number of files expected to be processed
        self.totalExpectedFiles = totalExpectedFiles

        # ProgressCounters for this update
        self.progressCounters = copy.copy(progressCounters)

    def __str__(self):
        return ("ProgressUpdate(isFinal=%s, totalExpectedBytes=%d, totalExpectedFiles=%d, progressCounters=%s)"
                % (self.isFinal,
                   self.totalExpectedBytes,
                   self.totalExpectedFiles,
                   self.progressCounters))


class ProgressCounters():
    '''
    A structure used to track the progress reported to SyncProgress.
    '''

    def __init__(self, updateTime=None, bytesProcessed=0, filesProcessed=0, processingFile=None):
        # The time (from time.time()) this instance was last updated
        self.updateTime = updateTime

        # The cumulative number of bytes processed as of the last update
        self.bytesProcessed = bytesProcessed

        # The cumulative number of files *completed* as of the last update
        self.filesProcessed = filesProcessed

        # The file being processed during the last update
        self.processingFile = processingFile

    def __str__(self):
        return ("ProgressCounters(updateTime=%f, bytesProcessed=%d, filesProcessed=%d, processingFile=\"%s\"))"
                % (self.updateTime,
                   self.bytesProcessed,
                   self.filesProcessed,
                   self.processingFile))


class SyncProgress(threading.Thread):
    '''
    The SyncProgress class is used to record synchronization progress by RemotePysync and
    issue a periodic progress report.  SyncProgress runs as a daemon Thread and must be 
    started following allocation.  The update() method must be called to feed progress 
    information to this instance; the stop() method should be called when the sync
    operation is complete so a final report can be issued.
    '''

    # The minimum allowed reporting interval based on time (in seconds)
    MINIMUM_TIME_INTERVAL = 2 * 60.0
    DEFAULT_TIME_INTERVAL = 10 * 60.0

    # The minimum allowed reporting interval based on byte volume
    MINIMUM_VOLUME_INTERVAL = 512 * 1024 * 1024
    DEFAULT_VOLUME_INTERVAL = "5.0%"

    def __init__(self, list, reportInterval=0, reportBytes=0,
                 recordProgressCallback=None, recordRawProgressCallback=None,
                 progressTimestamp=False):
        '''
        Initialize a new SyncProgress instance.
        
        list - the dict containing the name:attribute pairings for the
                file system objects to synchronize; this list is expected
                to be obtained from a LocalPysync "getList" command.
        reportInterval - the minimum number of seconds between progress
                reports; volume reports are emitted regardless of the
                reportInterval value.  If zero, the default report interval
                is SyncProgress.DEFAULT_TIME_INTERVAL
        reportBytes - the number of bytes processed for a volume report
                to be issued;  if zero, the default volume report interval
                is SyncProgress.DEFAULT_VOLUME_INTERVAL.  If a string of the 
                form "n.n%", the reportBytes is calculated to be n.n percent
                of the total expected bytes to process.
        recordProgressCallback - method to call to emit the progress *message*
                instead of writting the message to stderr.  This method is 
                presented a string containing the progress message as the only
                argument.  If None, the progress message is written to stderr
                prepended with the timestamp of the observation.
        recordRawProgressCallback - method to call to emit the raw progress data
                collected by an update() call.  This method is presented a
                ProgressUpdate instance as the only argument.
        progressTimestamp - indicates whether or not the observation timestamp is
                included in progress messages emitted by this SyncProgress instance.
        '''

        self._final = False
        self._totalBytes = 0
        self._totalFiles = 0
        self.processingChunk = 0
        self.progressTimestamp = progressTimestamp

        # Calculate the total number of files and bytes that could be 
        # processed.  Only regular files contribute to the total byte 
        # count.
        for desc in list.itervalues():
            if desc[0] == '-':
                self._totalBytes += desc[2]
                self._totalFiles += 1

        # Indicates the time, in seconds, of the progress reporting interval.
        if reportInterval == 0:
            reportInterval = SyncProgress.DEFAULT_TIME_INTERVAL
        elif reportInterval < SyncProgress.MINIMUM_TIME_INTERVAL:
            raise ValueError("reportInterval must be at least %d" % SyncProgress.MINIMUM_TIME_INTERVAL, reportInterval)
        self.progressInterval = reportInterval

        # Indicates the amount, in bytes, of the data processed before a progress report is forced.
        if reportBytes == 0:
            reportBytes = SyncProgress.DEFAULT_VOLUME_INTERVAL
        elif type(reportBytes) == str:
            pass
        elif reportBytes < SyncProgress.MINIMUM_VOLUME_INTERVAL:
            raise ValueError("reportBytes must be at least %d" % SyncProgress.MINIMUM_VOLUME_INTERVAL, reportBytes)
        if type(reportBytes) == str:
            if reportBytes[-1] == '%':
                # Caller has requested a percent of the total
                reportBytes = int(round(self._totalBytes * float(reportBytes[:-1]) / 100))
            else:
                raise ValueError("reportBytes must be numeric or a string in the form \"n.n%\"", reportBytes)
        self.progressBytes = reportBytes

        # Method to call for recording progress details
        self.recordProgressCallback = recordProgressCallback

        # Method to call for recording raw progress data
        self.recordRawProgressCallback = recordRawProgressCallback

        # Active ProgressCounters instance
        self.progressCounters = ProgressCounters()
        # Access synchronization lock for self.progressCounters
        self.progressCountersLock = threading.Lock()

        # Last time progress was reported
        self.lastReportTime = None
        # Copy of ProgressCounters at self.lastReportTime
        self.lastReportProgressCounters = None
        # Access synchronization lock for self.lastReportTime and self.lastReportProgressCounters
        self.progressReportLock = threading.Lock()

        # threading.Event instance used to run the interval timer;
        # setting this Event terminates the reporting loop.
        self.timerEvent = None

        threading.Thread.__init__(self, name="ProgressTimer")
        self.daemon = True

    def _emitProgress(self, progressCounters=None, timedReport=True):
        '''
        Prepare and emit a progress message.  If progressCounters
        is not provided, a copy of self.progressCounters is made
        while holding the self.progressCountersLock; the lock is 
        not held while actually formatting and emitting the progress
        message.
        
        Returns the amount of time, in seconds, for the next timed 
        report.
        '''
        if not progressCounters:
            with self.progressCountersLock:
                progressCounters = copy.copy(self.progressCounters)

        currentReportTime = time.time()
        with self.progressReportLock:
            lastReportTime = self.lastReportTime

            if timedReport:
                # If the last report was more recent than the desired reporting interval,
                # suppress this report and return the amount of time remaining until the
                # next reporting time
                lastReportInterval = currentReportTime - lastReportTime
                if lastReportInterval < self.progressInterval:
                    return self.progressInterval - lastReportInterval

            lastReportProgressCounters = self.lastReportProgressCounters
            self.lastReportTime = currentReportTime
            self.lastReportProgressCounters = progressCounters

        if self._final:
            bytesMoved = progressCounters.bytesProcessed
            processingInterval = currentReportTime - self.collectionStartTime
            byteRate = bytesMoved / processingInterval if processingInterval else 0.0
        else:
            bytesMoved = progressCounters.bytesProcessed - lastReportProgressCounters.bytesProcessed if lastReportProgressCounters else 0
            processingInterval = progressCounters.updateTime - lastReportProgressCounters.updateTime if lastReportProgressCounters else 0.0
            byteRate = bytesMoved / processingInterval if processingInterval else 0.0

        message = ("%sProcessed %sB of %sB (%d%%); %sB processed at %sBps; %d of %d files processed"
                   % ('*' if timedReport else ' ',
                      formatSiBinary(progressCounters.bytesProcessed),
                      formatSiBinary(self._totalBytes),
                      100 * progressCounters.bytesProcessed / self._totalBytes if self._totalBytes != 0 else 1,
                      formatSiBinary(bytesMoved),
                      formatSiBinary(byteRate),
                      progressCounters.filesProcessed,
                      self._totalFiles))

        if self.progressTimestamp:
            message = "[%s]%s" % (datetime.fromtimestamp(currentReportTime).isoformat(' '), message)

        if self.recordProgressCallback:
            self.recordProgressCallback(message)
        else:
            sys.stderr.write(message)
            sys.stderr.write("\n")

        return self.progressInterval

    def update(self, fileBytesProcessed=0, processingFile=None):
        '''
        Update the progress information and emit a progress message if
        deemed necessary.
        
        fileBytesProcessed - the number of bytes processed (moved/examined)
                since progress was last reported.
        processingFile - the file currently being processed.  If different
                from the previous value, the file completion counter is
                incremented.
        
        The final (closing) call to this method is made when the SyncProgress
        thread is terminated by the stop() method call.
        '''
        progressUpdate = None
        progressCounters = None

        with self.progressCountersLock:
            self.progressCounters.updateTime = time.time()
            self.progressCounters.bytesProcessed += fileBytesProcessed

            if self.progressCounters.processingFile != processingFile:
                if self.progressCounters.processingFile:
                    self.progressCounters.filesProcessed += 1
                self.progressCounters.processingFile = processingFile

            processingChunk = self.progressCounters.bytesProcessed // self.progressBytes if self.progressBytes != 0 else 1
            if self._final or processingChunk > self.processingChunk:
                self.processingChunk = processingChunk
                progressCounters = copy.copy(self.progressCounters)

            if self.recordRawProgressCallback:
                progressUpdate = ProgressUpdate(self._final, self._totalBytes, self._totalFiles, self.progressCounters)

        if progressUpdate:
            self.recordRawProgressCallback(progressUpdate)

        if progressCounters:
            self._emitProgress(progressCounters, False)

    def run(self):
        '''
        Emit a progress message periodically based on self.progressInterval.
        Exit once self.timerEvent is set by self.stop().
        
        Updates to the progress counters should be made by calling 
        self.update().
        '''
        self.collectionStartTime = self.lastReportTime = self.progressCounters.updateTime = time.time()
        self.lastReportProgressCounters = copy.copy(self.progressCounters)
        self.timerEvent = threading.Event()

        waitInterval = self.progressInterval
        while not self.timerEvent.isSet():
            self.timerEvent.wait(waitInterval)
            if self.timerEvent.isSet():
                break
            waitInterval = self._emitProgress()

        # Close out statistics and emit the final report
        self._final = True
        self.update()

        self.timerEvent = None

    def stop(self):
        '''
        Stop emitting progress messages and wait for this thread to terminate.
        '''
        if self.isAlive():
            if self.timerEvent:
                self.timerEvent.set()
            self.join(0.5)


class RemotePysync:
    '''
    The RemotePysync class is the receiving end of the directory synchronization 
    initiaited by LocalPysync.  Once started by LocalPysync, RemotePysync manages
    copying the content of a source directory.  
    
    A RemotePysync instance and a LocalPysync instance communicate with each other
    over a pipe; command orders and arguments are transferred to LocalPysync over
    stdout; responses are received from LocalPysync via stdin.
    
    This class is *not* thread-safe; a single RemotePysync instance may be used 
    in a process and must be free to change the current working directory for the
    duration of the synchronization operation.
    '''

    def __init__(self):

        # The SyncProgress instance used to track progress of the directory 
        # synchronization managed by this RemotePysync instance.
        self.syncProgress = None

        # Synchronization lock to prevent interleaved communications using
        # self.sendCommand() and self.getAnswer().
        self.commLock = threading.RLock()

    def sendCommand(self, *args):
        '''
        Serialize the command & arguments using cPickle and send write to stdout.
        
        The self.commLock must be held before calling this method.  The lock should
        not be released until the getAnswer() method is called to obtain the results.
        '''
        a = cPickle.dumps(args)
        sys.stdout.write('%d\n%s' % (len(a), a))
        sys.stdout.flush()

    def getAnswer(self):
        '''
        Read a command response from stdin for a command sent using the sendCommand() 
        method. The response is a data stream serialized using cPickle and preceded by
        the integer size of the serialized data.  The data stream is deserialized using
        cPickle and returned.
        
        The self.commLock must be held before calling this method.  The lock should
        be obtained before calling the self.sendCommand() method to send the command
        for which the call to this method is being made.  Once the answer is obtained,
        the self.commLock should be released.
        '''
        size = int(sys.stdin.readline())
        data = sys.stdin.read(size)
        assert size == len(data)
        return cPickle.loads(data)

    def doCommand(self, *args):
        '''
        Sends the command represented by args to the local peer of this
        RemotePysync and returns with the answer.
        
        This method calls the sendCommand() method followed immediately
        by the getAnswer() method while holding self.commLock.
        '''
        with self.commLock:
            self.sendCommand(*args)
            return self.getAnswer()

    def _recordProgress(self, message):
        '''
        Send progress information to associated LocalPysync instance.
        '''
        if message:
            self.doCommand('recordProgress', message)

    def _recordRawProgress(self, progressUpdate):
        '''
        Send raw progress data to associated LocalPysync instance.
        '''
        if progressUpdate:
            self.doCommand('recordRawProgress', progressUpdate)

    def removeJunk(self, list):
        '''
        Trim the current directory of files and directories not in list
        (if options.delete is True) and files and directories in list if
        the current type is not the expected type.
        
        Existing files of the expected type may be altered or removed in
        self.createFiles().
        '''
        for root, dirs, files in os.walk('.', False):
            os.chmod(root, 0700)
            for i in dirs + files:
                name = os.path.join(root, i)
                t = statToTuple(os.lstat(name), name)
                if name not in list and self.options.delete or name in list and list[name][0] != t[0]:
                    if t[0] == 'd':
                        os.rmdir(name)
                    else:
                        os.remove(name)

    def createDirs(self, list):
        '''
        Create all directories identified in list.
        '''
        for name in sorted(list.keys()):
            value = list[name]
            if value[0] == 'd':
                try:
                    os.mkdir(name, 0700)
                except OSError, err:
                    if err.errno != errno.EEXIST:
                        raise
                        # Ignoring creation error for existing directory

    def createFiles(self, list):
        '''
        Create all files identified in the list.
        
        Directories must be created by calling self.createDirs() before calling
        this method.  Hard links (entry type = 'L') are not processed by this 
        method.
        '''
        for name, value in list.iteritems():

            # If a directory or hard link, skip it.
            if value[0] == 'd' or value[0] == 'L':
                continue

            # If the file already exists, process according to its existing
            # type:
            #    1) If the existing type and the expected/desired type
            #       are the same:
            #        a) set the permissions to 0700 for all except symbolic
            #           links
            #        b) if a regular file and the existing size is greater
            #           than the expected size, truncate the file to the 
            #           expected size and skip further processing for this
            #           file
            #        c) if the variable content of the internal stat tuple
            #           for the existing file and the expected file are the
            #           same, skip further processing for this file.
            #    2) If the existing type and the expected type are not the
            #       same or file processing was not skipped above, remove the
            #       existing file and continue processing.
            if os.path.lexists(name):
                t = statToTuple(os.lstat(name), name)
                if value[0] == t[0]:
                    if value[0] != 'l':
                        os.chmod(name, 0700)
                    if value[0] == '-':
                        value[3] = t[2]
                        if t[2] > value[2]:
                            f = open(name, 'r+b')
                            f.truncate(value[2])
                            f.close()
                        continue
                    if value[2:] == t[2:]:
                        continue
                os.remove(name)

            # Create the file entry based on the desired type.
            # For regular files, an empty (zero-length) file is created.
            if value[0] == '-':
                open(name, 'w+b').close()
            elif value[0] == 'l':
                os.symlink(value[2], name)
            elif value[0] == 'c':
                os.mknod(name, 0700 | stat.S_IFCHR, value[2])
            elif value[0] == 'b':
                os.mknod(name, 0700 | stat.S_IFBLK, value[2])
            elif value[0] == 'p':
                os.mkfifo(name, 0700)
            elif value[0] == 's':
                f = socket.socket(socket.AF_UNIX)
                f.bind(name)
                f.close()
            else:
                assert 0

    def linkLinks(self, list):
        '''
        Create hard links identified in the list.  An existing file
        having the same name as the link is removed before the link
        is created.
        
        Directories must be created by calling self.createDirs() and
        files must be created by calling self.createFiles() before calling
        this method.
        '''
        for name, value in list.iteritems():
            if value[0] == 'L':
                if os.path.lexists(name):
                    os.remove(name)
                os.link(value[1], name)

    def getData(self, name, offset, size):
        if not self.socket:
            ss = socket.socket(self.options.addrinfo[0])
            ss.bind(self.options.addrinfo[4])
            ss.listen(1)
            self.doCommand('connect', ss.getsockname())
            self.socket = ss.accept()[0]
            ss.close()
        s = self.doCommand('getData', name, offset, size)
        o = 0
        sb = []
        while o < s:
            a = self.socket.recv(min(s - o, 65536))
            if len(a) == 0:
                raise IOError('EOF')
            sb.append(a)
            o += len(a)
        data = "".join(sb)
        if self.options.compress:
            data = zlib.decompress(data)
        assert len(data) == size
        return data

    def copyData(self, list):
        fileMax = 0
        byteMax = 0
        for name, value in list.iteritems():
            if value[0] == '-':
                fileMax += 1
                byteMax += value[2]
        progress = Progress(byteMax, fileMax)
        fileNow = 0
        byteNow = 0
        bytesTotal = 0
        unchanged = 0
        for name, value in list.iteritems():
            if value[0] == '-':
                f = None
                if self.options.minusn:
                    if os.path.lexists(name) and stat.S_ISREG(os.lstat(name).st_mode):
                        f = open(name, 'rb')
                else:
                    f = open(name, 'r+b')
                localSize = 0
                if f:
                    f.seek(0, 2)
                    localSize = f.tell()
                offset = 0
                bytesFile = 0
                changed = value[2] != value[3]
                if not value[2]:
                    # Zero-length files require little work
                    if not self.options.minusn:
                        self.syncProgress.update(processingFile=name)
                else:
                    while offset < value[2]:
                        if self.options.verbose:
                            progress.update('', name, byteNow, fileNow)
                        size = min(value[2] - offset, CHUNK_SIZE)
                        digest = None
                        if offset + size <= localSize:
                            with self.commLock:
                                self.sendCommand('getDigest', name, offset, size)
                                f.seek(offset)
                                b = f.read(size)
                                assert len(b) == size
                                m = hashlib.sha256()
                                m.update(b)
                                a = m.digest()
                                digest = self.getAnswer()
                            if a == digest:
                                if not self.options.minusn:
                                    self.syncProgress.update(fileBytesProcessed=size, processingFile=name)
                                offset += size
                                byteNow += size
                                continue
                        changed = True
                        if not self.options.minusn:
                            data = self.getData(name, offset, size)
                            if not self.options.insecure:
                                if digest == None:
                                    digest = self.doCommand('getDigest', name, offset, size)
                                m = hashlib.sha256()
                                m.update(data)
                                if m.digest() != digest:
                                    raise Exception('Digest did not match.')
                            f.seek(offset)
                            f.write(data)
                            self.syncProgress.update(fileBytesProcessed=size, processingFile=name)
                        bytesFile += size
                        bytesTotal += size
                        offset += size
                        byteNow += size
                if f:
                    f.close()
                if self.options.minusn and self.options.verbose:
                    sys.stderr.write('%s %d\n' % (name, bytesFile))
                if not changed:
                    if self.options.verbose:
                        sys.stderr.write('%s is the same\n' % name)
                    unchanged += 1
                fileNow += 1
        if self.options.verbose:
            progress.update('', '', byteNow, fileNow)
        updated = fileMax - unchanged
        if self.options.minusn or self.options.verbose:
            sys.stderr.write('%d out of %d file(s) updated.\n' % (updated, fileMax))
        if self.options.minusn:
            sys.stderr.write('Total--%d byte%s\n' % (bytesTotal, bytesTotal != 1 and 's' or ''))

    def fixPermissions(self, list):
        '''
        Set the permission bits of the non-link files and directories to
        the expected value.
        '''
        for name, value in list.iteritems():
            if value[0] != 'L' and value[0] != 'l':
                os.chmod(name, value[1] & 01777)

    def run(self):
        self.socket = None
        self.options = self.doCommand('getOptions')

        # TODO: Add a safety check for destDir
        os.chdir(self.doCommand('getDestDir'))

        list = self.doCommand('getList')

        if self.options.minusn:
            self.copyData(list)
        else:
            self.syncProgress = SyncProgress(list,
                                             reportInterval=self.options.progressTime,
                                             reportBytes=self.options.progressBytes,
                                             recordProgressCallback=(
                                             self._recordProgress if self.options.sendProgress else None),
                                             recordRawProgressCallback=(
                                             self._recordRawProgress if self.options.sendRawProgress else None),
                                             progressTimestamp=self.options.progressTimestamp)
            self.syncProgress.start()

            self.removeJunk(list)
            self.createDirs(list)
            self.createFiles(list)
            self.linkLinks(list)
            self.copyData(list)
            self.fixPermissions(list)

            self.syncProgress.stop()

        with self.commLock:
            self.sendCommand('quit', 0)


if __name__ == '__main__':
    RemotePysync().run()
