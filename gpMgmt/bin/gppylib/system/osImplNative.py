#!/usr/bin/env python3
#
# Copyright (c) Greenplum Inc 2009. All Rights Reserved.
#

"""

This file defines the interface that can be used to
   fetch and update system configuration information,
   as well as the data object returned by the

"""
import os, time

from gppylib.gplog import *
from gppylib.utils import checkNotNone
from gppylib.system.osInterface import GpOsProvider

logger = get_default_logger()

#
# An implementation of GpOsProvider that passes operations through to the underlying
#  system
#
class GpOsProviderUsingNative(GpOsProvider) :
    def __init__(self):
        pass

    def sleep(self, sleepTime):
        time.sleep(sleepTime)
