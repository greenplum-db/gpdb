#!/usr/bin/env python3
#
# Copyright (c) Greenplum Inc 2008. All Rights Reserved. 
#

import os
import pipes

from gppylib.gplog import *
from gppylib.gparray import *
from .base import *
from .unix import *
from gppylib.commands.base import *

logger = get_default_logger()

GPHOME=os.environ.get('GPHOME')

# FIXME: Use ReadPostmasterPidFile instead?
class DbStatus(Command):
    def __init__(self,name,db,ctxt=LOCAL,remoteHost=None):
        self.db=db        
        self.cmdStr="$GPHOME/bin/pg_ctl -D %s status" % (db.getSegmentDataDirectory())
        Command.__init__(self,name,self.cmdStr,ctxt,remoteHost)

    def is_running(self):
        if self.results.rc != 0:
            return False
        elif self.results.stdout.lower().find('no server running') != -1:
            return False
        else:
            return True
    
    @staticmethod
    def local(name,db):
        cmd=DbStatus(name,db)
        cmd.run(validateAfter=False)
        return cmd.is_running()
    
    @staticmethod
    def remote(name,db,remoteHost):
        cmd=DbStatus(name,db,ctxt=REMOTE,remoteHost=remoteHost)
        cmd.run(validateAfter=False)
        return cmd.is_running()
    
class ReloadDbConf(Command):
    def __init__(self,name,db,ctxt=LOCAL,remoteHost=None):
        self.db=db
        cmdStr="$GPHOME/bin/pg_ctl reload -D %s" % (db.getSegmentDataDirectory())
        Command.__init__(self,name,cmdStr,ctxt,remoteHost)
    
    @staticmethod
    def local(name,db):
        cmd=ReloadDbConf(name,db)
        cmd.run(validateAfter=True)
        return cmd
        
class ReadPostmasterPidFile(Command):
    def __init__(self, name, datadir, host=None):
        self.datadir = datadir
        self.host = host
        self.cmdStr = "cat %s/postmaster.pid" % datadir
        self.reason = None
        ctxt = LOCAL if host is None else REMOTE
        Command.__init__(self, name, self.cmdStr, ctxt=ctxt, remoteHost=host)

    def validate(self):
        if not self.results.completed or self.results.halt:
            raise ExecutionError("Command did not complete successfully rc: %s" % self.results.rc, self)
    
    def getResult(self):
        if self.results.stderr.find("No such file or directory") != -1:
            self.reason = 'NotFound'
            return None
        if self.results.stdout is None:
            self.reason = 'NotReady'
            return None

        lines = self.results.stdout.splitlines()
        if len(lines) < 8:
            logger.info("postmaster.pid is not ready %s" % self.host)
            self.reason = 'NotReady'
            return None

        pid = int(lines[0].strip())

        ctxt = LOCAL if self.host is None else REMOTE
        cmd = Command("test pid", "kill -0 %s" % str(pid), ctxt=ctxt, remoteHost=self.host)
        cmd.run(validateAfter=False)
        if cmd.results.rc != 0:
            logger.info("check process %s faild: %s" % (str(pid), cmd.results.stderr))
            self.reason = 'NotAlive'
            return None

        result = {}
        result['pid'] = pid
        result['datadir'] = lines[1].strip()
        result['timestamp'] = lines[2].strip()
        result['port'] = int(lines[3].strip())
        result['sockdir'] = lines[4].strip()
        result['listenAddr'] = lines[5].strip()
        result['shm'] = lines[6].strip()
        result['pmstatus'] = lines[7].strip()
        self.reason = 'OK'

        return result
        
# FIXME: Use ReadPostmasterPidFile instead?
class ReadPostmasterTempFile(Command):
    def __init__(self,name,port,ctxt=LOCAL,remoteHost=None):
        self.port=port
        self.cmdStr="cat /tmp/.s.PGSQL.%s.lock" % port
        Command.__init__(self,name,self.cmdStr,ctxt,remoteHost)        
    
    def validate(self):
        if not self.results.completed or self.results.halt:
            raise ExecutionError("Command did not complete successfully rc: %d" % self.results.rc, self)   
    
    def getResults(self):
        if self.results.stderr.find("No such file or directory") != -1:
            return (False,-1,None)
        if self.results.stdout is None:
            return (False,-2,None)
        
        lines = self.results.stdout.split()
        
        if len(lines) < 2:
            return (False,-3,None)
        
        PID=int(self.results.stdout.split()[0])
        datadir = self.results.stdout.split()[1]
        
        return (True,PID,datadir)
    

    @staticmethod
    def local(name,port):
        cmd=ReadPostmasterTempFile(name,port)
        cmd.run(validateAfter=True)
        return cmd
        
                
    @staticmethod
    def remote(name,port,host):
        cmd=ReadPostmasterTempFile(name,port,ctxt=REMOTE,remoteHost=host)
        cmd.run(validateAfter=True)
        return cmd
 

def getProcWithParent(host,targetParentPID,procname):    
    """ returns (parentPID,procPID) tuple for the procname with the specified parent """
    cmdStr="ps -ef | grep '%s' | grep -v grep" % (procname)
    cmd=Command("ps",cmdStr,ctxt=REMOTE,remoteHost=host)
    cmd.run(validateAfter=True)
    
    sout=cmd.get_results().stdout
    
    logger.info(cmd.get_results().printResult())
    
    if sout is None:
        return (0,0)
    
    lines=sout.split('\n')
        
    for line in lines:
        if line == '':
            continue
        fields=line.lstrip(' ').split()
        if len(fields) < 3:
            logger.info("not enough fields line: '%s'" %  line)
            return (0,0)
            
        procPID=int(line.split()[1])
        parentPID=int(line.split()[2])
        if parentPID == targetParentPID:
            return (parentPID,procPID)
    
    logger.info("couldn't find process with name: %s which is a child of PID: %s" % (procname,targetParentPID))
    return (0,0)


# FIXME: Use ReadPostmasterPidFile instead?
def getPostmasterPID(db):
    datadir = db.getSegmentDataDirectory()
    hostname = db.getSegmentHostName()
    cmdStr="ps -ef | grep 'postgres -D %s' | grep -v grep" % datadir
    name="get postmaster"
    cmd=Command(name,cmdStr,ctxt=REMOTE,remoteHost=hostname)
    cmd.run(validateAfter=True)
    logger.critical(cmd.cmdStr)
    logger.critical(cmd.get_results().printResult())
    sout=cmd.get_results().stdout.lstrip(' ')
    return int(sout.split()[1])

def killPgProc(db,procname,signal):
    postmasterPID=getPostmasterPID(db)
    hostname=db.getSegmentHostName()
    if procname == "postmaster":
        procPID = postmasterPID
        parentPID = 0
    else:
        (parentPID,procPID)=getProcWithParent(hostname,postmasterPID,procname)
    if procPID == 0:
        raise Exception("Invalid PID: '0' to kill.  parent postmaster PID: %s" % postmasterPID)
    cmd=Kill.remote("kill "+procname,procPID,signal,hostname)
    return (parentPID,procPID)

class PgControlData(Command):
    def __init__(self, name, datadir, ctxt=LOCAL, remoteHost=None):
        self.datadir = datadir
        self.remotehost=remoteHost
        self.data = None
        Command.__init__(self, name, "$GPHOME/bin/pg_controldata %s" % self.datadir, ctxt, remoteHost)

    def get_value(self, name):
        if not self.results:
            raise Exception('Command not yet executed')
        if not self.data:
            self.data = {}
            for l in self.results.stdout.split('\n'):
                if len(l) > 0:
                    (n,v) = l.split(':', 1)
                    self.data[n.strip()] = v.strip() 
        return self.data[name]

    def get_datadir(self):
        return self.datadir


class PgBaseBackup(Command):
    def __init__(self, pgdata, host, port, create_slot=False, replication_slot_name=None, excludePaths=[], ctxt=LOCAL, remoteHost=None, forceoverwrite=False, target_gp_dbid=0, logfile=None,
                 recovery_mode=True):
        cmd_tokens = ['pg_basebackup', '-c', 'fast']
        cmd_tokens.append('-D')
        cmd_tokens.append(pgdata)
        cmd_tokens.append('-h')
        cmd_tokens.append(host)
        cmd_tokens.append('-p')
        cmd_tokens.append(port)

        if create_slot:
            cmd_tokens.append('--create-slot')

        cmd_tokens.extend(self._xlog_arguments(replication_slot_name))

        # GPDB_12_MERGE_FIXME: avoid checking checksum for heap tables
        # till we code logic to skip/verify checksum for
        # appendoptimized tables. Enabling this results in basebackup
        # failures with appendoptimized tables.
        cmd_tokens.append('--no-verify-checksums')

        if forceoverwrite:
            cmd_tokens.append('--force-overwrite')

        if recovery_mode:
            cmd_tokens.append('--write-recovery-conf')

        # This is needed to handle Greenplum tablespaces
        cmd_tokens.append('--target-gp-dbid')
        cmd_tokens.append(str(target_gp_dbid))

        # We exclude certain unnecessary directories from being copied as they will greatly
        # slow down the speed of gpinitstandby if containing a lot of data
        if excludePaths is None or len(excludePaths) == 0:
            cmd_tokens.append('-E')
            cmd_tokens.append('./db_dumps')
            cmd_tokens.append('-E')
            cmd_tokens.append('./promote')
        else:
            for path in excludePaths:
                cmd_tokens.append('-E')
                cmd_tokens.append(path)

        cmd_tokens.append('--progress')
        cmd_tokens.append('--verbose')

        if logfile:
            cmd_tokens.append('> %s 2>&1' % pipes.quote(logfile))

        cmd_str = ' '.join(cmd_tokens)

        self.command_tokens = cmd_tokens

        Command.__init__(self, 'pg_basebackup', cmd_str, ctxt=ctxt, remoteHost=remoteHost)

    @staticmethod
    def _xlog_arguments(replication_slot_name):
        if replication_slot_name:
            return ["--slot", replication_slot_name, "--wal-method", "stream"]
        else:
            return ["--wal-method", "fetch"]
