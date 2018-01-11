"""
Copyright (c) 2004-Present Pivotal Software, Inc.

This program and the accompanying materials are made available under
the terms of the under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

ClusterControl - code for manipulating the state of GPDB individual cluster processes
"""

import sys, os, subprocess, inspect, platform
MYD = os.path.abspath( os.path.dirname( __file__ ) )
mkpath = lambda * x: os.path.join( MYD, *x )
UPD = os.path.abspath( mkpath( '..' ) )
if UPD not in sys.path:
    sys.path.append( UPD )

try:
    # from Config import Config
    # from Shell import shell
    from gppylib.commands.base import Command
    # from gpRemoteCmd import GpRemoteCmd
    from gpdbSegmentConfig import GpdbSegmentConfig
except ImportError, e:
    sys.exit('ClusterControl: Cannot import modules.  Please check that you have sourced greenplum_path.sh.  Detail: ' + str(e))

class SegmentControl(object):

    #remote = GpRemoteCmd()
    segment_config = GpdbSegmentConfig()

    def __init__(self, host='localhost', user=os.environ.get("LOGNAME")):
        """Return a new SegmentControl object
        @param host: hostname on which to run kill commands
        @param user: user to run commands as, defaults to the environment var $LOGNAME
        """
        self.host = host
        self.user = user

    # def getSegmentInfo(self, role="p", contentId=0, killmaster=False):
        # """Get segment process info for a specific segment
        # @param role: 'p' or 'm' for primary or mirror
        # @param contentId: id of segment to get info for
        # """

        # config_role = True
        # if role == 'm':
        #     config_role = False

        # for r in config.record:
        #     print "getSegmentInfo r.content = %s; r.role = %s" % (str(r.content), r.role)
        #     if r.role == config_role and (killmaster or r.content != -1) and r.status and r.content == contentId:
        #         return [r.port, r.hostname]
        #   print "NO MATCH!!!"

    def getSegmentInfo(self, role="p", contentId=0, killmaster=False):
        """Get segment process info for a specific segment
        @param role: 'p' or 'm' for primary or mirror
        @param contentId: id of segment to get info for
        """

        segs = self.segment_config.GetSegmentData(contentId, myRole=role)
        # if no segments found, return None
        if not len(segs):
            return None
        info = segs[0]
        print "segmentInfo = %s" % str(segs)
        return (info['port'], info['hostname'])

    def getMasterInfo(self, role="p"):
        """Get segment process info for the master segment
        @param role: 'p' or 'm' for primary or mirror
        """
        return self.getSegmentInfo(role, -1, True)

    def getFirstSegmentInfo(self,role="p"):
        """Get segment process info for first segment
        @param role: 'p' or 'm' for primary or mirror
        """
        return self.getSegmentInfo(role, 0)

    def killProcessUnix(self, segment,processes=[],signal="9"):
        """Kill a process for a database segment
        @param segment:  segment info, as returned by getSegmentInfo or getFirstSegmentInfo
        @param processes:  list of process pids to kill or 'all'
        @param signal:  UNIX signal to send processes, via kill(1)
        """
        if(platform.uname()[0].lower() == 'sunos'):
            # Get the Parent Process ID first
            cmd = "/usr/ucb/ps auxwww |grep postgres | grep %s |grep -v grep | awk '{print \$2}'" % segment[0]
            # use class Command in gppylib.commands.base
            remote = Command(name='get parent process ID',cmdStr=cmd,remoteHost=self.host)
            remote.run()
            rc = remote.get_results().rc
            if rc != 0:
                raise Exception("get parent process ID failed with rc:  (%d)" % (rc))
            parent_id = remote.get_results().stdout

            if len(parent_id)==0: # Nothing Found
                return None
            cmd = "ps -ef |grep %s |grep -v grep | awk '{print $3}' | xargs pargs" % parent_id[0].strip()
            remote = Command(name='ps -ef',cmdStr=cmd,remoteHost=self.host)
            remote.run()
            rc = remote.get_results().rc
            if rc != 0:
                raise Exception("get process id failed with rc:  (%d)" % (rc))
            process_out = remote.get_results().stdout
            for line in process_out:
                cur = line.split(":")
                try:
                    processName = cur[2]
                    if cur[0]=="argv[0]":
                        cmd = "kill -%s %s" % (signal,curproc)
                        remote = Command(name='kill', cmdStr=cmd,remoteHost=self.host)
                        if processes[0]=="all":
                            remote.run()
                            rc = remote.get_results().rc
                            if rc != 0:
                                raise Exception("kill failed with rc:  (%d)" % (rc))
                            out = remote.get_results().stdout
                        else:
                            for process in processes:
                                if processName.find(process.trim())>=0:
                                    remote.run()
                                    rc = remote.get_results().rc
                                    if rc != 0:
                                        raise Exception("kill failed with rc:  (%d)" % (rc))
                                    out = remote.get_results().stdout
                except:
                    curproc = cur[0] # Get the Process ID first
        else:
            if processes[0]=="all": # Kill all Postgres process
                cmd = "ps -ef |grep postgres | grep %s |grep -v grep | awk '{print $3}' | xargs kill -%s" % (segment[0],signal)
                print cmd
                remote = Command(name='ps -ef', cmdStr=cmd,remoteHost=self.host)
                remote.run()
                rc = remote.get_results().rc

                if rc != 0:
                    raise Exception("ps -ef failed with rc:  (%d)" % (rc))
                out = remote.get_results().stdout
            else: # Kill the specific process for the segment
                for process in processes:
                    cmd = "ps -ef |grep postgres | grep %s |grep -v grep | grep \"%s\" | awk '{print $3}' | xargs kill -%s" % (segment[0],process,signal)
                    remote = Command(name='ps -ef', cmdStr=cmd,remoteHost=self.host)
                    remote.run()
                    rc = remote.get_results().rc
                    if rc != 0:
                        raise Exception("ps -ef failed with rc:  (%d)" % (rc))
                    out = remote.get_results().stdout
        return rc

    def killFirstMirror(self):
        """Kill the first mirror process
        """
        segment = self.getFirstSegmentInfo("m")
        # if no segment found, then nothing need to be killed.
        if not segment:
            return 0
        return self.killProcessUnix(segment,["all"])

    def killFirstPrimary(self):
        """Kill the first primary process
        """
        segment = self.getFirstSegmentInfo("p")
        # if no segment found, then nothing need to be killed.
        if not segment:
            return 0
        return self.killProcessUnix(segment,["all"])

    def killMirror(self, contentId=0):
        """Kill a mirror process by contentId
        @param contentId: content ID of segment to kill, default=1
        """
        segment = self.getSegmentInfo("m", contentId)
        # if no segment found, then nothing need to be killed.
        if not segment:
            return 0
        return self.killProcessUnix(segment, ["all"])

    def killPrimary(self, contentId=0):
        """Kill a primary process by contentId
        @param contentId: content ID of segment to kill, default=1
        """
        segment = self.getSegmentInfo("p", contentId)
        # if no segment found, then nothing need to be killed.
        if not segment:
            return 0
        return self.killProcessUnix(segment, ["all"])
