#!/usr/bin/env python3
#
# Recovers Greenplum segment instances that are marked as invalid, if
# mirroring is configured and operational
#


#
# THIS IMPORT MUST COME FIRST
#
# import mainUtils FIRST to get python version check
from gppylib.mainUtils import *
from gppylib.programs.clsSystemState import *

#-------------------------------------------------------------------------
if __name__ == '__main__':
    options = {}
    options["programNameOverride"] = "gpstate" # for now since we are invoked from the real gpstate
    options["suppressStartupLogMessage"] = False # since we are called from another program it's funny to print what we were called with
    simple_main( GpSystemStateProgram.createParser, GpSystemStateProgram.createProgram, options)

