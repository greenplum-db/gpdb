#!/usr/bin/env python
#
# Copyright (c) Greenplum Inc 2010. All Rights Reserved.
#
from gppylib.gparray import *

"""

This file defines the interface that can be used to
   fetch and update system configuration information
   for testing.  It does not use the database for
   getting and updating system configuration data

"""
import os

import copy

from gppylib.gplog import *
from gppylib.utils import checkNotNone
from gppylib.system.configurationInterface import *
from gppylib.db import dbconn
from gppylib.testold.testUtils import testOutput

logger = get_default_logger()

class GpConfigurationProviderForTesting(GpConfigurationProvider) :

    def __init__(self):
        self.__testSegments=[]
        pass

    def initializeProvider( self, masterPort ) :
        """
         Initialize the provider to get information from the given master db, if it chooses to
           get its data from the database

         returns self
        """

        # verify here to match what the true one will require
        checkNotNone("masterPort", masterPort)
        return self

    def loadSystemConfig( self, useUtilityMode ) :
        """
        Load all segment information from the configuration source.

        Returns a new GpArray object
        """
        segments = []
        for segment in self.__testSegments:
            segments.append(segment.copy())
        result = GpArray(segments)
        return result

    def sendPgElogFromMaster( self, msg, sendAlerts):
        """
        Send a message from the master database using select pg_elog ...
        """
        testOutput("Elog on master: " + ("(with alerts)" if sendAlerts else "(without alerts)") + msg)


    def updateSystemConfig( self, systemConfiguration, textForConfigTable, dbIdToForceMirrorRemoveAdd, useUtilityMode) :
        """
        Update the configuration for the given segments in the underlying configuration store
            to match the current values
        """
        self.__testSegments = []
        for segment in systemConfiguration.getDbList():
            self.addTestSegment(segment)

    def addTestSegment(self, configForSegment):
        """
        Add a test segment.  The segment is COPIED before adding
        """
        self.__testSegments.append(configForSegment.copy())
