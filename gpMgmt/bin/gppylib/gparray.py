#!/usr/bin/env python
#
# Copyright (c) Greenplum Inc 2008. All Rights Reserved.
#
"""
  gparray.py:

    Contains three classes representing configuration information of a
    Greenplum array:

      GpArray - The primary interface - collection of all Segment within an array
      Segment    - represents configuration information for a single dbid
      SegmentPair - a Primary/Mirror pair with the same content id
"""

# ============================================================================
from datetime import date
import copy
import traceback

from gppylib.utils import checkNotNone, checkIsInt
from gppylib    import gplog
from gppylib.db import dbconn
from gppylib.gpversion import GpVersion
from gppylib.commands.unix import *
import os

logger = gplog.get_default_logger()

ROLE_PRIMARY = 'p'
ROLE_MIRROR  = 'm'
VALID_ROLES  = [ROLE_PRIMARY, ROLE_MIRROR]

# Map gp_segment_configuration role values to values from gp_primarymirror.
ROLE_TO_MODE_MAP = {}
SEG_MODE_PRIMARY = "PrimarySegment"
SEG_MODE_MIRROR  = "MirrorSegment"
ROLE_TO_MODE_MAP[ROLE_PRIMARY] = SEG_MODE_PRIMARY
ROLE_TO_MODE_MAP[ROLE_MIRROR]  = SEG_MODE_MIRROR


STATUS_UP    = 'u'
STATUS_DOWN  = 'd'
VALID_STATUS = [STATUS_UP, STATUS_DOWN]

MODE_NOT_INITIALIZED = ''               # no mirroring
MODE_CHANGELOGGING = 'c'                # filerep logging
MODE_SYNCHRONIZED = 's'                 # filerep synchronized
MODE_NOT_SYNC = 'n'
MODE_RESYNCHRONIZATION = 'r'            #

# Map gp_segment_configuration mode values to values retured from gp_primarymirror.
MODE_TO_DATA_STATE_MAP = {}
SEG_DATA_STATE_NOT_INITIALIZED    = "NotInitialized"
SEG_DATA_STATE_IN_CHANGE_TRACKING = "InChangeTracking"
SEG_DATA_STATE_SYNCHRONIZED       = "InSync"
SEG_DATA_STATE_IN_RESYNC          = "InResync"
MODE_TO_DATA_STATE_MAP[MODE_NOT_INITIALIZED]   = SEG_DATA_STATE_NOT_INITIALIZED
MODE_TO_DATA_STATE_MAP[MODE_CHANGELOGGING]     = SEG_DATA_STATE_IN_CHANGE_TRACKING
MODE_TO_DATA_STATE_MAP[MODE_SYNCHRONIZED]      = SEG_DATA_STATE_SYNCHRONIZED
MODE_TO_DATA_STATE_MAP[MODE_RESYNCHRONIZATION] = SEG_DATA_STATE_IN_RESYNC

# SegmentState values returned from gp_primarymirror.
SEGMENT_STATE_NOT_INITIALIZED               = "NotInitialized"
SEGMENT_STATE_INITIALIZATION                = "Initialization"
SEGMENT_STATE_IN_CHANGE_TRACKING_TRANSITION = "InChangeTrackingTransition"
SEGMENT_STATE_IN_RESYNCTRANSITION           = "InResyncTransition"
SEGMENT_STATE_IN_SYNC_TRANSITION            = "InSyncTransition"
SEGMENT_STATE_READY                         = "Ready"
SEGMENT_STATE_CHANGE_TRACKING_DISABLED      = "ChangeTrackingDisabled"
SEGMENT_STATE_FAULT                         = "Fault"
SEGMENT_STATE_SHUTDOWN_BACKENDS             = "ShutdownBackends"
SEGMENT_STATE_SHUTDOWN                      = "Shutdown"
SEGMENT_STATE_IMMEDIATE_SHUTDOWN            = "ImmediateShutdown"


VALID_MODE = [
    MODE_SYNCHRONIZED,
    MODE_NOT_SYNC,
    MODE_CHANGELOGGING,
    MODE_RESYNCHRONIZATION,
]
MODE_LABELS = {
    MODE_CHANGELOGGING: "Change Tracking",
    MODE_SYNCHRONIZED: "Synchronized",
    MODE_RESYNCHRONIZATION: "Resynchronizing",
    MODE_NOT_SYNC: "Not In Sync"
}

# These are all the valid states primary/mirror pairs can
# be in.  Any configuration other than this will cause the
# FTS Prober to bring down the master postmaster until the
# configuration is corrected.  Here, primary and mirror refer
# to the segments current role, not the preferred_role.
#
# The format of the tuples are:
#    (<primary status>, <prmary mode>, <mirror status>, <mirror_mode>)
VALID_SEGMENT_STATES = [
    (STATUS_UP, MODE_CHANGELOGGING, STATUS_DOWN, MODE_SYNCHRONIZED),
    (STATUS_UP, MODE_CHANGELOGGING, STATUS_DOWN, MODE_RESYNCHRONIZATION),
    (STATUS_UP, MODE_RESYNCHRONIZATION, STATUS_UP, MODE_RESYNCHRONIZATION),
    (STATUS_UP, MODE_SYNCHRONIZED, STATUS_UP, MODE_SYNCHRONIZED),
    (STATUS_UP, MODE_NOT_SYNC, STATUS_UP, MODE_NOT_SYNC),
    (STATUS_UP, MODE_NOT_SYNC, STATUS_DOWN, MODE_NOT_SYNC)
]

def getDataModeLabel(mode):
    return MODE_LABELS[mode]

MASTER_CONTENT_ID = -1

class InvalidSegmentConfiguration(Exception):
    """Exception raised when an invalid gparray configuration is
    read from gp_segment_configuration or an attempt to save an
    invalid gparray configuration is made."""
    def __init__(self, array):
        self.array = array

    def __str__(self):
        return "Invalid GpArray: %s" % self.array

# ============================================================================
# ============================================================================
class Segment:
    """
    Segment class representing configuration information for a single dbid
    within a Greenplum Array.
    """

    # --------------------------------------------------------------------
    def __init__(self, content, preferred_role, dbid, role, mode, status,
                 hostname, address, port, datadir):

        # Todo: replace all these fields with private alternatives:
        # e.g. '_content' instead of 'content'.
        #
        # Other code should go through class interfaces for access, this
        # will allow easier modifications in the future.
        self.content=content
        self.preferred_role=preferred_role
        self.dbid=dbid
        self.role=role
        self.mode=mode
        self.status=status
        self.hostname=hostname
        self.address=address
        self.port=port
        self.datadir=datadir

        # Todo: Remove old dead code
        self.valid = (status == 'u')

    # --------------------------------------------------------------------
    def __str__(self):
        """
        Construct a printable string representation of a Segment
        """
        return "%s:%s:content=%s:dbid=%s:mode=%s:status=%s" % (
            self.hostname,
            self.datadir,
            self.content,
            self.dbid,
            self.mode,
            self.status
            )

    #
    # Note that this is not an ideal comparison -- it uses the string representation
    #   for comparison
    #
    def __cmp__(self,other):
        left = repr(self)
        right = repr(other)
        if left < right: return -1
        elif left > right: return 1
        else: return 0

    #
    # Moved here from system/configurationImplGpdb.py
    #
    def equalIgnoringModeAndStatus(self, other):
        """
        Return true if none of the "core" attributes (e.g. filespace)
          of two segments differ, false otherwise.

        This method is used by updateSystemConfig() to know when a catalog
        change will cause removing and re-adding a mirror segment.
        """
        firstMode = self.getSegmentMode()
        firstStatus = self.getSegmentStatus()
        try:

            # make the elements we don't want to compare match and see if they are then equal
            self.setSegmentMode(other.getSegmentMode())
            self.setSegmentStatus(other.getSegmentStatus())

            return self == other
        finally:
            #
            # restore mode and status after comaprison
            #
            self.setSegmentMode(firstMode)
            self.setSegmentStatus(firstStatus)


    # --------------------------------------------------------------------
    def __repr__(self):
        """
        Construct a string representation of class, must be sufficient
        information to call initFromString on the result and deterministic
        so it can be used for __cmp__ comparison
        """

        return '%d|%d|%s|%s|%s|%s|%s|%s|%d|%s' % (
            self.dbid,
            self.content,
            self.role,
            self.preferred_role,
            self.mode,
            self.status,
            self.hostname,
            self.address,
            self.port,
            self.datadir
            )

    # --------------------------------------------------------------------
    @staticmethod
    def initFromString(s):
        """
        Factory method, initializes a Segment object from string representation.
          - Used when importing from file format.
          - TODO: Should be compatable with repr() formatting.
        """
        tup = s.strip().split('|')

        if len(tup) != 10:
            raise Exception("Segment unknown input format: %s" % s)

        # This describes the gp_segment_configuration catalog
        dbid            = int(tup[0])
        content         = int(tup[1])
        role            = tup[2]
        preferred_role  = tup[3]
        mode            = tup[4]
        status          = tup[5]
        hostname        = tup[6]
        address         = tup[7]
        port            = int(tup[8])
        datadir         = tup[9]  # from the gp_segment_config table

        # Initialize segment without filespace information
        gpdb = Segment(content         = content,
                       preferred_role  = preferred_role,
                       dbid            = dbid,
                       role            = role,
                       mode            = mode,
                       status          = status,
                       hostname        = hostname,
                       address         = address,
                       port            = port,
                       datadir         = datadir)

        # Return the completed segment
        return gpdb


    # --------------------------------------------------------------------
    @staticmethod
    def getDataDirPrefix(datadir):
        retValue = ""
        retValue = datadir[:datadir.rfind('/')]
        return retValue

    # --------------------------------------------------------------------
    def copy(self):
        """
        Creates a copy of the segment, shallow for everything

        """
        res = copy.copy(self)
        return res

    # --------------------------------------------------------------------
    def createTemplate(self, dstDir):
        """
        Create a tempate given the information in this Segment.
        """

        # Make sure we have enough room in the dstDir to fit the segment and its filespaces.
        duCmd = DiskUsage( name = "srcDir"
                           , directory = dstDir
                           )
        duCmd.run(validateAfter=True)
        requiredSize = duCmd.get_bytes_used()
        logger.info("Starting copy of segment dbid %d to location %s" % (int(self.getSegmentDbId()), dstDir))

        cpCmd = LocalDirCopy("Copy system data directory", self.getSegmentDataDirectory(), dstDir)
        cpCmd.run(validateAfter = True)
        res = cpCmd.get_results()

    # --------------------------------------------------------------------
    # Six simple helper functions to identify what role a segment plays:
    #  + QD (Query Dispatcher)
    #     + master
    #     + standby master
    #  + QE (Query Executor)
    #     + primary
    #     + mirror
    # --------------------------------------------------------------------
    def isSegmentQD(self):
        return self.content < 0

    def isSegmentMaster(self, current_role=False):
        role = self.role if current_role else self.preferred_role
        return self.content < 0 and role == ROLE_PRIMARY

    def isSegmentStandby(self, current_role=False):
        role = self.role if current_role else self.preferred_role
        return self.content < 0 and role == ROLE_MIRROR

    def isSegmentQE(self):
        return self.content >= 0

    def isSegmentPrimary(self, current_role=False):
        role = self.role if current_role else self.preferred_role
        return self.content >= 0 and role == ROLE_PRIMARY

    def isSegmentMirror(self, current_role=False):
        role = self.role if current_role else self.preferred_role
        return self.content >= 0 and role == ROLE_MIRROR

    def isSegmentUp(self):
        return self.status == STATUS_UP

    def isSegmentDown(self):
        return self.status == STATUS_DOWN

    def isSegmentModeInChangeLogging(self):
        return self.mode == MODE_CHANGELOGGING

    def isSegmentModeSynchronized(self):
        return self.mode == MODE_SYNCHRONIZED

    def isSegmentModeInResynchronization(self):
        return self.mode == MODE_RESYNCHRONIZATION

    # --------------------------------------------------------------------
    # getters
    # --------------------------------------------------------------------
    def getSegmentDbId(self):
        return checkNotNone("dbId", self.dbid)

    def getSegmentContentId(self):
        return checkNotNone("contentId", self.content)

    def getSegmentRole(self):
        return checkNotNone("role", self.role)

    def getSegmentPreferredRole(self):
        return checkNotNone("preferredRole", self.preferred_role)

    def getSegmentMode(self):
        return checkNotNone("mode", self.mode)

    def getSegmentStatus(self):
        return checkNotNone("status", self.status)

    def getSegmentPort(self):
        """
        Returns the listening port for the postmaster for this segment.
        """
        return checkNotNone("port", self.port)

    def getSegmentHostName(self):
        """
        Returns the actual `hostname` for the host

        Note: use getSegmentAddress for the network address to use
        """
        return self.hostname

    def getSegmentAddress(self):
        """
        Returns the network address to use to contact the segment (i.e. the NIC address).

        """
        return self.address

    def getSegmentDataDirectory(self):
        """
        Return the primary datadirectory location for the segment.

        Note: the datadirectory is just one of the filespace locations
        associated with the segment, calling code should be carefull not
        to assume that this is the only directory location for this segment.

        Todo: evaluate callers of this function to see if they should really
        be dealing with a list of filespaces.
        """
        return checkNotNone("dataDirectory", self.datadir)


    # --------------------------------------------------------------------
    # setters
    # --------------------------------------------------------------------
    def setSegmentDbId(self, dbId):
        checkNotNone("dbId", dbId)
        self.dbid = dbId

    def setSegmentContentId(self, contentId):
        checkNotNone("contentId", contentId)
        if contentId < -1:
            raise Exception("Invalid content id %s" % contentId)
        self.content = contentId

    def setSegmentRole(self, role):
        checkNotNone("role", role)

        if role not in VALID_ROLES:
            raise Exception("Invalid role '%s'" % role)

        self.role = role

    def setSegmentPreferredRole(self, preferredRole):
        checkNotNone("preferredRole", preferredRole)

        if preferredRole not in VALID_ROLES:
            raise Exception("Invalid preferredRole '%s'" % preferredRole)

        self.preferred_role = preferredRole

    def setSegmentMode(self, mode):
        checkNotNone("mode", mode)

        if not mode in VALID_MODE:
            raise Exception("Invalid mode '%s'" % mode)

        self.mode = mode

    def setSegmentStatus(self, status):
        checkNotNone("status", status)

        if status not in VALID_STATUS:
            raise Exception("Invalid status '%s'" % status)

        self.status = status

    def setSegmentPort(self, port):
        checkNotNone("port", port)
        checkIsInt("port", port)
        self.port = port

    def setSegmentHostName(self, hostName):
        # None is allowed -- don't check
        self.hostname = hostName

    def setSegmentAddress(self, address):
        # None is allowed -- don't check
        self.address = address

    def setSegmentDataDirectory(self, dataDirectory):
        checkNotNone("dataDirectory", dataDirectory)
        self.datadir = dataDirectory



# ============================================================================
class SegmentPair:
    """
    Used to represent all of the SegmentDBs with the same contentID.  Today this
    can be at most a primary SegDB and a single mirror SegDB. Future plans to
    support multiple mirror segDBs are quite far away and we should consider
    simplifying until then.

    It is permissible to not have a mirror corresponding to a primary

    Note: This class complicates the implementation of gparray; we are looking
    to simplify and clarify its purpose.
    """
    primaryDB=None
    mirrorDB=None

    # --------------------------------------------------------------------
    def __init__(self):
        pass

    # --------------------------------------------------------------------
    def __str__(self):
        return "(Primary: %s, Mirror: %s)" % (str(self.primaryDB),
                                              str(self.mirrorDB))

    # --------------------------------------------------------------------
    def addPrimary(self,segDB):
        self.primaryDB=segDB

    def addMirror(self,segDB):
        self.mirrorDB = segDB

    # --------------------------------------------------------------------
    def get_dbs(self):
        dbs=[]
        if self.primaryDB is not None: # MPP-10886 don't add None to result list
            dbs.append(self.primaryDB)
        if self.mirrorDB:
            dbs.append(self.mirrorDB)
        return dbs

    # --------------------------------------------------------------------
    def get_hosts(self):
        hosts=[]
        hosts.append(self.primaryDB.hostname)
        if self.mirrorDB:
            hosts.append(self.mirrorDB.hostname)
        return hosts

    def is_segment_pair_valid(self):
        """Validates that the primary/mirror pair are in a valid state"""
        prim_status = self.primaryDB.getSegmentStatus()
        prim_mode = self.primaryDB.getSegmentMode()

        if not self.mirrorDB:
            # Since we don't have a mirror, we can assume that the status would
            # be down and not in sync.
            return (prim_status, prim_mode, STATUS_DOWN, MODE_NOT_SYNC) in VALID_SEGMENT_STATES

        mirror_status = self.mirrorDB.getSegmentStatus()
        mirror_role = self.mirrorDB.getSegmentMode()
        return (prim_status, prim_mode, mirror_status, mirror_role) in VALID_SEGMENT_STATES

# --------------------------------------------------------------------
# --------------------------------------------------------------------
class SegmentRow():

    def __init__(self, content, isprimary, dbid, host, address, port, fulldir):
        self.content         = content
        self.isprimary       = isprimary
        self.dbid            = dbid
        self.host            = host
        self.address         = address
        self.port            = port
        self.fulldir         = fulldir

    def __str__(self):
        retVal = "" + \
        "content = "          + str(self.content)   + "\n" + \
        "isprimary ="         + str(self.isprimary)       + "\n" + \
        "dbid = "             + str(self.dbid)            + "\n" + \
        "host = "             + str(self.host)             + "\n" + \
        "address = "          + str(self.address)         + "\n" + \
        "port = "             + str(self.port)            + "\n" + \
        "fulldir = "          + str(self.fulldir)         + "\n" +  "\n"


def createSegmentRows( hostlist
                     , interface_list
                     , primary_list
                     , primary_portbase
                     , mirror_type
                     , mirror_list
                     , mirror_portbase
                     , dir_prefix
                     ):
    """
    This method will return a list of SegmentRow objects that represent new segments on each host.
    The "hostlist" parameter contains both existing hosts as well as any new hosts that are
    a result of expansion.
    """

    rows    =[]
    dbid    = 0
    content = 0

    for host in hostlist:
        isprimary='t'
        port=primary_portbase
        index = 0
        for pdir in primary_list:
            fulldir = "%s/%s%d" % (pdir,dir_prefix,content)
            if len(interface_list) > 0:
                interfaceNumber = interface_list[index % len(interface_list)]
                address = host + '-' + str(interfaceNumber)
            else:
                address = host
            rows.append( SegmentRow( content = content
                                   , isprimary = isprimary
                                   , dbid = dbid
                                   , host = host
                                   , address = address
                                   , port = port
                                   , fulldir = fulldir
                                   ) )
            port += 1
            content += 1
            dbid += 1
            index = index + 1

    #mirrors
    if mirror_type is None or mirror_type == 'none':
        return rows
    elif mirror_type.lower().strip() == 'spread':
        #TODO: must be sure to put mirrors on a different subnet than primary.
        #      this is a general problem for GPDB these days.
        #      best to have the interface mapping stuff 1st.
        content=0
        isprimary='f'
        num_hosts = len(hostlist)
        num_dirs=len(primary_list)
        if num_hosts <= num_dirs:
            raise Exception("Not enough hosts for spread mirroring.  You must have more hosts than primary segments per host")

        mirror_port = {}

        mirror_host_offset=1
        last_mirror_offset=1
        for host in hostlist:
            mirror_host_offset = last_mirror_offset + 1
            last_mirror_offset += 1
            index = 0
            for mdir in mirror_list:
                fulldir = "%s/%s%d" % (mdir,dir_prefix,content)
                mirror_host = hostlist[mirror_host_offset % num_hosts]
                if mirror_host == host:
                    mirror_host_offset += 1
                    mirror_host = hostlist[mirror_host_offset % num_hosts]
                if len(interface_list) > 0:
                    interfaceNumber = interface_list[mirror_host_offset % len(interface_list)]
                    address = mirror_host + '-' + str(interfaceNumber)
                else:
                    address = mirror_host

                if not mirror_port.has_key(mirror_host):
                    mirror_port[mirror_host] = mirror_portbase

                rows.append( SegmentRow( content = content
                                       , isprimary = isprimary
                                       , dbid = dbid
                                       , host = mirror_host
                                       , address = address
                                       , port = mirror_port[mirror_host]
                                       , fulldir = fulldir
                                       ) )

                mirror_port[mirror_host] += 1
                content += 1
                dbid += 1
                mirror_host_offset += 1
                index = index + 1


    elif mirror_type.lower().strip() == 'grouped':
        content = 0
        num_hosts = len(hostlist)

        if num_hosts < 2:
            raise Exception("Not enough hosts for grouped mirroring.  You must have at least 2")

        #we'll pick our mirror host to be 1 host "ahead" of the primary.
        mirror_host_offset = 1

        isprimary='f'
        for host in hostlist:
            mirror_host = hostlist[mirror_host_offset % num_hosts]
            mirror_host_offset += 1
            port = mirror_portbase
            index = 0
            for mdir in mirror_list:
                fulldir = "%s/%s%d" % (mdir,dir_prefix,content)
                if len(interface_list) > 0:
                    interfaceNumber = interface_list[(index + 1) % len(interface_list)]
                    address = mirror_host + '-' + str(interfaceNumber)
                else:
                    address = mirror_host
                rows.append( SegmentRow( content = content
                                       , isprimary = isprimary
                                       , dbid = dbid
                                       , host = mirror_host
                                       , address = address
                                       , port = port
                                       , fulldir = fulldir
                                       ) )
                port += 1
                content += 1
                dbid += 1
                index = index + 1

    else:
        raise Exception("Invalid mirror type specified: %s" % mirror_type)

    return rows

#========================================================================
def createSegmentRowsFromSegmentList( newHostlist
                                    , interface_list
                                    , primary_segment_list
                                    , primary_portbase
                                    , mirror_type
                                    , mirror_segment_list
                                    , mirror_portbase
                                    , dir_prefix
                                    ):
    """
    This method will return a list of SegmentRow objects that represent an expansion of existing
    segments on new hosts.
    """
    rows    = []
    dbid    = 0
    content = 0
    interfaceDict = {}

    for host in newHostlist:
        isprimary='t'
        port=primary_portbase
        index = 0
        for pSeg in primary_segment_list:
            if len(interface_list) > 0:
                interfaceNumber = interface_list[index % len(interface_list)]
                address = host + '-' + str(interfaceNumber)
                interfaceDict[content] = index % len(interface_list)
            else:
                address = host
            newFulldir = "%s/%s%d" % (Segment.getDataDirPrefix(pSeg.getSegmentDataDirectory()), dir_prefix, content)
            rows.append( SegmentRow( content = content
                                   , isprimary = isprimary
                                   , dbid = dbid
                                   , host = host
                                   , address = address
                                   , port = port
                                   , fulldir = newFulldir
                                   ) )
            port += 1
            content += 1
            dbid += 1
            index += 1

    #mirrors
    if mirror_type is None or mirror_type == 'none':
        return rows
    elif mirror_type.lower().strip() == 'spread':
        content=0
        isprimary='f'
        num_hosts = len(newHostlist)
        num_dirs=len(primary_segment_list)
        if num_hosts <= num_dirs:
            raise Exception("Not enough hosts for spread mirroring.  You must have more hosts than primary segments per host")

        mirror_port = {}

        mirror_host_offset=1
        last_mirror_offset=0
        for host in newHostlist:
            mirror_host_offset = last_mirror_offset + 1
            last_mirror_offset += 1
            for mSeg in mirror_segment_list:
                newFulldir = "%s/%s%d" % (Segment.getDataDirPrefix(mSeg.getSegmentDataDirectory()), dir_prefix, content)
                mirror_host = newHostlist[mirror_host_offset % num_hosts]
                if mirror_host == host:
                    mirror_host_offset += 1
                    mirror_host = newHostlist[mirror_host_offset % num_hosts]
                if len(interface_list) > 0:
                    interfaceNumber = interface_list[(interfaceDict[content] + 1) % len(interface_list)]
                    address = mirror_host + '-' + str(interfaceNumber)
                else:
                    address = mirror_host

                if not mirror_port.has_key(mirror_host):
                    mirror_port[mirror_host] = mirror_portbase

                rows.append( SegmentRow( content = content
                                       , isprimary = isprimary
                                       , dbid = dbid
                                       , host = mirror_host
                                       , address = address
                                       , port = mirror_port[mirror_host]
                                       , fulldir = newFulldir
                                       ) )

                mirror_port[mirror_host] += 1
                content += 1
                dbid += 1
                mirror_host_offset += 1


    elif mirror_type.lower().strip() == 'grouped':
        content = 0
        num_hosts = len(newHostlist)

        if num_hosts < 2:
            raise Exception("Not enough hosts for grouped mirroring.  You must have at least 2")

        #we'll pick our mirror host to be 1 host "ahead" of the primary.
        mirror_host_offset = 1

        isprimary='f'
        for host in newHostlist:
            mirror_host = newHostlist[mirror_host_offset % num_hosts]
            mirror_host_offset += 1
            port = mirror_portbase
            index = 0
            for mSeg in mirror_segment_list:
                if len(interface_list) > 0:
                    interfaceNumber = interface_list[(interfaceDict[content] + 1) % len(interface_list)]
                    address = mirror_host + '-' + str(interfaceNumber)
                else:
                    address = mirror_host
                newFulldir = "%s/%s%d" % (Segment.getDataDirPrefix(mSeg.getSegmentDataDirectory()), dir_prefix, content)
                rows.append( SegmentRow( content = content
                                       , isprimary = isprimary
                                       , dbid = dbid
                                       , host = mirror_host
                                       , address = address
                                       , port = port
                                       , fulldir = newFulldir
                                       ) )
                port += 1
                content += 1
                dbid += 1
                index = index + 1

    else:
        raise Exception("Invalid mirror type specified: %s" % mirror_type)

    return rows

#========================================================================
def parseTrueFalse(value):
    if value.lower() == 'f':
        return False
    elif value.lower() == 't':
        return True
    raise Exception('Invalid true/false value')

# TODO: Destroy this  (MPP-7686)
#   Now that "hostname" is a distinct field in gp_segment_configuration
#   attempting to derive hostnames from interaface names should be eliminated.
#
def get_host_interface(full_hostname):
    (host_part, inf_num) = ['-'.join(full_hostname.split('-')[:-1]),
                            full_hostname.split('-')[-1]]
    if host_part == '' or not inf_num.isdigit():
        return (full_hostname, None)
    else:
        return (host_part, inf_num)


# ============================================================================
class GpArray:
    """
    GpArray is a python class that describes a Greenplum array.

    A Greenplum array consists of:
      master         - The primary QD for the array
      standby master - The mirror QD for the array [optional]
      segmentPairs array  - an array of segmentPairs within the cluster

    Each segmentPair has a primary and a mirror segment; if the system has no
    mirrors, there's still a segmentPair that simply doesn't have a mirror element

    It can be initialized either from a database connection, in which case
    it discovers the configuration information by examining the catalog, or
    via a configuration file.
    """

    # --------------------------------------------------------------------
    def __init__(self, segments, segmentsAsLoadedFromDb=None):
        self.master = None
        self.standbyMaster = None
        self.segmentPairs = []
        self.expansionSegmentPairs=[]
        self.numPrimarySegments = 0

        self.recoveredSegmentDbids = []

        self.__version = None
        self.__segmentsAsLoadedFromDb = segmentsAsLoadedFromDb
        self.hasMirrors = False

        for segdb in segments:

            # Handle QD nodes
            if segdb.isSegmentMaster(True):
                if self.master != None:
                    logger.error("multiple master dbs defined")
                    raise Exception("GpArray - multiple master dbs defined")
                self.master = segdb

            elif segdb.isSegmentStandby(True):
                if self.standbyMaster != None:
                    logger.error("multiple standby master dbs defined")
                    raise Exception("GpArray - multiple standby master dbs defined")
                self.standbyMaster = segdb

            # Handle regular segments
            elif segdb.isSegmentQE():
                if segdb.isSegmentMirror():
                    self.hasMirrors = True
                self.addSegmentDb(segdb)

            else:
                # Not a master, standbymaster, primary, or mirror?
                # shouldn't even be possible.
                logger.error("FATAL - invalid dbs defined")
                raise Exception("Error: GpArray() - invalid dbs defined")

        # Make sure we have a master db
        if self.master is None:
            logger.error("FATAL - no master dbs defined!")
            raise Exception("Error: GpArray() - no master dbs defined")

    def __str__(self):
        return "Master: %s\nStandby: %s\nSegment Pairs: %s" % (str(self.master),
                                                          str(self.standbyMaster) if self.standbyMaster else 'Not Configured',
                                                          "\n".join([str(segPair) for segPair in self.segmentPairs]))

    def hasStandbyMaster(self):
        return self.standbyMaster is not None

    def addSegmentDb(self, segdb):
        """
        This method adds a segment to the gpArray's segmentPairs list

        It extends the list if needed, initializing empty segmentPair objects
        """

        content = segdb.getSegmentContentId()

        while len(self.segmentPairs) <= content:
            self.segmentPairs.insert(content, SegmentPair())

        seg = self.segmentPairs[content]
        if segdb.isSegmentPrimary(True):
            seg.addPrimary(segdb)
            self.numPrimarySegments += 1
        else:
            seg.addMirror(segdb)

    # --------------------------------------------------------------------
    def isStandardArray(self):
        """
        This method will check various aspects of the array to see if it looks like a standard
        setup. It returns two values:

           True or False depending on if the array looks like a standard array.
           If message if the array does not look like a standard array.
        """

        try:
            # Do all the segments contain the same number of primary and mirrors.
            firstNumPrimaries = 0
            firstNumMirrors   = 0
            firstHost         = ""
            first             = True
            dbList = self.getDbList(includeExpansionSegs = True)
            gpdbByHost = self.getSegmentsByHostName(dbList)
            for host in gpdbByHost:
                gpdbList = gpdbByHost[host]
                if len(gpdbList) == 1 and gpdbList[0].isSegmentQD() == True:
                    # This host has one master segment and nothing else
                    continue
                if len(gpdbList) == 2 and gpdbList[0].isSegmentQD() and gpdbList[1].isSegmentQD():
                    # This host has the master segment and its mirror and nothing else
                    continue
                numPrimaries = 0
                numMirrors   = 0
                for gpdb in gpdbList:
                    if gpdb.isSegmentQD() == True:
                        continue
                    if gpdb.isSegmentPrimary() == True:
                        numPrimaries = numPrimaries + 1
                    else:
                        numMirrors = numMirrors + 1
                if first == True:
                    firstNumPrimaries = numPrimaries
                    firstNumMirrors = numMirrors
                    firstHost = host
                    first = False
                if numPrimaries != firstNumPrimaries:
                    raise Exception("The number of primary segments is not consistent across all nodes: %s != %s." % (host, firstHost))
                elif numMirrors != firstNumMirrors:
                    raise Exception("The number of mirror segments is not consistent across all nodes. %s != %s." % (host, firstHost))


            # Make sure the address all have the same suffix "-<n>" (like -1, -2, -3...)
            firstSuffixList = []
            first           = True
            suffixList      = []
            for host in gpdbByHost:
                gpdbList = gpdbByHost[host]
                for gpdb in gpdbList:
                    if gpdb.isSegmentMaster() == True:
                        continue
                    address = gpdb.getSegmentAddress()
                    if address == host:
                        if len(suffixList) == 0:
                            continue
                        else:
                            raise Exception("The address value for %s is the same as the host name, but other addresses on the host are not." % address)
                    suffix  = address.split('-')[-1]
                    if suffix.isdigit() == False:
                        raise Exception("The address value for %s does not correspond to a standard address." % address)
                    suffixList.append(suffix)
                suffixList.sort()
                if first == True:
                    firstSuffixList = suffixList
                first = False
                if suffixList != firstSuffixList:
                    raise Exception("The address list for %s doesn't not have the same pattern as %s." % (str(suffixList), str(firstSuffixList)))
        except Exception, e:
            # Assume any exception implies a non-standard array
            return False, str(e)

        return True, ""


    # --------------------------------------------------------------------
    @staticmethod
    def initFromCatalog(dbURL, utility=False):
        """
        Factory method, initializes a GpArray from provided database URL
        """

        hasMirrors = False
        conn = dbconn.connect(dbURL, utility)

        # Get the version from the database:
        version_str = None
        for row in dbconn.execSQL(conn, "SELECT version()"):
            version_str = row[0]
        version = GpVersion(version_str)

        config_rows = dbconn.execSQL(conn, '''
        SELECT dbid, content, role, preferred_role, mode, status,
        hostname, address, port, datadir
        FROM pg_catalog.gp_segment_configuration
        ORDER BY content, preferred_role DESC
        ''')

        # Todo: add checks that all segments should have the same filespaces?
        recoveredSegmentDbids = []
        segments = []
        seg = None
        for row in config_rows:

            # Extract fields from the row
            (dbid, content, role, preferred_role, mode, status, hostname,
             address, port, datadir) = row

            # Check if mirrors exist
            if preferred_role == ROLE_MIRROR:
                hasMirrors = True

            # If we have segments which have recovered, record them.
            if preferred_role != role and content >= 0:
                if mode == MODE_SYNCHRONIZED and status == STATUS_UP:
                    recoveredSegmentDbids.append(dbid)

            seg = Segment(content, preferred_role, dbid, role, mode, status,
                          hostname, address, port, datadir)
            segments.append(seg)

        origSegments = [seg.copy() for seg in segments]

        array = GpArray(segments, origSegments)
        array.__version = version
        array.recoveredSegmentDbids = recoveredSegmentDbids
        array.hasMirrors = hasMirrors

        return array

    # --------------------------------------------------------------------
    @staticmethod
    def initFromFile(filename):
        """
        Factory method: creates a GpArray from an input file
        (called by gpexpand.)

        Note: Currently this is only used by the gpexpand rollback facility,
        there is currently NO expectation that this file format is saved
        on disk in any long term fashion.

        Format changes of the file are acceptable until this assumption is
        changed, but initFromFile and dumpToFile must be kept in parity.
        """
        segdbs=[]
        fp = open(filename, 'r')
        for line in fp:
            segdbs.append(Segment.initFromString(line))
        fp.close()

        return GpArray(segdbs)

    # --------------------------------------------------------------------
    def is_array_valid(self):
        """Checks that each primary/mirror pair is in a valid state"""
        for segPair in self.segmentPairs:
            if not segPair.is_segment_pair_valid():
                return False
        return True

    # --------------------------------------------------------------------
    def dumpToFile(self, filename):
        """
        Dumps a GpArray to a file (called by gpexpand)

        Note: See notes above for initFromFile()
        """
        fp = open(filename, 'w')
        for gpdb in self.getDbList():
            fp.write(repr(gpdb) + '\n')
        fp.close()

    # --------------------------------------------------------------------
    def getDbList(self, includeExpansionSegs=False):
        """
        Return a list of all Segment objects that make up the array
        """

        dbs=[]
        dbs.append(self.master)
        if self.standbyMaster:
            dbs.append(self.standbyMaster)
        if includeExpansionSegs:
            dbs.extend(self.getSegDbList(True))
        else:
            dbs.extend(self.getSegDbList())
        return dbs

    # --------------------------------------------------------------------
    def getHostList(self, includeExpansionSegs = False):
        """
        Return a list of all Hosts that make up the array
        """
        hostList = []
        hostList.append(self.master.getSegmentHostName())
        if (self.standbyMaster and
            self.master.getSegmentHostName() != self.standbyMaster.getSegmentHostName()):
            hostList.append(self.standbyMaster.getSegmentHostName())

        dbList = self.getDbList(includeExpansionSegs = includeExpansionSegs)
        for db in dbList:
            if db.getSegmentHostName() in hostList:
                continue
            else:
                hostList.append(db.getSegmentHostName())
        return hostList


    def getDbIdToPeerMap(self):
        """
        Returns a map that maps a dbid to the peer segment for that dbid
        """
        contentIdToSegments = {}
        for seg in self.getSegDbList():
            arr = contentIdToSegments.get(seg.getSegmentContentId())
            if arr is None:
                arr = []
                contentIdToSegments[seg.getSegmentContentId()] = arr
            arr.append(seg)

        result = {}
        for contentId, arr in contentIdToSegments.iteritems():
            if len(arr) == 1:
                pass
            elif len(arr) != 2:
                raise Exception("Content %s has more than two segments"% contentId)
            else:
                result[arr[0].getSegmentDbId()] = arr[1]
                result[arr[1].getSegmentDbId()] = arr[0]
        return result


    # --------------------------------------------------------------------
    def getSegDbList(self, includeExpansionSegs=False):
        """Return a list of all Segment objects for all segments in the array"""
        dbs=[]
        for segPair in self.segmentPairs:
            dbs.extend(segPair.get_dbs())
        if includeExpansionSegs:
            for segPair in self.expansionSegmentPairs:
                dbs.extend(segPair.get_dbs())
        return dbs

    # --------------------------------------------------------------------
    def getSegmentList(self, includeExpansionSegs=False):
        """Return a list of SegmentPair objects for all segments in the array"""
        dbs=[]
        dbs.extend(self.segmentPairs)
        if includeExpansionSegs:
            dbs.extend(self.expansionSegmentPairs)
        return dbs

    # --------------------------------------------------------------------
    def getSegDbMap(self):
        """
        Return a map of all Segment objects that make up the array.
        """
        dbsMap = {}
        for db in self.getSegDbList():
            dbsMap[db.getSegmentDbId()] = db
        return dbsMap

    # --------------------------------------------------------------------
    def getExpansionSegDbList(self):
        """Returns a list of all Segment objects that make up the new segments
        of an expansion"""
        dbs=[]
        for segPair in self.expansionSegmentPairs:
            dbs.extend(segPair.get_dbs())
        return dbs

    # --------------------------------------------------------------------
    def getSegmentContainingDb(self, db):
        for segPair in self.segmentPairs:
            for segDb in segPair.get_dbs():
                if db.getSegmentDbId() == segDb.getSegmentDbId():
                    return segPair
        return None

    # --------------------------------------------------------------------
    def getExpansionSegmentContainingDb(self, db):
        for segPair in self.expansionSegmentPairs:
            for segDb in segPair.get_dbs():
                if db.getSegmentDbId() == segDb.getSegmentDbId():
                    return segPair
        return None
    # --------------------------------------------------------------------
    def get_invalid_segdbs(self):
        dbs=[]
        for segPair in self.segmentPairs:
            if not segPair.primaryDB.valid:
                dbs.append(segPair.primaryDB)
            if segPair.mirrorDB and not segPair.mirrorDB.valid:
                dbs.append(segPair.mirrorDB)
        return dbs

    # --------------------------------------------------------------------
    def get_synchronized_segdbs(self):
        dbs=[]
        for segPair in self.segmentPairs:
            if segPair.primaryDB.mode == MODE_SYNCHRONIZED:
                dbs.append(segPair.primaryDB)
            if segPair.mirrorDB and segPair.mirrorDB.mode == MODE_SYNCHRONIZED:
                dbs.append(segPair.primaryDB)
        return dbs

    # --------------------------------------------------------------------
    def get_unbalanced_segdbs(self):
        dbs=[]
        for segPair in self.segmentPairs:
            for segdb in segPair.get_dbs():
                if segdb.preferred_role != segdb.role:
                    dbs.append(segdb)
        return dbs

    # --------------------------------------------------------------------
    def get_unbalanced_primary_segdbs(self):
        dbs = [seg for seg in self.get_unbalanced_segdbs() if seg.role == ROLE_PRIMARY]
        return dbs

    # --------------------------------------------------------------------
    def get_valid_segdbs(self):
        dbs=[]
        for segPair in self.segmentPairs:
            if segPair.primaryDB.valid:
                dbs.append(segPair.primaryDB)
            if segPair.mirrorDB and segPair.mirrorDB.valid:
                dbs.append(segPair.mirrorDB)
        return dbs

    # --------------------------------------------------------------------
    def get_hostlist(self, includeMaster=True):
        hosts=[]
        if includeMaster:
            hosts.append(self.master.hostname)
            if self.standbyMaster is not None:
                hosts.append(self.standbyMaster.hostname)
        for segPair in self.segmentPairs:
            hosts.extend(segPair.get_hosts())
        # dedupe? segPair.get_hosts() doesn't promise to dedupe itself, and there might be more deduping to do
        return hosts

    # --------------------------------------------------------------------
    def get_master_host_names(self):
        if self.hasStandbyMaster():
            return [self.master.hostname, self.standbyMaster.hostname]
        else:
            return [self.master.hostname]

    # --------------------------------------------------------------------
    def get_max_dbid(self,includeExpansionSegs=False):
        """Returns the maximum dbid in the array.  If includeExpansionSegs
        is True, this includes the expansion segment array in the search"""
        dbid = 0

        for db in self.getDbList(includeExpansionSegs):
            if db.getSegmentDbId() > dbid:
                dbid = db.getSegmentDbId()

        return dbid

    # --------------------------------------------------------------------
    def get_max_contentid(self, includeExpansionSegs=False):
        """Returns the maximum contentid in the array.  If includeExpansionSegs
        is True, this includes the expansion segment array in the search"""
        content = 0

        for db in self.getDbList(includeExpansionSegs):
            if db.content > content:
                content = db.content

        return content

    # --------------------------------------------------------------------
    def get_segment_count(self):
        return len(self.segmentPairs)

    # --------------------------------------------------------------------
    def get_min_primary_port(self):
        """Returns the minimum primary segment db port"""
        min_primary_port = self.segmentPairs[0].primaryDB.port
        for segPair in self.segmentPairs:
            if segPair.primaryDB.port < min_primary_port:
                min_primary_port = segPair.primaryDB.port

        return min_primary_port

    # --------------------------------------------------------------------
    def get_max_primary_port(self):
        """Returns the maximum primary segment db port"""
        max_primary_port = self.segmentPairs[0].primaryDB.port
        for segPair in self.segmentPairs:
            if segPair.primaryDB.port > max_primary_port:
                max_primary_port = segPair.primaryDB.port

        return max_primary_port

    # --------------------------------------------------------------------
    def get_min_mirror_port(self):
        """Returns the minimum mirror segment db port"""
        if self.get_mirroring_enabled() is False:
            raise Exception('Mirroring is not enabled')

        min_mirror_port = self.segmentPairs[0].mirrorDB.port

        for segPair in self.segmentPairs:
            mirror = segPair.mirrorDB
            if mirror and mirror.port < min_mirror_port:
                min_mirror_port = mirror.port

        return min_mirror_port

    # --------------------------------------------------------------------
    def get_max_mirror_port(self):
        """Returns the maximum mirror segment db port"""
        if self.get_mirroring_enabled() is False:
            raise Exception('Mirroring is not enabled')

        max_mirror_port = self.segmentPairs[0].mirrorDB.port

        for segPair in self.segmentPairs:
            mirror = segPair.mirrorDB
            if mirror and mirror.port > max_mirror_port:
                max_mirror_port = mirror.port

        return max_mirror_port

    # --------------------------------------------------------------------
    def get_interface_numbers(self):
        """Returns interface numbers in the array.  Assumes that addresses are named
        <hostname>-<int_num>.  If the nodes just have <hostname> then an empty
        array is returned."""

        interface_nums = []
        primary_hostname = self.segmentPairs[0].primaryDB.hostname
        primary_address_list = []
        dbList = self.getDbList()
        for db in dbList:
            if db.isSegmentQD() == True:
                continue
            if db.getSegmentHostName() == primary_hostname:
                if db.getSegmentAddress() not in primary_address_list:
                    primary_address_list.append(db.getSegmentAddress())

        for address in primary_address_list:
            if address.startswith(primary_hostname) == False or len(primary_hostname) + 2 > len(address):
                return []
            suffix = address[len(primary_hostname):]
            if len(suffix) < 2 or suffix[0] != '-' or suffix[1:].isdigit() == False:
                return []
            interface_nums.append(suffix[1:])

        return interface_nums

    # --------------------------------------------------------------------
    def get_primary_count(self):
        return self.numPrimarySegments

    # --------------------------------------------------------------------
    def get_mirroring_enabled(self):
        """
        Returns True if content ID 0 has a mirror
        """

        return self.segmentPairs[0].mirrorDB is not None

    # --------------------------------------------------------------------
    def get_list_of_primary_segments_on_host(self, hostname):
        retValue = []

        for db in self.getDbList():
            if db.isSegmentPrimary(False) == True and db.getSegmentHostName() == hostname:
                retValue.append(db)
        return retValue

    # --------------------------------------------------------------------
    def get_list_of_mirror_segments_on_host(self, hostname):
        retValue = []

        for db in self.getDbList():
            if db.isSegmentMirror(False) == True and db.getSegmentHostName() == hostname:
                retValue.append(db)
        return retValue

    # --------------------------------------------------------------------
    def get_primary_root_datadirs(self):
        """
        Returns a list of primary data directories minus the <prefix><contentid>

        NOTE 1:
           This currently assumes that all segments are configured the same
           and gets the results only from the host of segment 0

        NOTE 2:
           The determination of hostname is based on faulty logic
        """

        primary_datadirs = []

        seg0_hostname = self.segmentPairs[0].primaryDB.getSegmentAddress()
        (seg0_hostname, inf_num) = get_host_interface(seg0_hostname)

        for db in self.getDbList():
            if db.isSegmentPrimary(False) and db.getSegmentAddress().startswith(seg0_hostname):
                primary_datadirs.append(db.datadir[:db.datadir.rfind('/')])

        return primary_datadirs

    # --------------------------------------------------------------------
    def get_mirror_root_datadirs(self):
        """
        Returns a list of mirror data directories minus the <prefix><contentid>
        """

        mirror_datadirs = []

        seg0_hostname = self.segmentPairs[0].primaryDB.getSegmentAddress()
        (seg0_hostname, inf_num) = get_host_interface(seg0_hostname)

        for db in self.getDbList():
            if db.isSegmentMirror(False) and db.getSegmentAddress().startswith(seg0_hostname):
                mirror_datadirs.append(db.datadir[:db.datadir.rfind('/')])

        return mirror_datadirs

    # --------------------------------------------------------------------
    def get_datadir_prefix(self):
        """
        Returns the prefix portion of <prefix><contentid>
        """

        start_last_dir = self.master.datadir.rfind('/') + 1
        start_dir_content = self.master.datadir.rfind('-')
        prefix = self.master.datadir[start_last_dir:start_dir_content]
        return prefix

    # --------------------------------------------------------------------
    # If we've got recovered segments, and we have a matched-pair, we
    # can update the catalog to "rebalance" back to our original primary.
    def updateRoleForRecoveredSegs(self, dbURL):
        """
        Marks the segment role to match the configured preferred_role.
        """

        # walk our list of segments, checking to make sure that
        # both members of the peer-group are in our recovered-list,
        # save their content-id.
        recovered_contents = []
        for segPair in self.segmentPairs:
            if segPair.primaryDB:
                if segPair.primaryDB.dbid in self.recoveredSegmentDbids:
                    if segPair.mirrorDB and segPair.mirrorDB.dbid in self.recoveredSegmentDbids:
                        recovered_contents.append((segPair.primaryDB.content, segPair.primaryDB.dbid, segPair.mirrorDB.dbid))

        conn = dbconn.connect(dbURL, True, allowSystemTableMods = 'dml')
        for (content_id, primary_dbid, mirror_dbid) in recovered_contents:
            sql = "UPDATE gp_segment_configuration SET role=preferred_role where content = %d" % content_id
            dbconn.executeUpdateOrInsert(conn, sql, 2)

            # NOTE: primary-dbid (right now) is the mirror.
            sql = "INSERT INTO gp_configuration_history VALUES (now(), %d, 'Reassigned role for content %d to MIRROR')" % (primary_dbid, content_id)
            dbconn.executeUpdateOrInsert(conn, sql, 1)

            # NOTE: mirror-dbid (right now) is the primary.
            sql = "INSERT INTO gp_configuration_history VALUES (now(), %d, 'Reassigned role for content %d to PRIMARY')" % (mirror_dbid, content_id)
            dbconn.executeUpdateOrInsert(conn, sql, 1)

            # We could attempt to update the segments-array.
            # But the caller will re-read the configuration from the catalog.
        dbconn.execSQL(conn, "COMMIT")
        conn.close()

    # --------------------------------------------------------------------
    def addExpansionSeg(self, content, preferred_role, dbid, role,
                        hostname, address, port, datadir):
        """
        Adds a segment to the gparray as an expansion segment.

        Note: may work better to construct the new Segment in gpexpand and
        simply pass it in.
        """

        if (content <= self.segmentPairs[-1].get_dbs()[0].content):
            raise Exception('Invalid content ID for expansion segment')

        segdb = Segment(content = content,
                        preferred_role = preferred_role,
                        dbid = dbid,
                        role = role,
                        mode = MODE_SYNCHRONIZED,
                        status = STATUS_UP,
                        hostname = hostname,
                        address = address,
                        port = port,
                        datadir = datadir)

        seglen = len(self.segmentPairs)
        expseglen = len(self.expansionSegmentPairs)

        expseg_index = content - seglen
        logger.debug('New segment index is %d' % expseg_index)
        if expseglen < expseg_index + 1:
            extendByNum = expseg_index - expseglen + 1
            logger.debug('Extending expansion array by %d' % (extendByNum))
            self.expansionSegmentPairs.extend([None] * (extendByNum))
        if self.expansionSegmentPairs[expseg_index] == None:
            self.expansionSegmentPairs[expseg_index] = SegmentPair()

        seg = self.expansionSegmentPairs[expseg_index]
        if preferred_role == ROLE_PRIMARY:
            if seg.primaryDB:
                raise Exception('Duplicate content id for primary segment')
            seg.addPrimary(segdb)
        else:
            seg.addMirror(segdb)

    # --------------------------------------------------------------------
    def reOrderExpansionSegs(self):
        """
        The expansion segments content ID may have changed during the expansion.
        This method will re-order the the segments into their proper positions.
        Since there can be no gaps in the content id (see validateExpansionSegs),
        the self.expansionSegmentPairs list is the same length.
        """
        seglen = len(self.segmentPairs)
        expseglen = len(self.expansionSegmentPairs)

        newExpansionSegments = []
        newExpansionSegments.extend([None] * expseglen)
        for segPair in self.expansionSegmentPairs:
            contentId = segPair.primaryDB.getSegmentContentId()
            index = contentId - seglen
            newExpansionSegments[index] = segPair
        self.expansionSegmentPairs = newExpansionSegments


    # --------------------------------------------------------------------
    def validateExpansionSegs(self):
        """ Checks the segments added for various inconsistencies and errors.
        """
        dbids = []
        content = []
        expansion_seg_count = 0

        # make sure we have added at least one segment
        if len(self.expansionSegmentPairs) == 0:
            raise Exception('No expansion segments defined')

        expect_all_segments_to_have_mirror = self.segmentPairs[0].mirrorDB is not None

        for segPair in self.expansionSegmentPairs:
            # If a segment is 'None' that means we have a gap in the content ids
            if segPair is None:
                raise Exception('Expansion segments do not have contiguous content ids.')

            expansion_seg_count += 1

            for segdb in segPair.get_dbs():
                dbids.append(segdb.getSegmentDbId())
                if segdb.getSegmentRole() == ROLE_PRIMARY:
                    isprimary = True
                else:
                    isprimary = False
                content.append((segdb.getSegmentContentId(), isprimary))

            # mirror count correct for this content id?
            if segPair.mirrorDB is None and expect_all_segments_to_have_mirror:
                raise Exception('Expansion segment has no mirror but mirroring is enabled.')

            if segPair.mirrorDB is not None and not expect_all_segments_to_have_mirror:
                raise Exception('Expansion segment has a mirror segment defined but mirroring is not enabled.')

        # check that the dbids are what they should be
        dbids.sort()

        # KAS Is the following really true? dbids don't need to be continuous
        if dbids[0] != self.get_max_dbid() + 1:
            raise Exception('Expansion segments have incorrect dbids')
        for i in range(0, len(dbids) - 1):
            if dbids[i] != dbids[i + 1] - 1:
                raise Exception('Expansion segments have incorrect dbids')


        # check that content ids are ok
        valid_content = []
        for i in range(self.segmentPairs[-1].primaryDB.content + 1,
                       self.segmentPairs[-1].primaryDB.content + 1 + len(self.expansionSegmentPairs)):
            valid_content.append((i, True))
            if expect_all_segments_to_have_mirror:
                valid_content.append((i, False))

        valid_content.sort(lambda x,y: cmp(x[0], y[0]) or cmp(x[1], y[1]))
        content.sort(lambda x,y: cmp(x[0], y[0]) or cmp(x[1], y[1]))

        if valid_content != content:
            raise Exception('Invalid content ids')

        # Check for redefinition data dirs and ports
        datadirs = {}
        used_ports = {}
        hostname = ""
        for db in self.getDbList(True):
            datadir = db.getSegmentDataDirectory()
            hostname = db.getSegmentHostName()
            port = db.getSegmentPort()
            if datadirs.has_key(hostname):
                if datadir in datadirs[hostname]:
                    raise Exception('Data directory %s used multiple times on host %s' % (datadir, hostname))
                else:
                    datadirs[hostname].append(datadir)
            else:
                datadirs[hostname] = []
                datadirs[hostname].append(datadir)

            # Check ports
            if used_ports.has_key(hostname):
                if db.port in used_ports[hostname]:
                    raise Exception('Port %d is used multiple times on host %s' % (port, hostname))
                else:
                    used_ports[hostname].append(db.port)
            else:
                used_ports[hostname] = []
                used_ports[hostname].append(db.port)

    # --------------------------------------------------------------------
    def addExpansionHosts(self, hosts, mirror_type):
        """ Adds a list of hosts to the array, using the same data
        directories as the original hosts.  Also adds the mirrors
        based on mirror_type.
        """
        # remove interface numbers if they exist
        existing_hosts = []
        for host in self.get_hostlist(True):
            if host not in existing_hosts:
                existing_hosts.append(host)

        new_hosts = []

        for host in hosts:
            # see if we already have the host
            if host in existing_hosts or host in new_hosts:
                continue
            else:
                new_hosts.append(host)

        if len(new_hosts) == 0:
            raise Exception('No new hosts to add')

        """ Get the first segment's host name, and use this host's configuration as a prototype """
        seg0_hostname = self.segmentPairs[0].primaryDB.getSegmentHostName()

        primary_list = self.get_list_of_primary_segments_on_host(seg0_hostname)
        mirror_list = self.get_list_of_mirror_segments_on_host(seg0_hostname)
        interface_list = self.get_interface_numbers()
        base_primary_port = self.get_min_primary_port()
        base_mirror_port = 0
        if mirror_type != 'none':
            base_mirror_port = self.get_min_mirror_port()

        prefix = self.get_datadir_prefix()
        interface_list = self.get_interface_numbers()
        interface_list.sort()

        rows = createSegmentRowsFromSegmentList( newHostlist = new_hosts
                                               , interface_list = interface_list
                                               , primary_segment_list = primary_list
                                               , primary_portbase = base_primary_port
                                               , mirror_type = mirror_type
                                               , mirror_segment_list = mirror_list
                                               , mirror_portbase = base_mirror_port
                                               , dir_prefix = prefix
                                               )

        self._fixup_and_add_expansion_segments(rows, interface_list)

    # --------------------------------------------------------------------
    def addExpansionDatadirs(self, datadirs, mirrordirs, mirror_type):
        """ Adds new segments based on new data directories to both original
        hosts and hosts that were added by addExpansionHosts.
        """
        max_primary_port = self.get_max_primary_port()
        max_mirror_port = 0
        if mirror_type != 'none':
            max_mirror_port = self.get_max_mirror_port()

        interface_list = self.get_interface_numbers()
        interface_list.sort()

        prefix = self.get_datadir_prefix()

        hosts = []
        # Get all the hosts to add the data dirs to
        for seg in self.getSegDbList(includeExpansionSegs = True):
            host = seg.getSegmentHostName()
            if host not in hosts:
                hosts.append(host)

        # Create the rows
        tempPrimaryRP = None
        tempMirrorRP = None
        rows = createSegmentRows( hostlist = hosts
                                , interface_list = interface_list
                                , primary_list = datadirs
                                , primary_portbase = max_primary_port + 1
                                , mirror_type = mirror_type
                                , mirror_list = mirrordirs
                                , mirror_portbase = max_mirror_port + 1
                                , dir_prefix = prefix
                                )

        self._fixup_and_add_expansion_segments(rows, interface_list)


    # --------------------------------------------------------------------
    def _fixup_and_add_expansion_segments(self, rows, interface_list):
        """Fixes up expansion segments added to be after the original segdbs
        This includes fixing up the dbids, content ids, data directories,
        interface part of the hostnames and mirrors.  After this is done, it
        adds them to the expansion array."""
        interface_count = len(interface_list)

        mirror_dict = {}
        # must be sorted by isprimary, then hostname
        rows.sort(lambda a,b: (cmp(b.isprimary, a.isprimary) or cmp(a.host,b.host)))
        current_host = rows[0].host
        curr_dbid = self.get_max_dbid(True) + 1
        curr_content = self.get_max_contentid(True) + 1
        # Fix up the rows with correct dbids, contentids, datadirs and interfaces
        for row in rows:
            hostname = row.host
            address  = row.address

            # Add the new segment to the expansion segments array
            # Remove the content id off of the datadir
            new_datadir = row.fulldir[:row.fulldir.rfind(str(row.content))]
            if row.isprimary == 't':
                new_datadir += ('%d' % curr_content)
                self.addExpansionSeg(curr_content, ROLE_PRIMARY, curr_dbid,
                                        ROLE_PRIMARY, hostname, address, int(row.port), new_datadir)
                # The content id was adjusted, so we need to save it for the mirror
                mirror_dict[int(row.content)] = int(curr_content)
                curr_content += 1
            else:
                new_content = mirror_dict[int(row.content)]
                new_datadir += ('%d' % int(new_content))
                self.addExpansionSeg(new_content, ROLE_MIRROR, curr_dbid,
                                     ROLE_MIRROR, hostname, address, int(row.port), new_datadir)
            curr_dbid += 1


    def guessIsMultiHome(self):
        """
        Guess whether self is a multi-home (multiple interfaces per node) cluster
        """
        segments = self.getSegDbList()
        byHost = GpArray.getSegmentsByHostName(segments)
        byAddress = GpArray.getSegmentsGroupedByValue(segments, Segment.getSegmentAddress)
        return len(byHost) != len(byAddress)

    def guessIsSpreadMirror(self):
        """
        Guess whether self is a spread mirroring configuration.
        """
        if not self.hasMirrors:
            return False

        mirrors = [seg for seg in self.getSegDbList() if seg.isSegmentMirror(current_role=False)]
        primaries = [seg for seg in self.getSegDbList() if seg.isSegmentPrimary(current_role=False)]
        assert len(mirrors) == len(primaries)

        primaryHostNameToMirrorHostNameSet = {}
        mirrorsByContentId = GpArray.getSegmentsByContentId(mirrors)
        for primary in primaries:

            mir = mirrorsByContentId[primary.getSegmentContentId()][0]

            if primary.getSegmentHostName() not in primaryHostNameToMirrorHostNameSet:
                primaryHostNameToMirrorHostNameSet[primary.getSegmentHostName()] = {}

            primaryMap = primaryHostNameToMirrorHostNameSet[primary.getSegmentHostName()]
            if mir.getSegmentHostName() not in primaryMap:
                primaryMap[mir.getSegmentHostName()] = 0
            primaryMap[mir.getSegmentHostName()] += 1

            """
            This primary host has more than one segment on a single host: assume group mirroring!
            """
            if primaryMap[mir.getSegmentHostName()] > 1:
                return False

        """
        Fall-through -- note that for a 2 host system with 1 segment per host, this will cause the guess to be 'spread'
        """
        return True

    @staticmethod
    def getSegmentsGroupedByValue(segments, segmentMethodToGetValue):
        result = {}
        for segment in segments:
            value = segmentMethodToGetValue(segment)
            arr = result.get(value)
            if arr is None:
                result[value] = arr = []
            arr.append(segment)
        return result

    @staticmethod
    def getSegmentsByHostName(segments):
        """
        Returns a map from segment host name to an array of segments (Segment objects)
        """
        return GpArray.getSegmentsGroupedByValue(segments, Segment.getSegmentHostName)

    @staticmethod
    def getSegmentsByContentId(segments):
        """
        Returns a map from segment contentId to an array of segments (Segment objects)
        """
        return GpArray.getSegmentsGroupedByValue(segments, Segment.getSegmentContentId)

    def getNumSegmentContents(self):
        return len(GpArray.getSegmentsByContentId(self.getSegDbList()))

    def getSegmentsAsLoadedFromDb(self):
        """
        To be called by the configuration providers only
        """
        return self.__segmentsAsLoadedFromDb

    def setSegmentsAsLoadedFromDb(self, segments):
        """
            To be called by the configuration providers only
            """
        self.__segmentsAsLoadedFromDb = segments

def get_segment_hosts(master_port):
    """
    """
    gparray = GpArray.initFromCatalog( dbconn.DbURL(port=master_port), utility=True )
    segments = GpArray.getSegmentsByHostName( gparray.getDbList() )
    return segments.keys()


def get_session_ids(master_port):
    """
    """
    conn = dbconn.connect( dbconn.DbURL(port=master_port), utility=True )
    try:
        rows = dbconn.execSQL(conn, "SELECT sess_id from pg_stat_activity where sess_id > 0;")
        ids  = set(row[0] for row in rows)
        return ids
    finally:
        conn.close()


def get_gparray_from_config():
    # imports below, when moved to the top, seem to cause an import error in a unit test because of dependency issue
    from gppylib.system import configurationInterface
    from gppylib.system import configurationImplGpdb
    from gppylib.system.environment import GpMasterEnvironment
    master_data_dir = os.environ['MASTER_DATA_DIRECTORY']
    gpEnv = GpMasterEnvironment(master_data_dir, False)
    configurationInterface.registerConfigurationProvider(
        configurationImplGpdb.GpConfigurationProviderUsingGpdbCatalog())
    confProvider = configurationInterface.getConfigurationProvider().initializeProvider(gpEnv.getMasterPort())
    return confProvider.loadSystemConfig(useUtilityMode=True)

# === EOF ====
