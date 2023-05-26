#!/usr/bin/env python
#
# Copyright (c) Greenplum Inc 2008. All Rights Reserved. 
#

import os
import pipes

from gppylib.gplog import *
from gppylib.gparray import *
from gppylib.db import dbconn
from contextlib import closing
from base import *
from unix import *
from gppylib.commands.base import *
from gppylib.commands.gp import RECOVERY_REWIND_APPNAME
from pygresql import pg

logger = get_default_logger()

GPHOME=os.environ.get('GPHOME')

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


class PgReplicationSlot:
    """
    PgReplicationSlot have utility function related to replication slot
    """
    def __init__(self, host, port, slot_name):
        """
        @param host
        @param port
        @param slot_name
        """
        self.host = host
        self.port = port
        self.name = slot_name

    def slot_exists(self):
        count = -1
        logger.debug('Checking if slot {} exists for host:{}, port:{}'.format(self.name, self.host, self.port))
        sql = "SELECT count(*) FROM pg_catalog.pg_replication_slots WHERE slot_name = '{}'". format(self.name)
        try:
            dburl = dbconn.DbURL(hostname=self.host, port=self.port)
            with closing(dbconn.connect(dburl, utility=True, encoding='UTF8')) as conn:
                count = dbconn.execSQLForSingleton(conn, sql)
        except Exception as ex:
            raise Exception("Failed to query pg_replication_slots for host:{}, port:{}: {}".
                            format(self.host, self.port, str(ex)))

        if count == 0:
            logger.debug("Slot {} does not exist for host:{}, port:{}".
                         format(self.name, self.host, self.port))
            return False

        return True

    def drop_slot(self):
        logger.debug("Dropping slot {} for host:{}, port:{}".format(self.name, self.host, self.port))
        sql = "SELECT pg_drop_replication_slot('{}');".format(self.name)
        try:
            dburl = dbconn.DbURL(hostname=self.host, port=self.port)
            with closing(dbconn.connect(dburl, utility=True, encoding='UTF8')) as conn:
                dbconn.execSQL(conn, sql)
        except pg.DatabaseError as e:
            # one of the case can be where slot is present but currently in active state
            logger.exception("Failed to query pg_drop_replication_slot for host:{}, port:{}: {}".
                             format(self.host, self.port, str(e)))
            return False
        except Exception as ex:
            raise Exception("Failed to drop replication slot for host:{}, port:{} : {}".
                            format(self.host, self.port, str(ex)))

        logger.debug("Successfully dropped replication slot {} for host:{}, port:{}".
                     format(self.name, self.host, self.port))

        return True

    def create_slot(self):
        """
        Execute query SELECT pg_create_physical_replication_slot('slot_name') to
        create replication slot. pg_create_physical_replication_slot function can take two parameters:

        first parameter slot_name:
            Required, name of the replication slot

        Optional second parameter immediately_reserve boolean:
            when true, specifies that the LSN for this replication slot be reserved immediately;
            otherwise the LSN is reserved on first connection from a streaming replication client.

            Second parameter is passed 'True' as we don't want any XLOG to get deleted from start backup
            point till mirror is created and connects to the primary. As if XLOG gets deleted -
            mirror creates and connects and will fail with missing XLOG. We will have to recreate the
            XLOG.
        """

        logger.debug("Creating slot {} for host:{}, port:{}".format(self.name, self.host, self.port))
        sql = "SELECT pg_create_physical_replication_slot('{}', true);".format(self.name)
        try:
            dburl = dbconn.DbURL(hostname=self.host, port=self.port)
            with closing(dbconn.connect(dburl, utility=True, encoding='UTF8')) as conn:
                dbconn.execSQL(conn, sql)
        except pg.DatabaseError as e:
            logger.exception("Failed to query pg_create_physical_replication_slot for host:{}, port:{}: {}".
                             format(self.host, self.port, str(e)))
            return False
        except Exception as ex:
            raise Exception("Failed to create replication slot for host:{}, port:{} : {}".
                            format(self.host, self.port, str(ex)))

        logger.debug("Successfully created replication slot {} for host:{}, port:{}".
                     format(self.name, self.host, self.port))
        return True


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
                    split_line = l.split(':', 1)
                    # avoid ValueErrors when there is no value to the key
                    n = split_line[0]
                    v = split_line[1] if len(split_line) == 2 else ''
                    self.data[n.strip()] = v.strip() 
        return self.data[name]

    def get_datadir(self):
        return self.datadir

class PgRewind(Command):
    """
    PgRewind is used to run pg_rewind using source server.
    """
    def __init__(self, name, target_datadir, source_host, source_port, progress_file):

        # Construct the source server libpq connection string
        # We set application_name here so gpstate can identify whether an
        # incremental recovery is occurring.
        source_server = "host={} port={} dbname=template1 application_name={}".format(
                source_host, source_port, RECOVERY_REWIND_APPNAME
                                                   )

        # Build the pg_rewind command. Do not run pg_rewind if standby.signal
        # file exists in target data directory because the target instance can
        # be started up normally as a mirror for WAL replication catch up.
        rewind_cmd = '[ -f %s/recovery.conf ] || PGOPTIONS="-c gp_session_role=utility" $GPHOME/bin/pg_rewind ' \
                     '--write-recovery-conf --slot="internal_wal_replication_slot" --source-server="%s" ' \
                     '--target-pgdata=%s --progress' % (target_datadir, source_server, target_datadir)

        # pg_rewind prints progress updates to stdout, but it also prints
        # errors relating to relevant failures(like it will not rewind due to
        # a corrupted pg_control file) to stderr.
        rewind_cmd = rewind_cmd + " > {} 2>&1".format(pipes.quote(progress_file))
        self.cmdStr = rewind_cmd

        Command.__init__(self, name, self.cmdStr, LOCAL)

class PgBaseBackup(Command):
    def __init__(self, target_datadir, source_host, source_port, replication_slot_name=None,
                 excludePaths=[], ctxt=LOCAL, remoteHost=None, writeconffilesonly=False, forceoverwrite=False,
                 target_gp_dbid=0, progress_file=None, recovery_mode=True):
        cmd_tokens = ['pg_basebackup', '-c', 'fast']
        cmd_tokens.append('-D')
        cmd_tokens.append(target_datadir)
        cmd_tokens.append('-h')
        cmd_tokens.append(source_host)
        cmd_tokens.append('-p')
        cmd_tokens.append(source_port)
        cmd_tokens.extend(self._xlog_arguments(replication_slot_name))

        # In case of differential recovery, this flag is used to only write recovery.conf
        # and internal.auto.conf files to target data directory.
        if writeconffilesonly:
            cmd_tokens.append('--write-conf-files-only')

        else:
            if forceoverwrite:
                cmd_tokens.append('--force-overwrite')

            if recovery_mode:
                cmd_tokens.append('--write-recovery-conf')

            # We exclude certain unnecessary directories from being copied as they will greatly
            # slow down the speed of gpinitstandby if containing a lot of data
            if excludePaths is None or len(excludePaths) == 0:
                cmd_tokens.append('-E')
                cmd_tokens.append('./db_dumps')
                cmd_tokens.append('-E')
                cmd_tokens.append('./gpperfmon/data')
                cmd_tokens.append('-E')
                cmd_tokens.append('./gpperfmon/logs')
                cmd_tokens.append('-E')
                cmd_tokens.append('./promote')
            else:
                for path in excludePaths:
                    cmd_tokens.append('-E')
                    cmd_tokens.append(path)

        # This is needed to handle Greenplum tablespaces
        cmd_tokens.append('--target-gp-dbid')
        cmd_tokens.append(str(target_gp_dbid))

        cmd_tokens.append('--progress')
        cmd_tokens.append('--verbose')

        if progress_file:
            cmd_tokens.append('> %s 2>&1' % pipes.quote(progress_file))

        cmd_str = ' '.join(cmd_tokens)

        self.command_tokens = cmd_tokens

        Command.__init__(self, 'pg_basebackup', cmd_str, ctxt=ctxt, remoteHost=remoteHost)

    @staticmethod
    def _xlog_arguments(replication_slot_name):
        if replication_slot_name:
            return ["--slot", replication_slot_name, "--xlog-method", "stream"]
        else:
            return ['--xlog']
