#!/usr/bin/env python
#
# Copyright (c) Greenplum Inc 2008. All Rights Reserved. 
#

"""
TODO: docs

"""
import os

from gppylib.gplog import *
from gppylib.gparray import *
from base import *
from unix import *

logger = get_default_logger()

GPHOME=os.environ.get('GPHOME')




#----------------------- postgresql.conf ----------------------
#TODO:  what functions?




#----------------------- pg_hba.conf ----------------------
#TODO:  set of functions related to pg_hba.conf including:
#  - reading it in
#  - writing it out
#  - appending to it.




#----------------------- Basic PG maintenance ----------------------

#TODO: set of functions related to basic pg maintenance:
#  - initdb
#  - pg_ctl
#  - pg_config
#  - pg_controldata

#-------------initdb---------------------
class InitDB(Command):
    def __init__(self,name,db,ctxt=LOCAL,remoteHost=None):
        self.db=db
        self.cmdStr="$GPHOME/bin/initdb %s" % (db.getSegmentDataDirectory())
        Command.__init__(self,name,self.cmdStr,ctxt,remoteHost)
    
    @staticmethod
    def local(name,db):
        cmd=InitDB(name,db)
        cmd.run(validateAfter=True)
                
    @staticmethod
    def remote(name,db,host):
        cmd=InitDB(name,db,ctxt=REMOTE,remoteHost=host)
        cmd.run(validateAfter=True) 

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

def killPostmaster(db,signal):
    killPgProc(db,"postmaster",signal)

def getSeqServerPID(db):
    postmaster_pid=getPostmasterPID(db)    
    hostname=db.getSegmentHostName()
    return getProcWithParent(hostname,postmaster_pid,"seqserver")
    
def killSeqServer(db,signal):
    return killPgProc(db,"seqserver",signal)


def getBgWriterPID(db):
    postmaster_pid=getPostmasterPID(db)    
    hostname=db.getSegmentHostName()
    return getProcWithParent(hostname,postmaster_pid,"postgres: writer process")
        
def killBgWriter(db,signal):
    return killPgProc(db, "postgres: writer process",signal)

    
def getStatsCollectorPID(db):
    postmaster_pid=getPostmasterPID(db)    
    hostname=db.getSegmentHostName()
    return getProcWithParent(hostname,postmaster_pid,"postgres: stats collector")
    
def killStatsCollector(db,signal):
    return killPgProc(db,"postgres: stats collector",signal)

def getWALSendServerPID(db):
    postmaster_pid=getPostmasterPID(db)    
    hostname=db.getSegmentHostName()
    return getProcWithParent(hostname,postmaster_pid,"postgres: WAL Send Server")

def killWALSendServer(db,signal):
    return killPgProc(db,"postgres: WAL Send Server",signal)

def getFTSProbePID(db):
    postmaster_pid=getPostmasterPID(db)    
    hostname=db.getSegmentHostName()
    return getProcWithParent(hostname,postmaster_pid,"postgres: ftsprobe process")

def killFTSProbe(db,signal):
    return killPgProc(db,"postgres: ftsprobe process",signal)

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
            raise Exception, 'Command not yet executed'
        if not self.data:
            self.data = {}
            for l in self.results.stdout.split('\n'):
                if len(l) > 0:
                    (n,v) = l.split(':', 1)
                    self.data[n.strip()] = v.strip() 
        return self.data[name]

    def get_datadir(self):
        return self.datadir
