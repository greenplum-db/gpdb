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
from gppylib.system.fileSystemInterface import GpFileSystemProvider

from tempfile import NamedTemporaryFile

logger = get_default_logger()

#
# An implementation of GpFileSystemProvider that passes operations through to the underlying
#  operating system
#
class GpFileSystemProviderUsingOs(GpFileSystemProvider) :
    def __init__(self):
        pass

    #
    # Shutdown the file system provider
    #
    def destroy(self):
        pass

    #
    # Create a temporary file.  Note that this uses python's NamedTemporaryFile which
    #    may not do what we want on windows (that is, on windows, the temporary file
    #    cannot be opened for reading while it is opened for this write
    #
    # returns self
    #
    def createNamedTemporaryFile( self ) :
        return NamedTemporaryFile('w', delete=True)
