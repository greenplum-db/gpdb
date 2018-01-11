#!/usr/bin/env python
#
# Copyright (c) Greenplum Inc 2008. All Rights Reserved. 
#
#
# Internal Use Function.
#
#
# THIS IMPORT MUST COME FIRST
#
# import mainUtils FIRST to get python version check
from gppylib.mainUtils import *

import os, sys
import pickle, base64

from optparse import Option, OptionGroup, OptionParser, OptionValueError, SUPPRESS_USAGE

from gppylib.gpparseopts import OptParser, OptChecker
from gppylib import gplog, gparray, pgconf
from gppylib.commands import base, gp, pg, unix
from gppylib.db import dbconn

logger = gplog.get_default_logger()

description = (""" 
This utility is NOT SUPPORTED and is for internal-use only.

Changes the primary/mirror state on a set of one or more segment databases.
""")

_help = ["""
Utility should only be used by other GP utilities.  

Return codes:
  0 - All segments modified successfully
  1 - At least one segment was not modified successfully
  2 - Fatal error to script during startup.  unknown state.

"""]

#-------------------------------------------------------------------------
class SegToPrimaryMirrorStatus():
    def __init__(self,datadir,modified,reason):
        self.datadir=datadir
        self.modified=modified
        self.reason=reason
    
    def __str__(self):
        return "STATUS--DIR:%s--MODIFIED:%s--REASON:%s" % (self.datadir,self.modified,self.reason)

#-------------------------------------------------------------------------
#
# Be careful when modifying this -- this code should NOT attempt any database connections to the segment
#    because the fault prober may be paused at the time this is called.
#
class GpSegToPrimaryMirror():
    ######
    def __init__(self,dblist,gpversion,targetMirroringMode,pickledTransitionData):
        self.dblist=dblist
        self.expected_gpversion=gpversion
        self.pool = base.WorkerPool(numWorkers=len(dblist))
        self.targetMirroringMode = targetMirroringMode
        self.pickledTransitionData = pickledTransitionData
        
        self.gphome=os.path.abspath(os.pardir)
        
        self.actual_gpversion=gp.GpVersion.local('local GP software version check',self.gphome)
        if self.actual_gpversion != self.expected_gpversion:
            raise Exception("Local Software Version does not match what is expected.\n"
                            "The local software version is: '%s'\n"
                            "But we were expecting it to be: '%s'\n"
                            "Please review and correct" % (self.actual_gpversion,self.expected_gpversion))
        
        pass

    def run(self):
        results=[]
        failures=False
        dirportmap = {}

        for itm in self.dblist:
            (dir, port) = itm.split(':')
            dirportmap[dir] = port

        transitionData = pickle.loads(base64.urlsafe_b64decode(self.pickledTransitionData))
            
        logger.info("Changing segments...")
        for dir, port in dirportmap.iteritems():
            cmd = gp.SendFilerepTransitionMessage.buildTransitionMessageCommand(transitionData, dir, port)
            self.pool.addCommand(cmd)
              
        self.pool.join()
        
        cmds=self.pool.getCompletedItems()
        for cmd in cmds:
            res=cmd.get_results()
            if res.rc != 0:
                #TODO: read in last entries in startup.log
                reason = "Conversion failed.  stdout:\"%s\"  stderr:\"%s\"" % \
                            (res.stdout.replace("\n", " "), res.stderr.replace("\n", " "))
                status=SegToPrimaryMirrorStatus(cmd.dataDir,False, reason)
                results.append(status)
                del dirportmap[cmd.dataDir]
                failures=True

        #
        # note: we do not check whether we can connect to the database here because we don't want to block the
        #       transition because the fault prober is paused at this point.  If stuff fails just after conversion
        #       then we could block on a connect call, waiting for the prober to unstick the process
        #
        for datadir,port in dirportmap.iteritems():
            results.append(SegToPrimaryMirrorStatus(datadir,True,"Conversion Succeeded"))
                
        #Log the results!
        status = '\nCOMMAND RESULTS\n'
        
        for result in results:
            status += str(result) + "\n"
        
        logger.info(status)
        
        return 1 if failures else 0
    
    def cleanup(self):
        if self.pool:
            self.pool.haltWork()

    #-------------------------------------------------------------------------
    @staticmethod
    def createParser():
        """
        Constructs and returns an option parser.

        Called by simple_main()
        """
        parser = OptParser(option_class=OptChecker,
                    description=' '.join(description.split()),
                    version='%prog version $Revision: #1 $')
        parser.setHelp([])

        addStandardLoggingAndHelpOptions(parser, includeNonInteractiveOption=False)

        parser.add_option("-D","--datadir",dest="dblist", action="append", type="string")
        parser.add_option("-p","--pickledTransitionData",dest="pickledTransitionData", type="string")
        parser.add_option("-M","--mirroringMode",dest="mirroringMode", type="string")
        parser.add_option("-V", "--gp-version", dest="gpversion",metavar="GP_VERSION",
                        help="expected software version")

        parser.set_defaults(verbose=False, filters=[], slice=(None, None))

        return parser


    #-------------------------------------------------------------------------
    @staticmethod
    def createProgram(options, args):
        """
        Construct and returns a GpSegToPrimaryMirror object.

        Called by simple_main()
        """

        return GpSegToPrimaryMirror(options.dblist,options.gpversion,options.mirroringMode,options.pickledTransitionData)

#------------------------------------------------------------------------- 
if __name__ == '__main__':
    mainOptions = { 'setNonuserOnToolLogger': True }
    simple_main(GpSegToPrimaryMirror.createParser, GpSegToPrimaryMirror.createProgram, mainOptions)
