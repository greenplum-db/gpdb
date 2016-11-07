#!/usr/bin/env python
#
# Copyright (c) Commvault Systems (2016). All Rights Reserved.
#
'''
Commvault integration with Greenplum data protection

This Module contains some helper functions for storing information needed
to run Greenplum backup and restore using Commvault

Typical usage:

  from gppylib import CvContext

  CvCtx = CvContext()

  if options.verbose:
    gplog.enable_verbose_logging()

  if options.quiet:
    gplog.quiet_stdout_logging()

  logger.info("Start myTool")
  ...

'''
import datetime
import os
import errno
import sys
import re
from gppylib import gplog
from gppylib.commands.base import Command
import threading
import time
from Queue import Queue
import fcntl
try:
    import subprocess32 as subprocess
except:
    import subprocess

logger = gplog.get_default_logger()

class CvContext():
    def __init__(self, cv_clientname, cv_instance, cv_proxy_host, cv_proxy_port, cv_proxy_file, cv_dbname=None, cv_jobType='BACKUP', incremental=False, verbose=False, debuglevel=0):
        #-- Initialize context
        self.cv_job_id = 0
        self.cv_job_token = 0
        self._commcellid = 0
        self.cv_clientid = 0
        self.cv_instanceid = 0
        self._cv_subclient_id = 0
        self.cv_appid = '0:0'
        self.cv_backupsetid = 0
        self.cv_jobstatus = 1
        self.cv_prefix = None
        self._backup_file_list = []
        self._backup_file_guids = {}
        self.cv_apptype = 'Q_DISTRIBUTED_IDA'
        self.cv_logfile = os.path.dirname(gplog.get_logfile()) + "/cv_backup.log"

        if debuglevel > 0:
            self.cv_debuglvl = debuglevel
        elif verbose:
            self.cv_debuglvl = 1
        else:
            self.cv_debuglvl = 0
        self.queue = Queue()
        logger.debug("Logging Commvault-specific entries to: %s\n", self.cv_logfile)
        if cv_clientname is not None:
            self.cv_clientname = cv_clientname
        if cv_instance is not None:
            self.cv_instance = cv_instance

        command_string = "CVBkpRstWrapper -jobStart --cv-streams 1 --cv-clientname %s --cv-instance %s  --cv-debuglvl %s" % (self.cv_clientname, self.cv_instance, self.cv_debuglvl)
        if cv_dbname is not None:
            self.cv_subclient = "cv%s" % cv_dbname
            command_string += " --cv-subclient %s" % self.cv_subclient
        if cv_jobType == "RESTORE":
            command_string += " --cv-jobtype RESTORE"
        else:
            command_string += " --cv-jobtype BACKUP"
            if incremental:
                command_string += " --cv-bkplvl INCR"
            else:
                command_string += " --cv-bkplvl FULL"

        if cv_proxy_host is not None:
            self.cv_proxy_host = cv_proxy_host
            if cv_proxy_port is not None:
                self.cv_proxy_port = cv_proxy_port
            else:
                self.cv_proxy_port = 8400
            command_string += " --cv-proxy-host %s --cv-proxy-port %s" % (self.cv_proxy_host, self.cv_proxy_port)
        elif cv_proxy_file is not None:
                self.cv_proxy_file = cv_proxy_file

        command_string += " --cv-apptype Q_DISTRIBUTED_IDA"

        #-- Start a polling thread for  CVBkpRStWrapper wrapper
        t = threading.Thread(target=cv_job_controller, name='cv_job_controller',args=[self,command_string],kwargs={})
        t.start()

    def get_backup_files(self, timestamp, dbname=None):
        if timestamp > 0:
            logger.debug("Searching for browse times for timestamp %s" % timestamp)
            query_string = "CVBkpRstWrapper -query --cv-proxy-host %s --cv-proxy-port %s --cv-appid %s --cv-apptype Q_DISTRIBUTED_IDA --cv-clientid %s --cv-instanceId %s --cv-backupsetId %s --cv-filename \"*%s.rpt\" --cv-debuglvl %s --cv-search-allcycles 1" % (self.cv_proxy_host, self.cv_proxy_port, self.cv_appid, self.cv_clientid, self.cv_instanceid, self.cv_backupsetid, timestamp, self.cv_debuglvl)
            logger.debug("Command string for get_backup_files: %s\n", query_string)
            query = Command("Getting file info from the Commserve", query_string)
            query.run(validateAfter=True)

            file_info = query.get_results().stdout.split('\n')
            if len(file_info) > 0:
                for line in file_info:
                    if len(line.strip()) > 0:
                        (fname, oguid, cvguid, fromtime, totime, self._commcellid, self._cv_subclient_id) = line.strip().split(':')
                        self.cv_prefix = fname[(fname.rfind("/") + 1):fname.rfind("gp_")]
                        self.cv_subclient = self.cv_prefix[:-1]
                        self.cv_appid = self._commcellid + ":" + self._cv_subclient_id

                #command_string = "CVBkpRstWrapper -query --cv-proxy-host %s --cv-proxy-port %s --cv-appid %s --cv-apptype Q_DISTRIBUTED_IDA --cv-clientid %s --cv-instanceId %s --cv-backupsetId %s --cv-filename \"/\" --cv-browse-fromtime %s --cv-browse-totime %s --cv-debuglvl %s" % (self.cv_proxy_host, self.cv_proxy_port, self.cv_appid, self.cv_clientid, self.cv_instanceid, self.cv_backupsetid, fromtime, totime, self.cv_debuglvl)
                command_string = "CVBkpRstWrapper -query --cv-proxy-host %s --cv-proxy-port %s --cv-appid %s --cv-apptype Q_DISTRIBUTED_IDA --cv-clientid %s --cv-filename \"/\" --cv-browse-fromtime %s --cv-browse-totime %s --cv-debuglvl %s" % (self.cv_proxy_host, self.cv_proxy_port, self.cv_appid, self.cv_clientid, fromtime, totime, self.cv_debuglvl)
            else:
                raise Exception("No backup files found for timestamp %s" % timestamp)

        elif dbname is not None:
            command_string = "CVBkpRstWrapper -query --cv-proxy-host %s --cv-proxy-port %s --cv-appid %s --cv-instanceId %s --cv-backupsetId %s --cv-apptype Q_DISTRIBUTED_IDA --cv-clientid %s --cv-filename \"*%s*\" --cv-debuglvl %s" % (self.cv_proxy_host, self.cv_proxy_port, self.cv_appid, self.cv_instanceid, self.cv_backupsetid, self.cv_clientid, dbname, self.cv_debuglvl)
        else:
            command_string = "CVBkpRstWrapper -query --cv-proxy-host %s --cv-proxy-port %s --cv-appid %s --cv-apptype Q_DISTRIBUTED_IDA --cv-clientid %s --cv-filename \"*\" --cv-debuglvl %s" % (self.cv_proxy_host, self.cv_proxy_port, self.cv_appid, self.cv_clientid, self.cv_debuglvl)
        logger.debug("Command string for get_backup_files': %s\n", command_string)
        cmd = Command("Getting list of backup files from the Commserve", command_string)
        cmd.run(validateAfter=True)

        files_list = cmd.get_results().stdout.split('\n')

        for line in files_list:
            if len(line.strip()) > 0:
                (fname, oguid, cvguid, self._commcellid, self._cv_subclient_id) = line.strip().split(':')
                self._backup_file_guids[fname] = cvguid
                logger.debug("Caching file [%s] with GUID [%s]" % (fname, cvguid))
                self.cv_appid = self._commcellid + ":" + self._cv_subclient_id

        self._backup_file_list = sorted(self._backup_file_guids.keys(), None, None, True)

        # For restore scenarios with -s <dbname>
        # look for the restore timestamp in the latest backup report file name
        if timestamp == 0:
            for file in self._backup_file_list:
                if ".rpt" in file:
                    list = file.split('.')
                    list2 = list[0].split('_')
                    timestamp = list2.pop()
                    logger.debug("Found restore timestamp=%s for database=%s",timestamp, dbname)
                    break
            self.cv_prefix = "cv" + dbname + "_"
        return timestamp

    def get_file_guid(self, fname):
        for file in self._backup_file_list:
            if os.path.basename(fname) == os.path.basename(file):
                return self._backup_file_guids[file]
        return False

    def cv_exit(self):
        #self.cv_context.queue.task_done()
        if not self.processHandle.poll():
            logger.debug("Sending GP exit status (%d) to CVBkpRstWrapper",self.cv_jobstatus)
            named_fifo = "/tmp/CVBkpRstWrapper_read_fifo"
	    fd =  -1
	    try:
            	fd = os.open(named_fifo, os.O_WRONLY| os.O_NONBLOCK)
	    	os.write(fd, '%d'%self.cv_jobstatus)
	    except OSError as err:
		logger.debug("CVBkpRstWrapper is NOT listening for GP exit status. errno=%d",err.errno)
            #fcntl.fcntl(fd, fcntl.F_SETFL, os.O_NDELAY | os.O_NONBLOCK)
	    if fd is not -1:
	    	os.close(fd)
        return self.cv_jobstatus

def cv_job_controller(cv_context, command_string):
    logger.debug("Command string for initializing CV context: %s",command_string)
    #cmd = Command("CVContext", command_string)
    #cmd.run(validateAfter=True)
    wrapperOutput = ''
    args = command_string.split()
    cv_context.processHandle = subprocess.Popen(args, stdout=subprocess.PIPE)
    fcntl.fcntl(cv_context.processHandle.stdout, fcntl.F_SETFL, os.O_NDELAY | os.O_NONBLOCK)

    firstTime = True
    while not cv_context.processHandle.poll():
        if firstTime:
            logger.debug("Launched CVBkpRstWrapper controller PID:%d", cv_context.processHandle.pid)
            firstTime = False
        try:
            wrapperOutput = cv_context.processHandle.stdout.read()
        except IOError:
            pass

        if wrapperOutput:
            logger.debug("Received context %s", wrapperOutput)
            wrapperOutput = wrapperOutput.strip()
            for cmdToken in wrapperOutput.split(','):
                (tokId, tokVal) = cmdToken.split('=')
                if tokId == 'commcellId':
                    cv_context._commcellid = tokVal
                elif tokId == 'clientId':
                    cv_context.cv_clientid = tokVal
                elif tokId == 'instanceId':
                    cv_context.cv_instanceid = tokVal
                elif tokId == 'appId':
                    cv_context._cv_subclient_id = tokVal
                elif tokId == 'backupsetId':
                    cv_context.cv_backupsetid = tokVal
                elif tokId == 'jobId':
                    cv_context.cv_job_id = tokVal
                elif tokId == 'jobToken':
                    cv_context.cv_job_token = tokVal
            cv_context.cv_appid = cv_context._commcellid + ":" + cv_context._cv_subclient_id
            # Notify the main thread to consume clientId, appId, jobId, jobToken from CvContext object
            cv_context.queue.put('CV context updated')
            break;

    #-- Monitor the CVBkpRstWrapper controller session
    while not cv_context.processHandle.poll():
        time.sleep(10); #### set polling timer
        #logger.debug("CV Thread: Polling CVBkpRstWrapper controller PID [%d]....", cv_context.processHandle.pid)

    logger.debug("CVBkpRstWrapper controller session PID [%d] is NOT alive", cv_context.processHandle.pid)

