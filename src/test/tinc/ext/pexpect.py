"""Pexpect is a Python module for spawning child applications and controlling
them automatically. Pexpect can be used for automating interactive applications
such as ssh, ftp, passwd, telnet, etc. It can be used to a automate setup
scripts for duplicating software package installations on different servers. It
can be used for automated software testing. Pexpect is in the spirit of Don
Libes' Expect, but Pexpect is pure Python. Other Expect-like modules for Python
require TCL and Expect or require C extensions to be compiled. Pexpect does not
use C, Expect, or TCL extensions. It should work on any platform that supports
the standard Python pty module. The Pexpect interface focuses on ease of use so
that simple tasks are easy.

There are two main interfaces to Pexpect -- the function, run() and the class,
spawn. You can call the run() function to execute a command and return the
output. This is a handy replacement for os.system().

For example:
    pexpect.run('ls -la')

The more powerful interface is the spawn class. You can use this to spawn an
external child command and then interact with the child by sending lines and
expecting responses.

For example:
    child = pexpect.spawn('scp foo myname@host.example.com:.')
    child.expect ('Password:')
    child.sendline (mypassword)

This works even for commands that ask for passwords or other input outside of
the normal stdio streams.

Credits:
Noah Spurrier, Richard Holden, Marco Molteni, Kimberley Burchett, Robert Stone,
Hartmut Goebel, Chad Schroeder, Erick Tryzelaar, Dave Kirby, Ids vander Molen,
George Todd, Noel Taylor, Nicolas D. Cesar, Alexander Gattin,
Geoffrey Marshall, Francisco Lourenco, Glen Mabey, Karthik Gurusamy,
Fernando Perez 
(Let me know if I forgot anyone.)

Free, open source, and all that good stuff.

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

Pexpect Copyright (c) 2006 Noah Spurrier
http://pexpect.sourceforge.net/

$Revision: #4 $
$Date: 2008/03/25 $
"""
try:
    import os, sys, time
    import select
    import string
    import re
    import struct
    import resource
    import types
    import pty
    import tty
    import termios
    import fcntl
    import errno
    import traceback
    import signal
except ImportError, e:
    raise ImportError (str(e) + """
A critical module was not found. Probably this operating system does not support it.
Pexpect is intended for UNIX-like operating systems.""")

__version__ = '2.1'
__revision__ = '$Revision: #4 $'
__all__ = ['ExceptionPexpect', 'EOF', 'TIMEOUT', 'spawn', 'run', 'which', 'split_command_line',
    '__version__', '__revision__']

# Exception classes used by this module.
class ExceptionPexpect(Exception):
    """Base class for all exceptions raised by this module.
    """
    def __init__(self, value):
        self.value = value
    def __str__(self):
        return str(self.value)
    def get_trace(self):
        """This returns an abbreviated stack trace with lines that only concern the caller.
        In other words, the stack trace inside the Pexpect module is not included.
        """
        tblist = traceback.extract_tb(sys.exc_info()[2])
        tblist = filter(self.__filter_not_pexpect, tblist)
        tblist = traceback.format_list(tblist)
        return ''.join(tblist)
    def __filter_not_pexpect(self, trace_list_item):
        if trace_list_item[0].find('pexpect.py') == -1:
            return True
        else:
            return False
class EOF(ExceptionPexpect):
    """Raised when EOF is read from a child.
    """
class TIMEOUT(ExceptionPexpect):
    """Raised when a read time exceeds the timeout.
    """
##class TIMEOUT_PATTERN(TIMEOUT):
##    """Raised when the pattern match time exceeds the timeout.
##    This is different than a read TIMEOUT because the child process may
##    give output, thus never give a TIMEOUT, but the output
##    may never match a pattern.
##    """
##class MAXBUFFER(ExceptionPexpect):
##    """Raised when a scan buffer fills before matching an expected pattern."""

def run (command, timeout=-1, withexitstatus=False, events=None, extra_args=None, logfile=None):
    """This function runs the given command; waits for it to finish;
    then returns all output as a string. STDERR is included in output.
    If the full path to the command is not given then the path is searched.

    Note that lines are terminated by CR/LF (\\r\\n) combination
    even on UNIX-like systems because this is the standard for pseudo ttys.
    If you set withexitstatus to true, then run will return a tuple of
    (command_output, exitstatus). If withexitstatus is false then this
    returns just command_output.

    The run() function can often be used instead of creating a spawn instance.
    For example, the following code uses spawn:
        from pexpect import *
        child = spawn('scp foo myname@host.example.com:.')
        child.expect ('(?i)password')
        child.sendline (mypassword)
    The previous code can be replace with the following, which you may
    or may not find simpler:
        from pexpect import *
        run ('scp foo myname@host.example.com:.', events={'(?i)password': mypassword})

    Examples:
    Start the apache daemon on the local machine:
        from pexpect import *
        run ("/usr/local/apache/bin/apachectl start")
    Check in a file using SVN:
        from pexpect import *
        run ("svn ci -m 'automatic commit' my_file.py")
    Run a command and capture exit status:
        from pexpect import *
        (command_output, exitstatus) = run ('ls -l /bin', withexitstatus=1)

    Tricky Examples:   
    The following will run SSH and execute 'ls -l' on the remote machine.
    The password 'secret' will be sent if the '(?i)password' pattern is ever seen.
        run ("ssh username@machine.example.com 'ls -l'", events={'(?i)password':'secret\n'})

    This will start mencoder to rip a video from DVD. This will also display
    progress ticks every 5 seconds as it runs.
        from pexpect import *
        def print_ticks(d):
            print d['event_count'],
        run ("mencoder dvd://1 -o video.avi -oac copy -ovc copy", events={TIMEOUT:print_ticks}, timeout=5)

    The 'events' argument should be a dictionary of patterns and responses.
    Whenever one of the patterns is seen in the command out
    run() will send the associated response string. Note that you should
    put newlines in your string if Enter is necessary.
    The responses may also contain callback functions.
    Any callback is function that takes a dictionary as an argument.
    The dictionary contains all the locals from the run() function, so
    you can access the child spawn object or any other variable defined
    in run() (event_count, child, and extra_args are the most useful).
    A callback may return True to stop the current run process otherwise
    run() continues until the next event.
    A callback may also return a string which will be sent to the child.
    'extra_args' is not used by directly run(). It provides a way to pass data to
    a callback function through run() through the locals dictionary passed to a callback.
    """
    if timeout == -1:
        child = spawn(command, maxread=2000, logfile=logfile)
    else:
        child = spawn(command, timeout=timeout, maxread=2000, logfile=logfile)
    if events is not None:
        patterns = events.keys()
        responses = events.values()
    else:
        patterns=None # We assume that EOF or TIMEOUT will save us.
        responses=None
    child_result_list = []
    event_count = 0
    while 1:
        try:
            index = child.expect (patterns)
            if type(child.after) is types.StringType:
                child_result_list.append(child.before + child.after)
            else: # child.after may have been a TIMEOUT or EOF, so don't cat those.
                child_result_list.append(child.before)
            if type(responses[index]) is types.StringType:
                child.send(responses[index])
            elif type(responses[index]) is types.FunctionType:
                callback_result = responses[index](locals())
                sys.stdout.flush()
                if type(callback_result) is types.StringType:
                    child.send(callback_result)
                elif callback_result:
                    break
            else:
                raise TypeError ('The callback must be a string or function type.')
            event_count = event_count + 1
        except TIMEOUT, e:
            child_result_list.append(child.before)
            break
        except EOF, e:
            child_result_list.append(child.before)
            break
    child_result = ''.join(child_result_list)
    if withexitstatus:
        child.close()
        return (child_result, child.exitstatus)
    else:
        return child_result

class spawn (object):
    """This is the main class interface for Pexpect.
    Use this class to start and control child applications.
    """

    def __init__(self, command, args=[], timeout=30, maxread=2000, searchwindowsize=None, logfile=None, env=None):
        """This is the constructor. The command parameter may be a string
        that includes a command and any arguments to the command. For example:
            p = pexpect.spawn ('/usr/bin/ftp')
            p = pexpect.spawn ('/usr/bin/ssh user@example.com')
            p = pexpect.spawn ('ls -latr /tmp')
        You may also construct it with a list of arguments like so:
            p = pexpect.spawn ('/usr/bin/ftp', [])
            p = pexpect.spawn ('/usr/bin/ssh', ['user@example.com'])
            p = pexpect.spawn ('ls', ['-latr', '/tmp'])
        After this the child application will be created and
        will be ready to talk to. For normal use, see expect() and 
        send() and sendline().

        The maxread attribute sets the read buffer size.
        This is maximum number of bytes that Pexpect will try to read
        from a TTY at one time.
        Seeting the maxread size to 1 will turn off buffering.
        Setting the maxread value higher may help performance in cases
        where large amounts of output are read back from the child.
        This feature is useful in conjunction with searchwindowsize.
        
        The searchwindowsize attribute sets the how far back in
        the incomming seach buffer Pexpect will search for pattern matches.
        Every time Pexpect reads some data from the child it will append the data to
        the incomming buffer. The default is to search from the beginning of the
        imcomming buffer each time new data is read from the child.
        But this is very inefficient if you are running a command that
        generates a large amount of data where you want to match
        The searchwindowsize does not effect the size of the incomming data buffer.
        You will still have access to the full buffer after expect() returns.
        
        The logfile member turns on or off logging.
        All input and output will be copied to the given file object.
        Set logfile to None to stop logging. This is the default.
        Set logfile to sys.stdout to echo everything to standard output.
        The logfile is flushed after each write.
        Example 1:
            child = pexpect.spawn('some_command')
            fout = file('mylog.txt','w')
            child.logfile = fout
        Example 2:
            child = pexpect.spawn('some_command')
            child.logfile = sys.stdout
            
        The delaybeforesend helps overcome a weird behavior that many users were experiencing.
        The typical problem was that a user would expect() a "Password:" prompt and
        then immediately call sendline() to send the password. The user would then
        see that their password was echoed back to them. Passwords don't
        normally echo. The problem is caused by the fact that most applications
        print out the "Password" prompt and then turn off stdin echo, but if you
        send your password before the application turned off echo, then you get
        your password echoed. Normally this wouldn't be a problem when interacting
        with a human at a real heyboard. If you introduce a slight delay just before 
        writing then this seems to clear up the problem. This was such a common problem 
        for many users that I decided that the default pexpect behavior
        should be to sleep just before writing to the child application.
        1/10th of a second (100 ms) seems to be enough to clear up the problem.
        You can set delaybeforesend to 0 to return to the old behavior.
        
        Note that spawn is clever about finding commands on your path.
        It uses the same logic that "which" uses to find executables.

        If you wish to get the exit status of the child you must call
        the close() method. The exit or signal status of the child will be
        stored in self.exitstatus or self.signalstatus.
        If the child exited normally then exitstatus will store the exit return code and
        signalstatus will be None.
        If the child was terminated abnormally with a signal then signalstatus will store
        the signal value and exitstatus will be None.
        If you need more detail you can also read the self.status member which stores
        the status returned by os.waitpid. You can interpret this using
        os.WIFEXITED/os.WEXITSTATUS or os.WIFSIGNALED/os.TERMSIG.
        """
        self.STDIN_FILENO = pty.STDIN_FILENO
        self.STDOUT_FILENO = pty.STDOUT_FILENO
        self.STDERR_FILENO = pty.STDERR_FILENO
        self.stdin = sys.stdin
        self.stdout = sys.stdout
        self.stderr = sys.stderr

        self.patterns = None
        self.ignorecase = False
        self.before = None
        self.after = None
        self.match = None
        self.match_index = None
        self.terminated = True
        self.exitstatus = None
        self.signalstatus = None
        self.status = None # status returned by os.waitpid 
        self.flag_eof = False
        self.pid = None
        self.child_fd = -1 # initially closed
        self.timeout = timeout
        self.delimiter = EOF
        self.logfile = logfile    
        self.maxread = maxread # Max bytes to read at one time into buffer.
        self.buffer = '' # This is the read buffer. See maxread.
        self.searchwindowsize = searchwindowsize # Anything before searchwindowsize point is preserved, but not searched.
        self.delaybeforesend = 0.1 # Sets sleep time used just before sending data to child.
        self.delayafterclose = 0.1 # Sets delay in close() method to allow kernel time to update process status.
        self.delayafterterminate = 0.1 # Sets delay in terminate() method to allow kernel time to update process status.
        self.softspace = False # File-like object.
        self.name = '<' + repr(self) + '>' # File-like object.
        self.encoding = None # File-like object.
        self.closed = True # File-like object.
        self.env = env
        self.__irix_hack = sys.platform.lower().find('irix') >= 0 # This flags if we are running on irix
        self.use_native_pty_fork = not (sys.platform.lower().find('solaris') >= 0) # Solaris uses internal __fork_pty(). All other use pty.fork().

        # allow dummy instances for subclasses that may not use command or args.
        if command is None:
            self.command = None
            self.args = None
            self.name = '<pexpect factory incomplete>'
            return

        # If command is an int type then it may represent a file descriptor.
        if type(command) == type(0):
            raise ExceptionPexpect ('Command is an int type. If this is a file descriptor then maybe you want to use fdpexpect.fdspawn which takes an existing file descriptor instead of a command string.')

        if type (args) != type([]):
            raise TypeError ('The argument, args, must be a list.')

        if args == []:
            self.args = split_command_line(command)
            self.command = self.args[0]
        else:
            self.args = args[:] # work with a copy
            self.args.insert (0, command)
            self.command = command

        command_with_path = which(self.command)
        if command_with_path is None:
            raise ExceptionPexpect ('The command was not found or was not executable: %s.' % self.command)
        self.command = command_with_path
        self.args[0] = self.command

        self.name = '<' + ' '.join (self.args) + '>'
        self.__spawn()

    def __del__(self):
        """This makes sure that no system resources are left open.
        Python only garbage collects Python objects. OS file descriptors
        are not Python objects, so they must be handled explicitly.
        If the child file descriptor was opened outside of this class
        (passed to the constructor) then this does not close it.
        """
        if not self.closed:
            self.close()

    def __str__(self):
        """This returns the current state of the pexpect object as a string.
        """
        s = []
        s.append(repr(self))
        s.append('version: ' + __version__ + ' (' + __revision__ + ')')
        s.append('command: ' + str(self.command))
        s.append('args: ' + str(self.args))
        if self.patterns is None:
            s.append('patterns: None')
        else:
            s.append('patterns:')
            for p in self.patterns:
                if type(p) is type(re.compile('')):
                    s.append('    ' + str(p.pattern))
                else:
                    s.append('    ' + str(p))
        s.append('buffer (last 100 chars): ' + str(self.buffer)[-100:])
        s.append('before (last 100 chars): ' + str(self.before)[-100:])
        s.append('after: ' + str(self.after))
        s.append('match: ' + str(self.match))
        s.append('match_index: ' + str(self.match_index))
        s.append('exitstatus: ' + str(self.exitstatus))
        s.append('flag_eof: ' + str(self.flag_eof))
        s.append('pid: ' + str(self.pid))
        s.append('child_fd: ' + str(self.child_fd))
        s.append('closed: ' + str(self.closed))
        s.append('timeout: ' + str(self.timeout))
        s.append('delimiter: ' + str(self.delimiter))
        s.append('logfile: ' + str(self.logfile))
        s.append('maxread: ' + str(self.maxread))
        s.append('ignorecase: ' + str(self.ignorecase))
        s.append('searchwindowsize: ' + str(self.searchwindowsize))
        s.append('delaybeforesend: ' + str(self.delaybeforesend))
        s.append('delayafterclose: ' + str(self.delayafterclose))
        s.append('delayafterterminate: ' + str(self.delayafterterminate))
        return '\n'.join(s)

    def __spawn(self):
        """This starts the given command in a child process.
        This does all the fork/exec type of stuff for a pty.
        This is called by __init__. 
        """
        # The pid and child_fd of this object get set by this method.
        # Note that it is difficult for this method to fail.
        # You cannot detect if the child process cannot start.
        # So the only way you can tell if the child process started
        # or not is to try to read from the file descriptor. If you get
        # EOF immediately then it means that the child is already dead.
        # That may not necessarily be bad because you may haved spawned a child
        # that performs some task; creates no stdout output; and then dies.

        assert self.pid is None, 'The pid member should be None.'
        assert self.command is not None, 'The command member should not be None.'

        if self.use_native_pty_fork:
            try:
                self.pid, self.child_fd = pty.fork()
            except OSError, e:
                raise ExceptionPexpect('Error! pty.fork() failed: ' + str(e))
        else: # Use internal __fork_pty
            self.pid, self.child_fd = self.__fork_pty() 

        if self.pid == 0: # Child
            try: 
                self.child_fd = sys.stdout.fileno() # used by setwinsize()
                self.setwinsize(24, 80)
            except: 
                # Some platforms do not like setwinsize (Cygwin).
                # This will cause problem when running applications that
                # are very picky about window size.
                # This is a serious limitation, but not a show stopper.
                pass
            # Do not allow child to inherit open file descriptors from parent.
            max_fd = resource.getrlimit(resource.RLIMIT_NOFILE)[0]
            for i in range (3, max_fd):
                try:
                    os.close (i)
                except OSError:
                    pass

            # I don't know why this works, but ignoring SIGHUP fixes a
            # problem when trying to start a Java daemon with sudo
            # (specifically, Tomcat).
            signal.signal(signal.SIGHUP, signal.SIG_IGN)

            if self.env is None:
                os.execv(self.command, self.args)
            else:
                os.execvpe(self.command, self.args, self.env)

        # Parent
        self.terminated = False
        self.closed = False

    def __fork_pty(self):
        """This implements a substitute for the forkpty system call.
        This should be more portable than the pty.fork() function.
        Specifically, this should work on Solaris.
        
        Modified 10.06.05 by Geoff Marshall:
            Implemented __fork_pty() method to resolve the issue with Python's 
            pty.fork() not supporting Solaris, particularly ssh.
        Based on patch to posixmodule.c authored by Noah Spurrier:
            http://mail.python.org/pipermail/python-dev/2003-May/035281.html
        """
        parent_fd, child_fd = os.openpty()
        if parent_fd < 0 or child_fd < 0:
            raise ExceptionPexpect, "Error! Could not open pty with os.openpty()."
        
        pid = os.fork()
        if pid < 0:
            raise ExceptionPexpect, "Error! Failed os.fork()."
        elif pid == 0:
            # Child.
            os.close(parent_fd)
            self.__pty_make_controlling_tty(child_fd)
            
            os.dup2(child_fd, 0)
            os.dup2(child_fd, 1)
            os.dup2(child_fd, 2)
            
            if child_fd > 2:
                os.close(child_fd)
        else:
            # Parent.
            os.close(child_fd)
        
        return pid, parent_fd
                
    def __pty_make_controlling_tty(self, tty_fd):
        """This makes the pseudo-terminal the controlling tty.
        This should be more portable than the pty.fork() function.
        Specifically, this should work on Solaris.
        """
        child_name = os.ttyname(tty_fd)
        
        # Disconnect from controlling tty if still connected.
        fd = os.open("/dev/tty", os.O_RDWR | os.O_NOCTTY);
        if fd >= 0:
            os.close(fd)
            
        os.setsid()
        
        # Verify we are disconnected from controlling tty
        try:
            fd = os.open("/dev/tty", os.O_RDWR | os.O_NOCTTY);
            if fd >= 0:
                os.close(fd)
                raise ExceptionPexpect, "Error! We are not disconnected from a controlling tty."
        except:
            # Good! We are disconnected from a controlling tty.
            pass
        
        # Verify we can open child pty.
        fd = os.open(child_name, os.O_RDWR);
        if fd < 0:
            raise ExceptionPexpect, "Error! Could not open child pty, " + child_name
        else:
            os.close(fd)

        # Verify we now have a controlling tty.
        fd = os.open("/dev/tty", os.O_WRONLY)
        if fd < 0:
            raise ExceptionPexpect, "Error! Could not open controlling tty, /dev/tty"
        else:
            os.close(fd)
         
    def fileno (self):   # File-like object.
        """This returns the file descriptor of the pty for the child.
        """
        return self.child_fd

    def close (self, force=True):   # File-like object.
        """This closes the connection with the child application.
        Note that calling close() more than once is valid.
        This emulates standard Python behavior with files.
        Set force to True if you want to make sure that the child is terminated
        (SIGKILL is sent if the child ignores SIGHUP and SIGINT).
        """
        if not self.closed:
            self.flush()
            os.close (self.child_fd)
            self.child_fd = -1
            self.closed = True
            time.sleep(self.delayafterclose) # Give kernel time to update process status.
            if self.isalive():
                if not self.terminate(force):
                    raise ExceptionPexpect ('close() could not terminate the child using terminate()')

    def flush (self):   # File-like object.
        """This does nothing. It is here to support the interface for a File-like object.
        """
        pass

    def isatty (self):   # File-like object.
        """This returns True if the file descriptor is open and connected to a tty(-like) device, else False.
        """
        return os.isatty(self.child_fd)

    def setecho (self, state):
        """This sets the terminal echo mode on or off.
        Note that anything the child sent before the echo will be lost, so
        you should be sure that your input buffer is empty before you setecho.
        For example, the following will work as expected.
            p = pexpect.spawn('cat')
            p.sendline ('1234') # We will see this twice (once from tty echo and again from cat).
            p.expect (['1234'])
            p.expect (['1234'])
            p.setecho(False) # Turn off tty echo
            p.sendline ('abcd') # We will set this only once (echoed by cat).
            p.sendline ('wxyz') # We will set this only once (echoed by cat)
            p.expect (['abcd'])
            p.expect (['wxyz'])
        The following WILL NOT WORK because the lines sent before the setecho
        will be lost:
            p = pexpect.spawn('cat')
            p.sendline ('1234') # We will see this twice (once from tty echo and again from cat).
            p.setecho(False) # Turn off tty echo
            p.sendline ('abcd') # We will set this only once (echoed by cat).
            p.sendline ('wxyz') # We will set this only once (echoed by cat)
            p.expect (['1234'])
            p.expect (['1234'])
            p.expect (['abcd'])
            p.expect (['wxyz'])
        """
        self.child_fd
        new = termios.tcgetattr(self.child_fd)
        if state:
            new[3] = new[3] | termios.ECHO
        else:
            new[3] = new[3] & ~termios.ECHO
        # I tried TCSADRAIN and TCSAFLUSH, but these were inconsistent
        # and blocked on some platforms. TCSADRAIN is probably ideal if it worked.
        termios.tcsetattr(self.child_fd, termios.TCSANOW, new)
    
    def read_nonblocking (self, size = 1, timeout = -1):
        """This reads at most size characters from the child application.
        It includes a timeout. If the read does not complete within the
        timeout period then a TIMEOUT exception is raised.
        If the end of file is read then an EOF exception will be raised.
        If a log file was set using setlog() then all data will
        also be written to the log file.

        If timeout==None then the read may block indefinitely.
        If timeout==-1 then the self.timeout value is used.
        If timeout==0 then the child is polled and 
            if there was no data immediately ready then this will raise a TIMEOUT exception.
        
        The "timeout" refers only to the amount of time to read at least one character.
        This is not effected by the 'size' parameter, so if you call
        read_nonblocking(size=100, timeout=30) and only one character is
        available right away then one character will be returned immediately. 
        It will not wait for 30 seconds for another 99 characters to come in.
        
        This is a wrapper around os.read().
        It uses select.select() to implement a timeout. 
        """
        if self.closed:
            raise ValueError ('I/O operation on closed file in read_nonblocking().')

        if timeout == -1:
            timeout = self.timeout

        # Note that some systems such as Solaris do not give an EOF when
        # the child dies. In fact, you can still try to read
        # from the child_fd -- it will block forever or until TIMEOUT.
        # For this case, I test isalive() before doing any reading.
        # If isalive() is false, then I pretend that this is the same as EOF.
        if not self.isalive():
            r,w,e = self.__select([self.child_fd], [], [], 0) # timeout of 0 means "poll"
            if not r:
                self.flag_eof = True
                raise EOF ('End Of File (EOF) in read_nonblocking(). Braindead platform.')
        elif self.__irix_hack:
            # This is a hack for Irix. It seems that Irix requires a long delay before checking isalive.
            # This adds a 2 second delay, but only when the child is terminated.
            r, w, e = self.__select([self.child_fd], [], [], 2)
            if not r and not self.isalive():
                self.flag_eof = True
                raise EOF ('End Of File (EOF) in read_nonblocking(). Pokey platform.')
            
        r,w,e = self.__select([self.child_fd], [], [], timeout)
        
        if not r:
            if not self.isalive():
                # Some platforms, such as Irix, will claim that their processes are alive;
                # then timeout on the select; and then finally admit that they are not alive.
                self.flag_eof = True
                raise EOF ('End of File (EOF) in read_nonblocking(). Very pokey platform.')
            else:
                raise TIMEOUT ('Timeout exceeded in read_nonblocking().')

        if self.child_fd in r:
            try:
                s = os.read(self.child_fd, size)
            except OSError, e: # Linux does this
                self.flag_eof = True
                raise EOF ('End Of File (EOF) in read_nonblocking(). Exception style platform.')
            if s == '': # BSD style
                self.flag_eof = True
                raise EOF ('End Of File (EOF) in read_nonblocking(). Empty string style platform.')

            if self.logfile is not None:
                self.logfile.write (s)
                self.logfile.flush()

            return s

        raise ExceptionPexpect ('Reached an unexpected state in read_nonblocking().')

    def read (self, size = -1):   # File-like object.
        """This reads at most "size" bytes from the file 
        (less if the read hits EOF before obtaining size bytes). 
        If the size argument is negative or omitted, 
        read all data until EOF is reached. 
        The bytes are returned as a string object. 
        An empty string is returned when EOF is encountered immediately.
        """
        if size == 0:
            return ''
        if size < 0:
            self.expect (self.delimiter) # delimiter default is EOF
            return self.before

        # I could have done this more directly by not using expect(), but
        # I deliberately decided to couple read() to expect() so that
        # I would catch any bugs early and ensure consistant behavior.
        # It's a little less efficient, but there is less for me to
        # worry about if I have to later modify read() or expect().
        # Note, it's OK if size==-1 in the regex. That just means it
        # will never match anything in which case we stop only on EOF.
        cre = re.compile('.{%d}' % size, re.DOTALL) 
        index = self.expect ([cre, self.delimiter]) # delimiter default is EOF
        if index == 0:
            return self.after ### self.before should be ''. Should I assert this?
        return self.before
        
    def readline (self, size = -1):    # File-like object.
        """This reads and returns one entire line. A trailing newline is kept in
        the string, but may be absent when a file ends with an incomplete line. 
        Note: This readline() looks for a \\r\\n pair even on UNIX because
        this is what the pseudo tty device returns. So contrary to what you
        may expect you will receive the newline as \\r\\n.
        An empty string is returned when EOF is hit immediately.
        Currently, the size agument is mostly ignored, so this behavior is not
        standard for a file-like object. If size is 0 then an empty string
        is returned.
        """
        if size == 0:
            return ''
        index = self.expect (['\r\n', self.delimiter]) # delimiter default is EOF
        if index == 0:
            return self.before + '\r\n'
        else:
            return self.before

    def __iter__ (self):    # File-like object.
        """This is to support iterators over a file-like object.
        """
        return self

    def next (self):    # File-like object.
        """This is to support iterators over a file-like object.
        """
        result = self.readline()
        if result == "":
            raise StopIteration
        return result

    def readlines (self, sizehint = -1):    # File-like object.
        """This reads until EOF using readline() and returns a list containing 
        the lines thus read. The optional "sizehint" argument is ignored.
        """        
        lines = []
        while True:
            line = self.readline()
            if not line:
                break
            lines.append(line)
        return lines

    def write(self, str):   # File-like object.
        """This is similar to send() except that there is no return value.
        """
        self.send (str)

    def writelines (self, sequence):   # File-like object.
        """This calls write() for each element in the sequence.
        The sequence can be any iterable object producing strings, 
        typically a list of strings. This does not add line separators
        There is no return value.
        """
        for str in sequence:
            self.write (str)

    def send(self, str):
        """This sends a string to the child process.
        This returns the number of bytes written.
        If a log file was set then the data is also written to the log.
        """
        time.sleep(self.delaybeforesend)
        if self.logfile is not None:
            self.logfile.write (str)
            self.logfile.flush()
        c = os.write(self.child_fd, str)
        return c

    def sendline(self, str=''):
        """This is like send(), but it adds a line feed (os.linesep).
        This returns the number of bytes written.
        """
        n = self.send(str)
        n = n + self.send (os.linesep)
        return n

    def sendeof(self):
        """This sends an EOF to the child.
        This sends a character which causes the pending parent output
        buffer to be sent to the waiting child program without
        waiting for end-of-line. If it is the first character of the
        line, the read() in the user program returns 0, which
        signifies end-of-file. This means to work as expected 
        a sendeof() has to be called at the begining of a line. 
        This method does not send a newline. It is the responsibility
        of the caller to ensure the eof is sent at the beginning of a line.
        """
        ### Hmmm... how do I send an EOF?
        ###C  if ((m = write(pty, *buf, p - *buf)) < 0)
        ###C      return (errno == EWOULDBLOCK) ? n : -1;
        fd = sys.stdin.fileno()
        old = termios.tcgetattr(fd) # remember current state
        new = termios.tcgetattr(fd)
        new[3] = new[3] | termios.ICANON # ICANON must be set to recognize EOF
        try: # use try/finally to ensure state gets restored
            termios.tcsetattr(fd, termios.TCSADRAIN, new)
            if 'CEOF' in dir(termios):
                os.write (self.child_fd, '%c' % termios.CEOF)
            else:
                os.write (self.child_fd, '%c' % 4) # Silly platform does not define CEOF so assume CTRL-D
        finally: # restore state
            termios.tcsetattr(fd, termios.TCSADRAIN, old)

    def eof (self):
        """This returns True if the EOF exception was ever raised.
        """
        return self.flag_eof

    def terminate(self, force=False):
        """This forces a child process to terminate.
        It starts nicely with SIGHUP and SIGINT. If "force" is True then
        moves onto SIGKILL.
        This returns True if the child was terminated.
        This returns False if the child could not be terminated.
        """
        if not self.isalive():
            return True
        self.kill(signal.SIGHUP)
        time.sleep(self.delayafterterminate)
        if not self.isalive():
            return True
        self.kill(signal.SIGCONT)
        time.sleep(self.delayafterterminate)
        if not self.isalive():
            return True
        self.kill(signal.SIGINT)
        time.sleep(self.delayafterterminate)
        if not self.isalive():
            return True
        if force:
            self.kill(signal.SIGKILL)
            time.sleep(self.delayafterterminate)
            if not self.isalive():
                return True
            else:
                return False
        return False
        #raise ExceptionPexpect ('terminate() could not terminate child process. Try terminate(force=True)?')
     
    def wait(self):
        """This waits until the child exits. This is a blocking call.
            This will not read any data from the child, so this will block forever
            if the child has unread output and has terminated. In other words, the child
            may have printed output then called exit(); but, technically, the child is
            still alive until its output is read.
        """
        if self.isalive():
            pid, status = os.waitpid(self.pid, 0)
        else:
            raise ExceptionPexpect ('Cannot wait for dead child process.')
        self.exitstatus = os.WEXITSTATUS(status)
        if os.WIFEXITED (status):
            self.status = status
            self.exitstatus = os.WEXITSTATUS(status)
            self.signalstatus = None
            self.terminated = True
        elif os.WIFSIGNALED (status):
            self.status = status
            self.exitstatus = None
            self.signalstatus = os.WTERMSIG(status)
            self.terminated = True
        elif os.WIFSTOPPED (status):
            raise ExceptionPexpect ('Wait was called for a child process that is stopped. This is not supported. Is some other process attempting job control with our child pid?')
        return self.exitstatus
   
    def isalive(self):
        """This tests if the child process is running or not.
        This is non-blocking. If the child was terminated then this
        will read the exitstatus or signalstatus of the child.
        This returns True if the child process appears to be running or False if not.
        It can take literally SECONDS for Solaris to return the right status.
        """
        if self.terminated:
            return False

        if self.flag_eof:
            # This is for Linux, which requires the blocking form of waitpid to get
            # status of a defunct process. This is super-lame. The flag_eof would have
            # been set in read_nonblocking(), so this should be safe.
            waitpid_options = 0
        else:
            waitpid_options = os.WNOHANG
            
        try:
            pid, status = os.waitpid(self.pid, waitpid_options)
        except OSError, e: # No child processes
            if e[0] == errno.ECHILD:
                raise ExceptionPexpect ('isalive() encountered condition where "terminated" is 0, but there was no child process. Did someone else call waitpid() on our process?')
            else:
                raise e

        # I have to do this twice for Solaris. I can't even believe that I figured this out...
        # If waitpid() returns 0 it means that no child process wishes to
        # report, and the value of status is undefined.
        if pid == 0:
            try:
                pid, status = os.waitpid(self.pid, waitpid_options) ### os.WNOHANG) # Solaris!
            except OSError, e: # This should never happen...
                if e[0] == errno.ECHILD:
                    raise ExceptionPexpect ('isalive() encountered condition that should never happen. There was no child process. Did someone else call waitpid() on our process?')
                else:
                    raise e

            # If pid is still 0 after two calls to waitpid() then
            # the process really is alive. This seems to work on all platforms, except
            # for Irix which seems to require a blocking call on waitpid or select, so I let read_nonblocking
            # take care of this situation (unfortunately, this requires waiting through the timeout).
            if pid == 0:
                return True

        if pid == 0:
            return True

        if os.WIFEXITED (status):
            self.status = status
            self.exitstatus = os.WEXITSTATUS(status)
            self.signalstatus = None
            self.terminated = True
        elif os.WIFSIGNALED (status):
            self.status = status
            self.exitstatus = None
            self.signalstatus = os.WTERMSIG(status)
            self.terminated = True
        elif os.WIFSTOPPED (status):
            raise ExceptionPexpect ('isalive() encountered condition where child process is stopped. This is not supported. Is some other process attempting job control with our child pid?')
        return False

    def kill(self, sig):
        """This sends the given signal to the child application.
        In keeping with UNIX tradition it has a misleading name.
        It does not necessarily kill the child unless
        you send the right signal.
        """
        # Same as os.kill, but the pid is given for you.
        if self.isalive():
            os.kill(self.pid, sig)

    def compile_pattern_list(self, patterns):
        """This compiles a pattern-string or a list of pattern-strings.
        Patterns must be a StringType, EOF, TIMEOUT, SRE_Pattern, or 
        a list of those. Patterns may also be None which results in
        an empty list.

        This is used by expect() when calling expect_list().
        Thus expect() is nothing more than::
             cpl = self.compile_pattern_list(pl)
             return self.expect_list(clp, timeout)

        If you are using expect() within a loop it may be more
        efficient to compile the patterns first and then call expect_list().
        This avoid calls in a loop to compile_pattern_list():
             cpl = self.compile_pattern_list(my_pattern)
             while some_condition:
                ...
                i = self.expect_list(clp, timeout)
                ...
        """
        if patterns is None:
            return []
        if type(patterns) is not types.ListType:
            patterns = [patterns]

        compile_flags = re.DOTALL # Allow dot to match \n
        if self.ignorecase:
            compile_flags = compile_flags | re.IGNORECASE
        compiled_pattern_list = []
        for p in patterns:
            if type(p) is types.StringType:
                compiled_pattern_list.append(re.compile(p, compile_flags))
            elif p is EOF:
                compiled_pattern_list.append(EOF)
            elif p is TIMEOUT:
                compiled_pattern_list.append(TIMEOUT)
            elif type(p) is type(re.compile('')):
                compiled_pattern_list.append(p)
            else:
                raise TypeError ('Argument must be one of StringType, EOF, TIMEOUT, SRE_Pattern, or a list of those type. %s' % str(type(p)))

        return compiled_pattern_list
 
    def expect(self, pattern, timeout = -1, searchwindowsize=None):

        """This seeks through the stream until a pattern is matched.
        The pattern is overloaded and may take several types including a list.
        The pattern can be a StringType, EOF, a compiled re, or a list of
        those types. Strings will be compiled to re types. This returns the
        index into the pattern list. If the pattern was not a list this
        returns index 0 on a successful match. This may raise exceptions for
        EOF or TIMEOUT. To avoid the EOF or TIMEOUT exceptions add
        EOF or TIMEOUT to the pattern list.

        After a match is found the instance attributes
        'before', 'after' and 'match' will be set.
        You can see all the data read before the match in 'before'.
        You can see the data that was matched in 'after'.
        The re.MatchObject used in the re match will be in 'match'.
        If an error occured then 'before' will be set to all the
        data read so far and 'after' and 'match' will be None.

        If timeout is -1 then timeout will be set to the self.timeout value.

        Note: A list entry may be EOF or TIMEOUT instead of a string.
        This will catch these exceptions and return the index
        of the list entry instead of raising the exception.
        The attribute 'after' will be set to the exception type.
        The attribute 'match' will be None.
        This allows you to write code like this:
                index = p.expect (['good', 'bad', pexpect.EOF, pexpect.TIMEOUT])
                if index == 0:
                    do_something()
                elif index == 1:
                    do_something_else()
                elif index == 2:
                    do_some_other_thing()
                elif index == 3:
                    do_something_completely_different()
        instead of code like this:
                try:
                    index = p.expect (['good', 'bad'])
                    if index == 0:
                        do_something()
                    elif index == 1:
                        do_something_else()
                except EOF:
                    do_some_other_thing()
                except TIMEOUT:
                    do_something_completely_different()
        These two forms are equivalent. It all depends on what you want.
        You can also just expect the EOF if you are waiting for all output
        of a child to finish. For example:
                p = pexpect.spawn('/bin/ls')
                p.expect (pexpect.EOF)
                print p.before

        If you are trying to optimize for speed then see expect_list().
        """
        compiled_pattern_list = self.compile_pattern_list(pattern)
        return self.expect_list(compiled_pattern_list, timeout, searchwindowsize)

    def expect_list(self, pattern_list, timeout = -1, searchwindowsize = -1):
        """This takes a list of compiled regular expressions and returns 
        the index into the pattern_list that matched the child output.
        The list may also contain EOF or TIMEOUT (which are not
        compiled regular expressions). This method is similar to
        the expect() method except that expect_list() does not
        recompile the pattern list on every call.
        This may help if you are trying to optimize for speed, otherwise
        just use the expect() method.  This is called by expect().
        If timeout==-1 then the self.timeout value is used.
        If searchwindowsize==-1 then the self.searchwindowsize value is used.
        """

        self.patterns = pattern_list

        if timeout == -1:
            timeout = self.timeout
        if timeout is not None:
            end_time = time.time() + timeout 
        if searchwindowsize == -1:
            searchwindowsize = self.searchwindowsize

        try:
            incoming = self.buffer
            while True: # Keep reading until exception or return.
                # Sequence through the list of patterns looking for a match.
                first_match = -1
                for cre in pattern_list:
                    if cre is EOF or cre is TIMEOUT: 
                        continue # The patterns for PexpectExceptions are handled differently.
                    if searchwindowsize is None: # search everything
                        match = cre.search(incoming)
                    else:
                        startpos = max(0, len(incoming) - searchwindowsize)
                        match = cre.search(incoming, startpos)
                    if match is None:
                        continue
                    if first_match > match.start() or first_match == -1:
                        first_match = match.start()
                        self.match = match
                        self.match_index = pattern_list.index(cre)
                if first_match > -1:
                    self.buffer = incoming[self.match.end() : ]
                    self.before = incoming[ : self.match.start()]
                    self.after = incoming[self.match.start() : self.match.end()]
                    return self.match_index
                # No match at this point
                if timeout < 0 and timeout is not None:
                    raise TIMEOUT ('Timeout exceeded in expect_list().')
                # Still have time left, so read more data
                c = self.read_nonblocking (self.maxread, timeout)
                time.sleep (0.0001)
                incoming = incoming + c
                if timeout is not None:
                    timeout = end_time - time.time()
        except EOF, e:
            self.buffer = ''
            self.before = incoming
            self.after = EOF
            if EOF in pattern_list:
                self.match = EOF
                self.match_index = pattern_list.index(EOF)
                return self.match_index
            else:
                self.match = None
                self.match_index = None
                raise EOF (str(e) + '\n' + str(self))
        except TIMEOUT, e:
            self.before = incoming
            self.after = TIMEOUT
            if TIMEOUT in pattern_list:
                self.match = TIMEOUT
                self.match_index = pattern_list.index(TIMEOUT)
                return self.match_index
            else:
                self.match = None
                self.match_index = None
                raise TIMEOUT (str(e) + '\n' + str(self))
        except Exception:
            self.before = incoming
            self.after = None
            self.match = None
            self.match_index = None
            raise

    def getwinsize(self):
        """This returns the terminal window size of the child tty.
        The return value is a tuple of (rows, cols).
        """
        if 'TIOCGWINSZ' in dir(termios):
            TIOCGWINSZ = termios.TIOCGWINSZ
        else:
            TIOCGWINSZ = 1074295912L # assume if not defined
        s = struct.pack('HHHH', 0, 0, 0, 0)
        x = fcntl.ioctl(self.fileno(), TIOCGWINSZ, s)
        return struct.unpack('HHHH', x)[0:2]

    def setwinsize(self, r, c):
        """This sets the terminal window size of the child tty.
        This will cause a SIGWINCH signal to be sent to the child.
        This does not change the physical window size.
        It changes the size reported to TTY-aware applications like
        vi or curses -- applications that respond to the SIGWINCH signal.
        """
        # Check for buggy platforms. Some Python versions on some platforms
        # (notably OSF1 Alpha and RedHat 7.1) truncate the value for
        # termios.TIOCSWINSZ. It is not clear why this happens.
        # These platforms don't seem to handle the signed int very well;
        # yet other platforms like OpenBSD have a large negative value for
        # TIOCSWINSZ and they don't have a truncate problem.
        # Newer versions of Linux have totally different values for TIOCSWINSZ.
        # Note that this fix is a hack.
        if 'TIOCSWINSZ' in dir(termios):
            TIOCSWINSZ = termios.TIOCSWINSZ
        else:
            TIOCSWINSZ = -2146929561
        if TIOCSWINSZ == 2148037735L: # L is not required in Python >= 2.2.
            TIOCSWINSZ = -2146929561 # Same bits, but with sign.
        # Note, assume ws_xpixel and ws_ypixel are zero.
        s = struct.pack('HHHH', r, c, 0, 0)
        fcntl.ioctl(self.fileno(), TIOCSWINSZ, s)

    def interact(self, escape_character = chr(29), input_filter = None, output_filter = None):
        """This gives control of the child process to the interactive user
        (the human at the keyboard).
        Keystrokes are sent to the child process, and the stdout and stderr
        output of the child process is printed.
        This simply echos the child stdout and child stderr to the real
        stdout and it echos the real stdin to the child stdin.
        When the user types the escape_character this method will stop.
        The default for escape_character is ^]. This should not be confused
        with ASCII 27 -- the ESC character. ASCII 29 was chosen
        for historical merit because this is the character used
        by 'telnet' as the escape character. The escape_character will
        not be sent to the child process.

        You may pass in optional input and output filter functions.
        These functions should take a string and return a string.
        The output_filter will be passed all the output from the child process.
        The input_filter will be passed all the keyboard input from the user.
        The input_filter is run BEFORE the check for the escape_character.

        Note that if you change the window size of the parent
        the SIGWINCH signal will not be passed through to the child.
        If you want the child window size to change when the parent's
        window size changes then do something like the following example:
            import pexpect, struct, fcntl, termios, signal, sys
            def sigwinch_passthrough (sig, data):
                s = struct.pack("HHHH", 0, 0, 0, 0)
                a = struct.unpack('hhhh', fcntl.ioctl(sys.stdout.fileno(), termios.TIOCGWINSZ , s))
                global p
                p.setwinsize(a[0],a[1])
            p = pexpect.spawn('/bin/bash') # Note this is global and used in sigwinch_passthrough.
            signal.signal(signal.SIGWINCH, sigwinch_passthrough)
            p.interact()
        """
        # Flush the buffer.
        self.stdout.write (self.buffer)
        self.stdout.flush()
        self.buffer = ''
        mode = tty.tcgetattr(self.STDIN_FILENO)
        tty.setraw(self.STDIN_FILENO)
        try:
            self.__interact_copy(escape_character, input_filter, output_filter)
        finally:
            tty.tcsetattr(self.STDIN_FILENO, tty.TCSAFLUSH, mode)

    def __interact_writen(self, fd, data):
        """This is used by the interact() method.
        """
        while data != '' and self.isalive():
            n = os.write(fd, data)
            data = data[n:]
    def __interact_read(self, fd):
        """This is used by the interact() method.
        """
        return os.read(fd, 1000)
    def __interact_copy(self, escape_character = None, input_filter = None, output_filter = None):
        """This is used by the interact() method.
        """
        while self.isalive():
            r,w,e = self.__select([self.child_fd, self.STDIN_FILENO], [], [])
            if self.child_fd in r:
                data = self.__interact_read(self.child_fd)
                if output_filter: data = output_filter(data)
                if self.logfile is not None:
                    self.logfile.write (data)
                    self.logfile.flush()
                os.write(self.STDOUT_FILENO, data)
            if self.STDIN_FILENO in r:
                data = self.__interact_read(self.STDIN_FILENO)
                if input_filter: data = input_filter(data)
                i = data.rfind(escape_character)
                if i != -1:
                    data = data[:i]
                    self.__interact_writen(self.child_fd, data)
                    break
                self.__interact_writen(self.child_fd, data)
    def __select (self, iwtd, owtd, ewtd, timeout=None):
        """This is a wrapper around select.select() that ignores signals.
        If select.select raises a select.error exception and errno is an EINTR error then
        it is ignored. Mainly this is used to ignore sigwinch (terminal resize).
        """
        # if select() is interrupted by a signal (errno==EINTR) then
        # we loop back and enter the select() again.
        if timeout is not None:
            end_time = time.time() + timeout 
        while True:
            try:
                return select.select (iwtd, owtd, ewtd, timeout)
            except select.error, e:
                if e[0] == errno.EINTR:
                    # if we loop back we have to subtract the amount of time we already waited.
                    if timeout is not None:
                        timeout = end_time - time.time()
                        if timeout < 0:
                            return ([],[],[])
                else: # something else caused the select.error, so this really is an exception
                    raise

##############################################################################
# The following methods are no longer supported or allowed..                
    def setmaxread (self, maxread):
        """This method is no longer supported or allowed.
        I don't like getters and setters without a good reason.
        """
        raise ExceptionPexpect ('This method is no longer supported or allowed. Just assign a value to the maxread member variable.')
    def expect_exact (self, pattern_list, timeout = -1):
        """This method is no longer supported or allowed.
        It was too hard to maintain and keep it up to date with expect_list.
        Few people used this method. Most people favored reliability over speed.
        The implementation is left in comments in case anyone needs to hack this
        feature back into their copy.
        If someone wants to diff this with expect_list and make them work
        nearly the same then I will consider adding this make in.
        """
        raise ExceptionPexpect ('This method is no longer supported or allowed.')
    def setlog (self, fileobject):
        """This method is no longer supported or allowed.
        """
        raise ExceptionPexpect ('This method is no longer supported or allowed. Just assign a value to the logfile member variable.')

##############################################################################
# End of spawn class
##############################################################################

def which (filename):
    """This takes a given filename; tries to find it in the environment path; 
    then checks if it is executable.
    This returns the full path to the filename if found and executable.
    Otherwise this returns None.
    """
    # Special case where filename already contains a path.
    if os.path.dirname(filename) != '':
        if os.access (filename, os.X_OK):
            return filename

    if not os.environ.has_key('PATH') or os.environ['PATH'] == '':
        p = os.defpath
    else:
        p = os.environ['PATH']

    # Oddly enough this was the one line that made Pexpect
    # incompatible with Python 1.5.2.
    #pathlist = p.split (os.pathsep) 
    pathlist = string.split (p, os.pathsep)

    for path in pathlist:
        f = os.path.join(path, filename)
        if os.access(f, os.X_OK):
            return f
    return None

def split_command_line(command_line):
    """This splits a command line into a list of arguments.
    It splits arguments on spaces, but handles
    embedded quotes, doublequotes, and escaped characters.
    It's impossible to do this with a regular expression, so
    I wrote a little state machine to parse the command line.
    """
    arg_list = []
    arg = ''

    # Constants to name the states we can be in.
    state_basic = 0
    state_esc = 1
    state_singlequote = 2
    state_doublequote = 3
    state_whitespace = 4 # The state of consuming whitespace between commands.
    state = state_basic

    for c in command_line:
        if state == state_basic or state == state_whitespace:
            if c == '\\': # Escape the next character
                state = state_esc
            elif c == r"'": # Handle single quote
                state = state_singlequote
            elif c == r'"': # Handle double quote
                state = state_doublequote
            elif c.isspace():
                # Add arg to arg_list if we aren't in the middle of whitespace.
                if state == state_whitespace:
                    None # Do nothing.
                else:
                    arg_list.append(arg)
                    arg = ''
                    state = state_whitespace
            else:
                arg = arg + c
                state = state_basic
        elif state == state_esc:
            arg = arg + c
            state = state_basic
        elif state == state_singlequote:
            if c == r"'":
                state = state_basic
            else:
                arg = arg + c
        elif state == state_doublequote:
            if c == r'"':
                state = state_basic
            else:
                arg = arg + c

    if arg != '':
        arg_list.append(arg)
    return arg_list

