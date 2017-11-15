#!/usr/bin/env python
#
# Copyright (c) Greenplum Inc 2008. All Rights Reserved.
#
'''
Greenplum hostcache file facilities.

This Module contains some helper functions for mapping network
interface names used in gp_segment_configuration to a collapsed set
of hostnames.

example:  sdw1-1, sdw1-2, sdw1-3, and sdw1-4 are all located
          on sdw1.

The results of this collapsing will be stored in a file:
~/.gphostcache with entries of the form:

sdw1-1:sdw1
sdw1-2:sdw1
sdw1-3:sdw1

A big complication here is that we want to group all of the
segment databases for sdw1-1 thru sdw1-4 together but we can't
use the name returned by `hostname` as its not guaranteed to
have a trusted ssh environment setup for it.

'''
import os

from gppylib.commands import base
from gppylib.commands import unix
from gppylib import gplog
from gppylib.utils import readAllLinesFromFile
FILEDIR=os.path.expanduser("~")
FILENAME=".gphostcache"
CACHEFILE=FILEDIR + "/" + FILENAME

logger = gplog.get_default_logger()

class GpHost:
    def __init__(self, hostname):
        self.hostname = hostname
        self.dbs=[]

    def addDB(self,db):
        self.dbs.append(db)

    def __str__(self):
        dirlist=[]
        for db in self.dbs:
            dirlist.append(db.datadir)
        return "Host %s has Datadirs: [%s]" % (self.hostname,','.join(dirlist))

class GpInterfaceToHostNameCache:

    def __init__(self, pool, interfacesToLookup, currentHostNameAnswersForInterfaces):
        self.__hostCache={}         # interface -> hostname

        # Read the .gphostcache file if it exists
        if os.path.isfile(CACHEFILE):
            try:
                for line in readAllLinesFromFile(CACHEFILE, stripLines=True, skipEmptyLines=True):
                    if line[0] == '#': # okay check because empty lines are skipped
                        continue

                    arr = line.split(':')
                    if len(arr) == 2 and len(arr[0]) > 0 and len(arr[1]) > 0:
                        (interface, hostname) = arr
                        self.__hostCache[interface.strip()]=hostname.strip()
            except Exception, e:
                logger.warn("Error reading file '%s': %s" % (CACHEFILE, str(e)))

        #
        # check to see which values are inconsistent with the cache and need lookup again
        #
        inconsistent = []
        for i in range(len(interfacesToLookup)):
            interface = interfacesToLookup[i]
            hostname = currentHostNameAnswersForInterfaces[i]

            # If we don't have this mapping yet set it, otherwise we simply
            # validate consistency.
            if interface not in self.__hostCache:
                self.__hostCache[interface] = hostname
            elif hostname is None:
                # external source did not have a tentative answer, the first
                # case above should have fired for the first entry on this
                # interface and will force us to lookup the hostname.
                # Additional hits on the interface can be ignored.
                pass
            elif self.__hostCache[interface] is None:
                self.__hostCache[interface] = hostname
            elif self.__hostCache[interface] != hostname:
                logger.warn("inconsistent hostname '%s' for interface '%s' and expected hostname '%s'" % \
                            (self.__hostCache[interface], interface, hostname))
                inconsistent.append(interface)

        # Clear out any inconsistent hostnames to force a recheck.
        for i in inconsistent:
            self.__hostCache[i] = None

        # Lookup any hostnames that we don't have answers for:
        pending_cmds={}
        for interface in self.__hostCache:
            if self.__hostCache[interface] is None:
                logger.debug("hostname lookup for %s" % interface)
                cmd=unix.Hostname('host lookup', ctxt=base.REMOTE, remoteHost=interface)
                pool.addCommand(cmd)
                pending_cmds[interface] = cmd

        # Fetch the results out of the WorkerPool
        if len(pending_cmds) > 0:
            pool.join()

            for interface in pending_cmds:
                cmd = pending_cmds[interface]

                # Make sure the command completed successfully
                if cmd.get_results().rc != 0:
                    logger.warn("Failed to resolve hostname for %s" % interface)
                    continue

                self.__hostCache[interface] = cmd.get_hostname()

            pool.empty_completed_items()

        # Try to update the hostcache file if we executed any hostname commands
        if len(pending_cmds) > 0:
            try:
                fp = open(CACHEFILE, 'w')

                for interface in sorted(self.__hostCache.keys()):
                    hostname = self.__hostCache[interface]

                    # skip any dangling references we still have
                    if not hostname:
                        continue
                    fp.write("%s:%s\n" % (interface, hostname))

                fp.close()
            except Exception, e:
                logger.warn(str(e))
                logger.warn("Failed to write file '%s'" % CACHEFILE)


    #
    # returns the cached host name for the interface
    #
    # should only be called for interfaces that were passed to the constructor
    #
    # Will return None if lookup of the hostname was not possible
    #
    def getHostName(self, interface):
        return self.__hostCache[interface]


class GpHostCache:

    def __init__(self, gparray, pool, skiplist=[], withMasters=False, segs=None):
        self.gparray=gparray
        self.gphost_map={}        # hostname -> GpHost
        self.segs = segs

        # these are any db's that should be skipped.
        skipmap={}
        for db in skiplist:
            skipmap[db.getSegmentDbId()]=db

        # Go through the gparray and build list of interface and hostname that
        #  will be used to build the GpInterfaceToHostNameCache
        #
        # As Greenplum 4.0 we have both interface and hostname information in
        # the catalog, so the gparray should be able to supply all of the
        # information.
        #
        # However if we have initialized from an old catalog, or from a flatfile
        # then the interface->hostname mapping may not be available.  In this
        # case we still want to do the full pass first  so that we don't lookup
        # a given interface more than once.
        interfaces = []
        hostnames = []

        # Get list of segment dbs, optionally including masters
        if withMasters:
            dblist = self.gparray.getDbList()
        else:
            dblist = self.gparray.getSegDbList()

        if segs:
            dblist = segs

        # build the interface->host mapping
        for db in dblist:
            if db.getSegmentDbId() not in skipmap:
                interfaces.append(db.getSegmentAddress())
                hostnames.append(db.getSegmentHostName())
        interfaceToHostMap = \
            GpInterfaceToHostNameCache(pool, interfaces, hostnames)

        # Build up the GpHosts using our interface->hostname lookup
        for db in dblist:

            # skip this dbid ?
            if db.getSegmentDbId() in skipmap:
                continue
            interface = db.getSegmentAddress()
            hostname  = interfaceToHostMap.getHostName(interface)

            # If the db didn't have hostname already set, (it was loaded from
            # an old catalog?) set it based on the hostname from the interface
            # lookup.
            if db.getSegmentHostName() == None:
                db.setSegmentHostName(hostname)

            if hostname not in self.gphost_map:
                self.gphost_map[hostname] = GpHost(hostname)

            self.gphost_map[hostname].addDB(db)


    ######
    def log_contents(self):
        logger.debug("Construct host-->datadirs mapping:")

        entries=[]
        for key in self.gphost_map.keys():
            gphost=self.gphost_map[key]
            entries.append(gphost.__str__())

        logger.debug('\n'.join(entries))

    ######
    def get_hostnames(self):
        hosts=[]
        for key in self.gphost_map.keys():
            gphost=self.gphost_map[key]
            hosts.append(gphost.hostname)
        return hosts

    ######
    def get_hosts(self):
        return self.gphost_map.values()


    ######
    def get_host(self,hostname):
        if hostname in self.gphost_map:
            return self.gphost_map[hostname]
        else:
            raise Exception("map does not contain host: %s" % hostname)

    #####
    def ping_hosts(self, pool):
        '''
        go through all of the gphosts and try and ping all of the hosts.
        If any fail then go back to using the interface names for those
        segments.

        throws an Exception if still can't ping on the interface names.
        '''
        failed_segs=[]
        for key in self.gphost_map.keys():
            p = unix.Ping('ping', key)
            pool.addCommand(p)

        pool.join()
        cmds=pool.getCompletedItems()

        for cmd in cmds:
            # Look for commands that failed to ping
            if not cmd.was_successful() != 0:
                hostname=cmd.hostToPing
                logger.warning("Ping to host: '%s' FAILED" % hostname)
                logger.debug("  ping details: %s" % cmd)

                gphost=self.get_host(hostname)
                dblist=gphost.dbs

                alternateHost=None

                for db in dblist:
                    dbid = db.getSegmentDbId()
                    address = db.getSegmentAddress()

                    # It would be nice to handle these through a pool,
                    # but it is both a little difficult and not the
                    # expected case.
                    pingCmd = unix.Ping("dbid: %d" % dbid, address)
                    pingCmd.run()
                    if pingCmd.get_results().rc == 0:
                        alternateHost=address
                        logger.warning("alternate host: '%s' => '%s'" %
                                       (hostname, address))
                        break
                    else:
                        logger.warning("Ping to host: '%s' FAILED" % hostname)
                        logger.debug("  ping details: %s" % pingCmd)

                if alternateHost:
                    gphost.hostname=alternateHost
                else:
                    # no interface to reach any of the segments, append all
                    # segments to the list of failed segments
                    failed_segs.extend(dblist)

                    # Removing the failed host from the cache.
                    #
                    # This seems a bit draconian, but that is what all callers
                    # of this function seem to want.
                    del self.gphost_map[hostname]

        pool.empty_completed_items()

        return failed_segs
