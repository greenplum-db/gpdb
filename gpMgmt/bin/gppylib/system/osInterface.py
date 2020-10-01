#!/usr/bin/env python3
#
# Copyright (c) Greenplum Inc 2009. All Rights Reserved.
#

"""

This file defines the interface that can be used to
   fetch and update system configuration information,
   as well as the data object returned by the

"""
import os

from gppylib.gplog import *
from gppylib.utils import checkNotNone

logger = get_default_logger()

#
# An implementation of GpOsProvider will provide functionality to do native/os commands
#  (like sleeping)
#
class GpOsProvider :
    def __init__(self):
        pass

    def destroy(self):
        pass

    #
    # Sleep for the given number of seconds (as though calling python's sleep)
    #
    def sleep( self, sleepTime ) :
        raise Exception("not implemented")


#
# Management of registered configuration provider.  Right now it
#   is a singleton, so initializeDatabase calls _could_ mess up
#   the singleton for other parts of code.  Perhaps switch later
#   to a factory 
#
gProvider = None
def registerOsProvider(provider):
    global gProvider
    checkNotNone("New global osProvider", provider)

    if gProvider is not None:
        gProvider.destroy()

    gProvider = provider

def getOsProvider():
    global gProvider
    return checkNotNone("Global osProvider", gProvider)
