# Line too long - pylint: disable=C0301
# Copyright (c) Greenplum Inc 2011. All Rights Reserved.

import sys
try:
    from gppylib import gplog
    from gppylib.commands.base import Command, findCmdInPath, CommandNotFoundException
    from gppylib.operations import Operation
except ImportError as ex:
    sys.exit(
        'Operation: Cannot import modules.  Please check that you have sourced greenplum_path.sh.  Detail: ' + str(ex))

logger = gplog.get_default_logger()

class SyncPackagesCommand(Command):
    def __init__(self, name, gppkg_path, noask):
        cmdStr = gppkg_path + " sync"
        if noask:
            cmdStr += " -a"

        Command.__init__(self, name, cmdStr)

class SyncPackages(Operation):
    """
    Synchronizes packages at cluster level
    """
    def __init__(self):
        self.gppkg_path = None
        try:
            self.gppkg_path = findCmdInPath('gppkg')
        except CommandNotFoundException:
            pass

    def execute(self):
        if self.gppkg_path is None:
            logger.info("did not find gppkg binary. skip package sync")
            return

        SyncPackagesCommand(name="sync package", gppkg_path=self.gppkg_path, noask=True).run()
