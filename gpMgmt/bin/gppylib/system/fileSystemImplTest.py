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
from tempfile import NamedTemporaryFile
from gppylib.system.fileSystemInterface import GpFileSystemProvider

logger = get_default_logger()

#
# List of future improvements:
#
#
class GpTempFileForTesting(GpFileSystemProvider):

    def __init__(self, path):
        checkNotNone("path", path)

        self.__isClosed = False
        self.name = path
        self.__underlyingFile = NamedTemporaryFile('w', delete=True)

    def flush(self):
        if self.__isClosed:
            raise Exception("File has been closed")

        self.__underlyingFile.flush()

    def write(self, data):
        if self.__isClosed:
            raise Exception("File has been closed")

        checkNotNone("file", self.__underlyingFile) # already closed?

        self.__underlyingFile.write(data)

    def close(self):
        self.__isClosed = True
        self.__underlyingFile.flush()

    #
    # permanent close: not part of the filesystem interface, but used
    # by testing code to clean up after ourselves
    #
    def permanentCloseForTesting(self):
        self.__underlyingFile.close()
        self.__underlyingFile = None
        self.__isClosed = True

    #
    # For testing,
    #
    def getDataForTesting(self):
        f = open(self.__underlyingFile.name, 'r')
        result = f.read()
        f.close()
        return result


#
# An implementation of GpFileSystemProvider that passes operations through to the underlying
#  operating system
#
class GpFileSystemProviderForTest :


    def __init__(self):
        self.__temporaryFiles = []
        pass

    def destroy(self):
        for file in self.__temporaryFiles:
            file.permanentCloseForTesting()
        self.__temporaryFiles = []
        pass

    #
    # Create a temporary file
    #
    # returns self
    #
    def createNamedTemporaryFile( self ) :
        path = "/tmp/temporaryNamedFile%s" % len(self.__temporaryFiles)
        result = GpTempFileForTesting( path)
        self.__temporaryFiles.append( result )
        return result


    #
    def getTemporaryFileDataForTesting(self, tempFileIndex ):
        return self.__temporaryFiles[tempFileIndex].getDataForTesting()

    def hasTemporaryFileDataForTesting(self, tempFileIndex):
        return tempFileIndex >= 0 and tempFileIndex < len(self.__temporaryFiles)