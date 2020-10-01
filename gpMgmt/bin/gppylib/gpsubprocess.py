#!/usr/bin/env python3
#
# Copyright (c) Greenplum Inc 2008. All Rights Reserved. 
#

"""
The gpsubprocess is a subclass of subprocess that allows for non-blocking
processing of status and servicing of stdout and stderr.  

Also the aim was to reduce the overhead associated with this servicing.
Normal subprocess.communicate() results in up to 3 threads being created 
(stdin,stdout,stderr) and then a blocking wait for the process and threads
to finish.  This doesn't allow the user to be able to cancel long running
tasks.


"""
import errno
import fcntl
import os
import select
import time
import subprocess
from gppylib import gplog


logger=gplog.get_default_logger()



class Popen(subprocess.Popen):
    
    cancelRequested=False

    
    def communicate2(self, timeout=2,input=None):
        """ An extension to communicate() that allows for external cancels
        to abort processing.  

        All internal I/O calls are non-blocking.

        The timeout is in seconds and is the max. amount of time to wait in 
        select() for something to read from either stdout or stderr.  This 
        then effects how responsive it will be to cancel requests.
        """
        terminated=False
        
        output = []
        error = []

        self._setupNonblocking()

        if self.stdin:
            if input:
                if isinstance(input, str):
                    input = str.encode(input)
                self.stdin.write(input)

            # always close stdin, even when no input from caller
            self.stdin.close()
                
        while not (terminated or self.cancelRequested):
            terminated = self._check_status()

            if not terminated:
                self._read_files(timeout,output,error)

        # Consume rest of output
        self._finish_read_files(timeout,output,error)
                      
        (resout,reserr)=self._postprocess_outputs(output,error)

        return (self.returncode,resout,reserr)


    def cancel(self):
        """Sets a flag that will cause execution to halt during the next 
        select cycle.
        
        This amount of time is based on the timeout specified.
        """
        logger.debug("cancel is requested")
        self.cancelRequested=True
        
        

    
    def _setupNonblocking(self):
        """ sets stdout and stderr  fd's to be non-blocking.
        The fcntl throws an IOError if these calls fail. 
        """        
        fcntl.fcntl(self.stdout, fcntl.F_SETFL, os.O_NDELAY | os.O_NONBLOCK)
        fcntl.fcntl(self.stderr, fcntl.F_SETFL, os.O_NDELAY | os.O_NONBLOCK)

                
    def _postprocess_outputs(self,output,error):
        # All data exchanged.  Translate lists into bytestrings
        if output is not None:
            output = b''.join(output)
        if error is not None:
            error = b''.join(error)

        # Translate newlines, if requested.  We cannot let the file
        # object do the translation: It is based on stdio, which is
        # impossible to combine with select (unless forcing no
        # buffering).
        if self.universal_newlines and hasattr(file, 'newlines'):
            if output:
                output = self._translate_newlines(output)
            if error:
                error = self._translate_newlines(error)    
        return (output,error)
    
    
    def _read_files(self,timeout,output,error):
        readList=[]
        readList.append(self.stdout)
        readList.append(self.stderr)
        writeList = []
        errorList=[]
        (rset,wset,eset) = select.select(readList,writeList, errorList, timeout)

        if self.stdout in rset:
            output.append(os.read(self.stdout.fileno(),8192))

        if self.stderr in rset:
            error.append(os.read(self.stderr.fileno(),8192))
            
    
    def _finish_read_files(self, timeout, output, error):
        """This function reads the rest of stderr and stdout and appends
        it to error and output.  This ensures that all output is received"""
        bytesRead=0
        
        # consume rest of output
        try:
            (rset,wset,eset) = select.select([self.stdout],[],[], timeout)
            while (self.stdout in rset):
                buffer = os.read(self.stdout.fileno(), 8192)
                if not buffer:
                    break
                else:
                    output.append(buffer)
                (rset,wset,eset) = select.select([self.stdout],[],[], timeout)
        except OSError:
            # Pipe closed when we tried to read.  
            pass 
    
        try:
            (rset,wset,eset) = select.select([self.stderr],[],[], timeout)    
            while (self.stderr in rset):
                buffer = os.read(self.stderr.fileno(), 8192)
                if not buffer:
                    break
                else:
                    error.append(buffer)
                (rset, wset, eset) = select.select([self.stderr], [], [], timeout)
        except OSError:
            # Pipe closed when we tried to read.
            pass 
            


        """ Close stdout and stderr PIPEs """
        self.stdout.close()
        self.stderr.close()


    def _check_status(self):
        terminated=False
        """Another possibility for the below line would be to try and capture
        rusage information.  Perhaps that type of call would be an external
        method for users to check on status:        
        (pid, status, rusage)=os.wait4(self.pid, os.WNOHANG)
        """
        (pid,status)=os.waitpid(self.pid, os.WNOHANG)
        

        if pid == 0:
            #means we are still in progress
            return

        exitstatus = os.WEXITSTATUS(status)
        if os.WIFEXITED (status):            
            self.exitstatus = os.WEXITSTATUS(status)
            self.signalstatus = None
            terminated = True
        elif os.WIFSIGNALED (status):
            self.exitstatus = None
            self.signalstatus = os.WTERMSIG(status)
            terminated = True
        elif os.WIFSTOPPED (status):
            raise Exception('Wait was called for a child process that is stopped.\
                             This is not supported. Is some other process attempting\
                             job control with our child pid?')

        self._handle_exitstatus(status)

        return terminated

