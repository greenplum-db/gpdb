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
# An implementation of GpFileSystemProvider will provide functionality
#   to do filesystem operations (like iterating directories or writing temp files)
#
class GpFileSystemProvider :
    def __init__(self):
        pass

    #
    # Shutdown the file system provider
    #
    def destroy(self):
        pass

    #
    # Create a temporary file for output.  This temporary file will be automatically deleted on cleanup
    #
    def createNamedTemporaryFile( self ) :
        raise Exception("not implemented")


#
# Management of registered configuration provider.  Right now it
#   is a singleton, so initializeDatabase calls _could_ mess up
#   the singleton for other parts of code.  Perhaps switch later
#   to a factory 
#
gProvider = None
def registerFileSystemProvider(provider):
    global gProvider
    checkNotNone("New global fileSystemProvider", provider)

    if gProvider is not None:
        gProvider.destroy()

    gProvider = provider

def getFileSystemProvider():
    global gProvider
    return checkNotNone("Global fileSystemProvider", gProvider)
