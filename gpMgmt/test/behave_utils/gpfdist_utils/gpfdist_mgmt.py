try:
    import time
    import os
    import platform
    from gppylib.commands.base import Command, LOCAL, REMOTE, WorkerPool
except ImportError, import_exception:
    sys.exit('Cannot import modules.  Please check that you have sourced' \
             ' greenplum_path.sh.  Detail: %s' % str(import_exception))
import socket

class Gpfdist:
    """
    Gpfdist class to management create and cleanup of gpfdist instance
    """
    def __init__(self, name, directory, port, pid_file,
                 ctxt=LOCAL, remoteHost=None, remote_source_file=os.path.join(os.environ.get('GPHOME'),'greenplum_path.sh')):
        """  
        name: name of the command
        dir: directory for gpfdist to use as its root directory
        port: port for gpfdist to listen on
        max_line_length: maximum line length setting for gpfdist
        pid_file: full path of the pid file to create
        """

        self.name = name
        self.dir = directory
        self.port = port
        self.pid_file = pid_file
        self.ctxt = ctxt
        self.host = remoteHost
        self.ps_command = 'ps'
        self.source_file = remote_source_file
        if platform.system() in ['SunOS']:
            self.ps_command = '/bin/ps'

    def startGpfdist(self):
        if self.host in ('127.0.0.1',socket.gethostbyname(socket.gethostname()),socket.gethostname(),'localhost'):
            cmdStr = 'nohup gpfdist -d %s -p %s > /dev/null 2> /dev/null < ' \
                    '/dev/null & echo \\$! > %s' % (self.dir, self.port,
                                                    self.pid_file)
        else:
            cmdStr = 'gpssh -h %s -e "source %s; nohup gpfdist -d %s -p %s > /dev/null 2> /dev/null < ' \
                    '/dev/null & echo \\$! > %s"' % (self.host, self.source_file, self.dir, self.port,
                                                    self.pid_file)             

        cmd = Command(self.name, cmdStr, self.ctxt, self.host)
        cmd.run()
        return self.check_gpfdist_process()
    
    def check_gpfdist_process(self, wait=60, port=None, raise_assert=True):
        """
        Check for the gpfdist process
        Wait at least 60s until gpfdist starts, else raise an exception
        """
        if port is None:
            port = self.port
        count = 0
        # handle escape of string's quotation for localhost and remote host
        if self.host in ('127.0.0.1',socket.gethostbyname(socket.gethostname()),socket.gethostname(),'localhost'):
            cmdStr = "%s -ef | grep \'gpfdist -d %s -p %s\' | grep -v grep"%(self.ps_command, self.dir, port)
        else:
            cmdStr = 'gpssh -h %s -e "%s -ef | grep \'gpfdist -d %s -p %s\' |grep -v grep"'%(self.host, self.ps_command, self.dir, port)
        cmd = Command(self.name, cmdStr, self.ctxt, self.host)
        # run the command for 5 time
        while count < wait:
            cmd.run()      
            results = cmd.get_results()      
            if results.rc == 0:
                return True                
            count = count + 1
            time.sleep(1)
        if raise_assert:
            raise GPFDISTError("Could not start gpfdist process")
        else :
            return False

    def is_gpfdist_connected(self, port=None):
        """
        Check gpfdist by connecting after starting process
        @return: True or False
        @todo: Need the absolute path
        """
        if port is None:
            port = self.port

        url = "http://%s:%s" % (socket.gethostbyname(self.host), port)

        shell.run("curl %s 2>&1" % (url), oFile="gpfdist.process", mode="w")
        content = ''.join(open("gpfdist.process").readlines())
        if content.find("couldn't")>=0:
            return False
        return True

    def is_port_released(self, port=None):
        """
        Check whether the port is released after stopping gpfdist
        @return: True or False
        """
        if port is None:
            port = self.port
        # deal with the escape issue separately for localhost and remote one
        if self.host in ('127.0.0.1',socket.gethostbyname(socket.gethostname()),socket.gethostname(),'localhost'):
            cmdStr = "netstat -an |grep %s"%port
        else:
            cmdStr = 'gpssh -h %s -e "netstat -an |grep %s"'%(self.host, port)
        cmd = Command(self.name, cmdStr, self.ctxt, self.host)
        cmd.run()
        results = cmd.get_results()
        return (not results.rc)
       

    def cleanupGpfdist(self, wait=10, port=None):
        """  
        Command for terminating a running gpfdist instance and removing its pid file
        """
        # We have to kill and then wait for the process to fully exit so we
        # can reuse the port.  Without this delay the next gpfdist instance
        # that tries to start using the same port can fail to bind.
        if port is None:
            port = self.port
        # deal with the escape issue separately for localhost and remote one
        if self.host in ('127.0.0.1',socket.gethostbyname(socket.gethostname()),socket.gethostname(),'localhost'):
            cmdStr = '%s -ef | grep "gpfdist -d %s -p %s" | grep -v grep | awk \'{print $2}\' | xargs kill 2>&1 > /dev/null'%(self.ps_command, self.dir, port)
        else:
            cmdStr = 'gpssh -h %s -e "%s -ef | grep \'gpfdist -d %s -p %s\' | grep -v grep | awk \'{print $2}\' | xargs kill 2>&1 > /dev/null"'%(self.host, self.ps_command, self.dir, port)
        cmd = Command(self.name, cmdStr, self.ctxt, self.host)
        cmd.run()
        is_released = False
        count = 0
        while (not is_released and count < wait):
            is_released = self.is_port_released()
            count = count + 1
            time.sleep(1)

