#!/usr/bin/env python3
#
# Copyright (c) 2017, VMware, Inc. or its affiliates.
#

from gppylib.commands import base
from gppylib.commands.unix import *
from gppylib.commands.gp import *
from gppylib.gparray import GpArray
from gppylib.gplog import get_default_logger


class GpResGroup(object):

    def __init__(self):
        self.logger = get_default_logger()

    @staticmethod
    def validate():
        pool = base.WorkerPool()
        gp_array = GpArray.initFromCatalog(dbconn.DbURL(), utility=True)
        host_list = list(set(gp_array.get_hostlist(True)))
        msg = None

        for h in host_list:
            cmd = Command(h, "gpcheckresgroupimpl", REMOTE, h)
            pool.addCommand(cmd)
        pool.join()

        items = pool.getCompletedItems()
        failed = []
        for i in items:
            if not i.was_successful():
                failed.append("[{}:{}]".format(i.remoteHost, i.get_stderr().rstrip()))
        pool.haltWork()
        pool.joinWorkers()
        if failed:
            msg = ",".join(failed)
        return msg

    @staticmethod
    def validate_v2():
        pool = base.WorkerPool()
        gp_array = GpArray.initFromCatalog(dbconn.DbURL(), utility=True)
        host_list = list(set(gp_array.get_hostlist(True)))
        msg = None

        for h in host_list:
            cmd = Command(h, "gpcheckresgroupv2impl", REMOTE, h)
            pool.addCommand(cmd)
        pool.join()

        items = pool.getCompletedItems()
        failed = []
        for i in items:
            if not i.was_successful():
                failed.append("[{}:{}]".format(i.remoteHost, i.get_stderr().rstrip()))
        pool.haltWork()
        pool.joinWorkers()
        if failed:
            msg = ",".join(failed)
        return msg
