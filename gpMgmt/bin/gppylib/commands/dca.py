#!/usr/bin/env python3
#
# Copyright (c) Greenplum Inc 2011. All Rights Reserved.
#

"""
Module for commands that are DCA specific
"""

import os 
from gppylib.gplog import get_default_logger
from .base import Command, LOCAL, REMOTE

logger = get_default_logger()

# NOTE THIS IS A CHECK FOR 1040 or later appliance
def is_dca_appliance():
    try:
        if os.path.isfile('/opt/dca/bin/dca_gpdb_initialized'):
            return True
    except:
        pass

    return False

#-----------------------------------------------
class DcaGpdbInitialized(Command):
    def __init__(self, name, ctxt=LOCAL, remoteHost=None):
        self.cmdStr="/opt/dca/bin/dca_gpdb_initialized"
        Command.__init__(self, name, self.cmdStr, ctxt, remoteHost)

    @staticmethod
    def local():
        try:
            cmd=DcaGpdbInitialized('dcainit')
            cmd.run(validateAfter=True)
        except Exception as e:
            logger.error(e.__str__())
            logger.error("Exception running dca initialization")
        except:
            logger.error("Exception running dca initialization")

#-----------------------------------------------
class DcaGpdbStopped(Command):
    def __init__(self, name, ctxt=LOCAL, remoteHost=None):
        self.cmdStr="/opt/dca/bin/dca_gpdb_stopped"
        Command.__init__(self, name, self.cmdStr, ctxt, remoteHost)

    @staticmethod
    def local():
        try:
            cmd=DcaGpdbStopped('dcastop')
            cmd.run(validateAfter=True)
        except Exception as e:
            logger.error(e.__str__())
            logger.error("Exception running dca de-initialization")
        except:
            logger.error("Exception running dca de-initialization")

