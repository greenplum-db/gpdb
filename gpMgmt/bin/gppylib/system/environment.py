#!/usr/bin/env python3
#
# Copyright (c) Greenplum Inc 2010. All Rights Reserved.
#
from gppylib import gplog, pgconf
from gppylib.commands import gp
from gppylib.db import catalog, dbconn
from gppylib.utils import toNonNoneString, checkNotNone

logger = gplog.get_default_logger()

class GpCoordinatorEnvironment:
    """

    Encapsulates information about the environment in which the script is running AND about the
       coordinator database.

    In the future we should make it possible to build this object on segments, or when the coordinator data directory
       has not been built.

    """

    def __init__(self, coordinatorDataDir, readFromCoordinatorCatalog, timeout=None, retries=None, verbose=True):
        """
        coordinatorDataDir: if None then we try to find it from the system environment
        readFromCoordinatorCatalog: if True then we will connect to the coordinator in utility mode and fetch some more
                               data from there (like collation settings)

        """
        if coordinatorDataDir is None:
            self.__coordinatorDataDir = gp.get_coordinatordatadir()
        else:
            self.__coordinatorDataDir = coordinatorDataDir

        logger.debug("Obtaining coordinator's port from coordinator data directory")
        pgconf_dict = pgconf.readfile(self.__coordinatorDataDir + "/postgresql.conf")
        self.__coordinatorPort = pgconf_dict.int('port')
        logger.debug("Read from postgresql.conf port=%s" % self.__coordinatorPort)
        self.__coordinatorMaxConnections = pgconf_dict.int('max_connections')
        logger.debug("Read from postgresql.conf max_connections=%s" % self.__coordinatorMaxConnections)

        self.__gpHome = gp.get_gphome()
        self.__gpVersion = gp.GpVersion.local('local GP software version check',self.__gpHome)
        
        if verbose:
            logger.info("local Greenplum Version: '%s'" % self.__gpVersion)

        # read collation settings from coordinator
        if readFromCoordinatorCatalog:
            dbUrl = dbconn.DbURL(port=self.__coordinatorPort, dbname='template1', timeout=timeout, retries=retries)
            conn = dbconn.connect(dbUrl, utility=True)

            # MPP-13807, read/show the coordinator's database version too
            self.__pgVersion = dbconn.queryRow(conn, "select version();")[0]
            logger.info("coordinator Greenplum Version: '%s'" % self.__pgVersion)
            conn.close()
        else:
            self.__pgVersion = None


    def getGpHome(self): return self.__gpHome
    def getGpVersion(self): return self.__gpVersion
    def getPgVersion(self): return self.__pgVersion

    def getCoordinatorDataDir(self): return self.__coordinatorDataDir
    def getCoordinatorMaxConnections(self) : return self.__coordinatorMaxConnections
    def getCoordinatorPort(self) : return self.__coordinatorPort
