#!/usr/bin/env python
#
# Copyright (c) Greenplum Inc 2008. All Rights Reserved.
#
# This file contains ssh Session class and support functions/classes.

import cmd
import os
import sys
import socket
import threading
from gppylib.commands.base import WorkerPool, REMOTE, ExecutionError
from gppylib.commands.unix import Hostname, Echo

sys.path.insert(1, sys.path[0] + '/lib')
from pexpect import pxssh

class HostNameError(Exception):
    def __init__(self, msg, lineno = 0):
        if lineno: self.msg = ('%s at line %d' % (msg, lineno))
        else: self.msg = msg
    def __str__(self):
        return self.msg

class SSHError(Exception):
    def __init__(self, msg):
        self.msg = msg
    def __str__(self):
        return self.msg

# Utility Functions
def ssh_prefix(host):
    ssh = 'ssh -o "BatchMode yes" -o "StrictHostKeyChecking no" ' + host
    return ssh

def get_hosts(hostsfile):
    hostlist = HostList()
    hostlist.parseFile(hostsfile)
    return hostlist.get()


class HostList():
    
    def __init__(self): 
        self.list = []

    def get(self): 
        return self.list

    def add(self, host, lineno=0):
        '''Add a host to the hostlist.'''

        # we don't allow the user@ syntax here
        if host.find('@') >= 0:
            raise HostNameError(host, lineno)

        # MPP-13617 - check for ipv6
        if host.find(':') >= 0:
            try:
                socket.inet_pton(socket.AF_INET6, host)
            except socket.error, e:
                raise HostNameError(str(e), lineno)

        # MPP-13617 - check for ipv4
        if host.find('.') >= 0:
            octs = host.split('.')
            if len(octs) == 4 and False not in [o.isdigit() for o in octs]:
                try:
                    socket.inet_pton(socket.AF_INET, host)
                except socket.error, e:
                    raise HostNameError(str(e), lineno)

        self.list.append(host)
        return self.list

    def parseFile(self, path):
        '''Add lines in a file to the hostlist.'''
        with open(path) as fp:
            for i, line in enumerate(fp):
                line = line.strip()
                if not line or line[0] == '#': 
                    continue
                self.add(line, i+1)
        return self.list

    def checkSSH(self):
        '''Check that ssh to hostlist is okay.'''

        pool = WorkerPool()

        for h in self.list:
            cmd = Echo('ssh test', '', ctxt=REMOTE, remoteHost=h)
            pool.addCommand(cmd)
            
        pool.join()
        pool.haltWork()  


        for cmd in pool.getCompletedItems():
            if not cmd.get_results().wasSuccessful():
                raise SSHError("Unable to ssh to '%s'" % cmd.remoteHost)

        return True

    def filterMultiHomedHosts(self):
        '''For multiple host that is of the same node, keep only one in the hostlist.'''
        unique = {}

        pool = WorkerPool()
        for h in self.list:
            cmd = Hostname('hostname', ctxt=REMOTE, remoteHost=h)
            pool.addCommand(cmd)
            
        pool.join()
        pool.haltWork()
        
        for finished_cmd in pool.getCompletedItems():
            hostname = finished_cmd.get_hostname()
            if (not hostname):
                unique[finished_cmd.remoteHost] = finished_cmd.remoteHost
            elif not unique.get(hostname):
                unique[hostname] = finished_cmd.remoteHost
            elif hostname == finished_cmd.remoteHost:
                unique[hostname] = finished_cmd.remoteHost

        self.list = unique.values()
        
        return self.list

# Session is a command session, derived from a base class cmd.Cmd
class Session(cmd.Cmd):
    '''Implements a list of open ssh sessions ready to execute commands'''
    verbose=False
    hostList=[]
    userName=None
    echoCommand=False
    class SessionError(StandardError): pass
    class SessionCmdExit(StandardError): pass

    def __init__(self, hostList=None, userName=None):
        cmd.Cmd.__init__(self)
        self.pxssh_list = []
        self.prompt = '=> '
        self.peerStringFormatRaw = None
        if hostList:
            for host in hostList:
                self.hostList.append(host)
        if userName: self.userName=userName

    def peerStringFormat(self):
        if self.peerStringFormatRaw: return self.peerStringFormatRaw
        cnt = 0
        for p in self.pxssh_list:
            if cnt < len(p.x_peer): cnt = len(p.x_peer)
        self.peerStringFormatRaw = "[%%%ds]" % cnt
        return self.peerStringFormatRaw

    def login(self, hostList=None, userName=None, delaybeforesend=0.05, sync_multiplier=1.0):
        '''This is the normal entry point used to add host names to the object and log in to each of them'''
        if self.verbose: print '\n[Reset ...]'
        if not (self.hostList or hostList):
            raise self.SessionError('No host list available to Login method')
        if not (self.userName or userName):
            raise self.SessionError('No user name available to Login method')
        
        #Cleanup    
        self.clean()
    
        if hostList: #We have a new hostlist to use, initialize it
            self.hostList=[]
            for host in hostList:
                self.hostList.append(host)
        if userName: self.userName=userName  #We have a new userName to use

        # MPP-6583.  Save off term type and set to nothing before creating ssh process
        origTERM = os.getenv('TERM', None)
        os.putenv('TERM', '')

        good_list = []
        print_lock = threading.Lock()

        def connect_host(host):
            self.hostList.append(host)
            p = pxssh.pxssh(delaybeforesend=delaybeforesend,
                            options={"StrictHostKeyChecking": "no",
                                     "BatchMode": "yes"})
            try:
                # The sync_multiplier value is passed onto pexpect.pxssh which is used to determine timeout
                # values for prompt verification after an ssh connection is established.
                p.login(host, self.userName, sync_multiplier=sync_multiplier)
                p.x_peer = host
                p.x_pid = p.pid
                good_list.append(p)
                if self.verbose:
                    with print_lock:
                        print '[INFO] login %s' % host
            except Exception as e:
                with print_lock:
                    print '[ERROR] unable to login to %s' % host
                    if type(e) is pxssh.ExceptionPxssh:
                        print e
                    elif type(e) is pxssh.EOF:
                        print 'Could not acquire connection.'
                    else:
                        print 'hint: use gpssh-exkeys to setup public-key authentication between hosts'

        thread_list = []
        for host in hostList:
            t = threading.Thread(target=connect_host, args=(host,))
            t.start()
            thread_list.append(t)

        for t in thread_list:
            t.join()

        # Restore terminal type
        if origTERM:
            os.putenv('TERM', origTERM)

        self.pxssh_list = good_list

    def close(self):
        return self.clean()
    
    def reset(self):
        '''reads from all the ssh connections to make sure we dont have any pending cruft'''
        for s in self.pxssh_list:
            s.readlines()
                
    def clean(self):
        net_return_code = self.closePxsshList(self.pxssh_list)
        self.pxssh_list = []
        return net_return_code

    def emptyline(self):
        pass

    def escapeLine(self,line):
        '''Escape occurrences of \ and $ as needed and package the line as an "eval" shell command'''
        line = line.strip()
        if line == 'EOF' or line == 'exit' or line == 'quit':
            raise self.SessionCmdExit()
        line = line.split('\\')
        line = '\\\\'.join(line)
        line = line.split('"')
        line = '\\"'.join(line)
        line = line.split('$')
        line = '\\$'.join(line)
        line = 'eval "' + line + '" < /dev/null'
        
        return line

    def executeCommand(self,command):
        commandoutput=[]
        
        if self.echoCommand:
            escapedCommand = command.replace('"', '\\"')
            command = 'echo "%s"; %s' % (escapedCommand, command)
            
        #Execute the command in all of the ssh sessions
        for s in self.pxssh_list:
            s.sendline(command)
            
        #Wait for each command and retrieve the output
        for s in self.pxssh_list:
            #Wait for each command to finish
            #!! TODO verify that this is a tight wait loop and find another way to do this
            while not s.prompt(120) and s.isalive() and not s.eof(): pass
            
        for s in self.pxssh_list:
            #Split the output into an array of lines so that we can add text to the beginning of
            #    each line
            output = s.before.split('\n')
            output = output[1:-1]
                
            commandoutput.append(output)
            
        return commandoutput.__iter__()

# Interactive command line handler
#    Override of base class, handles commands that aren't recognized as part of a predefined set
#    The "command" argument is a command line to be executed on all available command sessions
#    The output of the command execution is printed to the standard output, prepended with
#        the hostname of each machine from which the output came
    def default(self, command):
    
        line = self.escapeLine(command)
    
        if self.verbose: print command
        
    #Execute the command on our ssh sessions
        commandoutput=self.executeCommand(command)
        self.writeCommandOutput(commandoutput)

    def writeCommandOutput(self,commandoutput):
        '''Takes a list of output lists as an iterator and writes them to standard output,
        formatted with the hostname from which each output array was obtained'''
        for s in self.pxssh_list:
            output = commandoutput.next()
            #Write the output
            if len(output) == 0:
                print (self.peerStringFormat() % s.x_peer)
            else:
                for line in output:
                    print (self.peerStringFormat() % s.x_peer), line
    
    def closePxsshList(self,list):
        lock = threading.Lock()
        return_codes = [0]
        def closePxsshOne(p, return_codes): 
            p.logout()
            with lock:
                return_codes.append(p.exitstatus)
        th = []
        for p in list:
            t = threading.Thread(target=closePxsshOne, args=(p, return_codes))
            t.start()
            th.append(t)
        for t in th:
            t.join() 
        return max(return_codes)
