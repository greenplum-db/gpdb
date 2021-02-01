#
# Copyright (c) Greenplum Inc 2010. All Rights Reserved.
#

"""

This file defines the interface that can be used to
   interface with the fault prober

"""
import os

from gppylib.gplog import *
from gppylib.utils import checkNotNone

logger = get_default_logger()

class GpFaultProber:
    def __init__(self):
        pass
    #
    # returns self
    #
    def initializeProber( self, coordinatorPort ) :
        return self

    def pauseFaultProber(self):
        pass

    def unpauseFaultProber(self):
        pass

    def isFaultProberPaused(self):
        pass
    
    def getFaultProberInterval(self):
        pass

#
# Management of registered fault prober.  Right now it
#   is a singleton.  Perhaps switch later to a factory.
#
gFaultProber = None
def registerFaultProber(prober):
    global gFaultProber
    gFaultProber = checkNotNone("New global fault prober interface", prober)

def getFaultProber():
    global gFaultProber
    return checkNotNone("Global fault prober interface", gFaultProber)
