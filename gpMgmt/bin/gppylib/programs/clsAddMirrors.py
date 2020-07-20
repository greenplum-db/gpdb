#!/usr/bin/env python
# Line too long            - pylint: disable=C0301
# Invalid name             - pylint: disable=C0103
#
# Copyright (c) Greenplum Inc 2010. All Rights Reserved.
#
# import mainUtils FIRST to get python version check
# THIS IMPORT SHOULD COME FIRST
from gppylib.mainUtils import *

from optparse import Option, OptionGroup, OptionParser, OptionValueError, SUPPRESS_USAGE
import os, sys, getopt, socket, StringIO, signal, copy

from gppylib import gparray, gplog, pgconf, userinput, utils, heapchecksum
from gppylib.commands.base import Command
from gppylib.util import gp_utils
from gppylib.commands import base, gp, pg, unix
from gppylib.db import catalog, dbconn
from gppylib.gpparseopts import OptParser, OptChecker
from gppylib.operations.startSegments import *
from gppylib.operations.buildMirrorSegments import *
from gppylib.programs import programIoUtils
from gppylib.system import configurationInterface as configInterface
from gppylib.system.environment import GpMasterEnvironment
from gppylib.parseutils import line_reader, check_values, canonicalize_address
from gppylib.utils import writeLinesToFile, readAllLinesFromFile, TableLogger, \
    PathNormalizationException, normalizeAndValidateInputPath
from gppylib.userinput import *
from gppylib.mainUtils import ExceptionNoStackTraceNeeded

logger = gplog.get_default_logger()

MAX_PARALLEL_ADD_MIRRORS = 96

class GpMirrorBuildCalculator:
    """
    Create mirror segment (Segment) objects for an existing array, using different strategies.

    This class should be used by constructing and then calling either getSpreadMirrors, getGroupMirrors, or call
       addMirror multiple times

    The class uses internal state for tracking so cannot be reused after calling getSpreadMirrors or getGroupMirrors
    """

    def __init__(self, gpArray, mirrorDataDirs, options):
        self.__options = options
        self.__gpArray = gpArray
        self.__primaries = [seg for seg in gpArray.getDbList() if seg.isSegmentPrimary(False)]
        self.__primariesByHost = GpArray.getSegmentsByHostName(self.__primaries)
        self.__nextDbId = max([seg.getSegmentDbId() for seg in gpArray.getDbList()]) + 1
        self.__minPrimaryPortOverall = min([seg.getSegmentPort() for seg in self.__primaries])

        def comparePorts(left, right):
            return cmp(left.getSegmentPort(), right.getSegmentPort())

        self.__mirrorsAddedByHost = {}  # map hostname to the # of mirrors that have been added to that host
        self.__primariesUpdatedToHaveMirrorsByHost = {}  # map hostname to the # of primaries that have been attached to mirrors for that host
        self.__primaryPortBaseByHost = {}  # map hostname to the lowest port number in-use by a primary on that host
        for hostName, segments in self.__primariesByHost.iteritems():
            self.__primaryPortBaseByHost[hostName] = min([seg.getSegmentPort() for seg in segments])
            self.__mirrorsAddedByHost[hostName] = 0
            self.__primariesUpdatedToHaveMirrorsByHost[hostName] = 0
            segments.sort(comparePorts)

        self.__mirrorPortOffset = options.mirrorOffset
        self.__mirrorDataDirs = mirrorDataDirs

        standard, message = self.__gpArray.isStandardArray()
        if standard == False:
            logger.warn('The current system appears to be non-standard.')
            logger.warn(message)
            logger.warn('gpaddmirrors will not be able to symmetrically distribute the new mirrors.')
            logger.warn('It is recommended that you specify your own input file with appropriate values.')
            if self.__options.interactive and not ask_yesno('', "Are you sure you want to continue with this gpaddmirrors session?", 'N'):
                logger.info("User Aborted. Exiting...")
                sys.exit(0)
            self.__isStandard = False
        else:
            self.__isStandard = True

    def addMirror(self, resultOut, primary, targetHost, address, port, mirrorDataDir):
        """
        Add a mirror to the gpArray backing this calculator, also update resultOut and do some other checking

        Unlike __addMirrorForTargetHost, this does not require that the segments be added to
               a host that already has a primary

        """
        if targetHost not in self.__mirrorsAddedByHost:
            self.__mirrorsAddedByHost[targetHost] = 0
            self.__primaryPortBaseByHost[targetHost] = self.__minPrimaryPortOverall
            self.__primariesUpdatedToHaveMirrorsByHost[targetHost] = 0

        mirrorIndexOnTargetHost = self.__mirrorsAddedByHost[targetHost]
        assert mirrorIndexOnTargetHost is not None

        mirror = gparray.Segment(
            content=primary.getSegmentContentId(),
            preferred_role=gparray.ROLE_MIRROR,
            dbid=self.__nextDbId,
            role=gparray.ROLE_MIRROR,
            mode=gparray.MODE_NOT_SYNC,
            status=gparray.STATUS_UP,
            hostname=targetHost,
            address=address,
            port=port,
            datadir=mirrorDataDir)

        self.__gpArray.addSegmentDb(mirror)

        primary.setSegmentMode(gparray.MODE_NOT_SYNC)

        resultOut.append(GpMirrorToBuild(None, primary, mirror, True))

        self.__primariesUpdatedToHaveMirrorsByHost[primary.getSegmentHostName()] += 1
        self.__mirrorsAddedByHost[targetHost] = mirrorIndexOnTargetHost + 1
        self.__nextDbId += 1

    def __addMirrorForTargetHost(self, resultOut, primary, targetHost):
        """
        Add a new mirror for the given primary to the targetHost.

        This code assumes that the mirror is added to a host that already has primaries

        Fetches directory path info from the various member variables
        """
        primaryHost = primary.getSegmentHostName()

        assert self.__nextDbId != -1

        primariesOnTargetHost = self.__primariesByHost[targetHost]
        assert primariesOnTargetHost is not None

        primariesOnPrimaryHost = self.__primariesByHost[primaryHost]
        assert primariesOnPrimaryHost is not None

        primaryHostAddressList = []
        for thePrimary in primariesOnPrimaryHost:
            address = thePrimary.getSegmentAddress()
            if address in primaryHostAddressList:
                continue
            primaryHostAddressList.append(address)
        primaryHostAddressList.sort()

        mirrorIndexOnTargetHost = self.__mirrorsAddedByHost[targetHost]
        assert mirrorIndexOnTargetHost is not None

        usedPrimaryIndexOnPrimaryHost = self.__primariesUpdatedToHaveMirrorsByHost[primaryHost]
        assert usedPrimaryIndexOnPrimaryHost is not None

        # find basePort for target host
        basePort = self.__primaryPortBaseByHost[targetHost]
        assert basePort is not None
        basePort += mirrorIndexOnTargetHost

        # find basePort for the primary host
        primaryHostBasePort = self.__primaryPortBaseByHost[primaryHost]
        assert primaryHostBasePort is not None
        primaryHostBasePort += usedPrimaryIndexOnPrimaryHost

        # assign new ports to be used
        port = basePort + self.__mirrorPortOffset

        if mirrorIndexOnTargetHost >= len(self.__mirrorDataDirs):
            raise Exception("More mirrors targeted to host %s than there are mirror data directories" % targetHost)

        mirrorDataDir = self.__mirrorDataDirs[mirrorIndexOnTargetHost]

        #
        # We want to spread out use of addresses on a single host so go through primary addresses
        # for the target host to get the mirror address. We also want to put the primary and new
        # mirror on different subnets (i.e. addresses).
        #

        if self.__isStandard == False:
            address = primariesOnTargetHost[mirrorIndexOnTargetHost % len(primariesOnTargetHost)].getSegmentAddress()
        else:
            # This looks like a nice standard system, so we will attempt to distribute the mirrors appropriately.
            # Get a list of all the address on the primary and the mirror and sort them. Take the current primaries
            # index into the primary host list, and add one, and mod it with the number of address on the primary to
            # get the mirror address offset in a sorted list of addresses on the mirror host.

            primaryHostAddressList = []
            for thePrimary in primariesOnPrimaryHost:
                address = thePrimary.getSegmentAddress()
                if address in primaryHostAddressList:
                    continue
                primaryHostAddressList.append(address)
            primaryHostAddressList.sort()

            mirrorHostAddressList = []
            for thePrimary in primariesOnTargetHost:
                address = thePrimary.getSegmentAddress()
                if address in mirrorHostAddressList:
                    continue
                mirrorHostAddressList.append(address)
            mirrorHostAddressList.sort()

            primaryAddress = primary.getSegmentAddress()
            index = 0
            for address in primaryHostAddressList:
                if address == primaryAddress:
                    break
                index = index + 1
            index = (index + 1) % len(primaryHostAddressList)
            address = mirrorHostAddressList[index]

        self.addMirror(resultOut, primary, targetHost, address, port, mirrorDataDir)

    def getGroupMirrors(self):
        """
         Side-effect: self.__gpArray and other fields are updated to contain the returned segments
        """

        hosts = self.__primariesByHost.keys()
        hosts.sort()

        result = []
        for hostIndex, primaryHostName in enumerate(hosts):
            primariesThisHost = self.__primariesByHost[primaryHostName]
            targetHost = hosts[(hostIndex + 1) % len(hosts)]

            # for the primary host, build mirrors on the target host
            for i in range(len(primariesThisHost)):
                self.__addMirrorForTargetHost(result, primariesThisHost[i], targetHost)

        return result

    def getSpreadMirrors(self):
        """
         Side-effect: self.__gpArray is updated to contain the returned segments
        """

        hosts = self.__primariesByHost.keys()
        hosts.sort()

        result = []
        for hostIndex, primaryHostName in enumerate(hosts):
            primariesThisHost = self.__primariesByHost[primaryHostName]

            hostOffset = 1  # hostOffset is used to put mirrors on primary+1,primary+2,primary+3,...
            for i in range(len(primariesThisHost)):
                targetHostIndex = (hostIndex + hostOffset) % len(hosts)
                if targetHostIndex == hostIndex:
                    hostOffset += 1
                    targetHostIndex = (hostIndex + hostOffset) % len(hosts)
                targetHost = hosts[targetHostIndex]

                self.__addMirrorForTargetHost(result, primariesThisHost[i], targetHost)

                hostOffset += 1
        return result


class GpAddMirrorsProgram:
    """
    The implementation of gpaddmirrors

    """

    def __init__(self, options):
        """
        Constructor:

        @param options the options as returned by the options parser

        """
        self.__options = options
        self.__pool = None

    def _getParsedRow(self, filename, lineno, line):
        parts = line.split('|')
        if len(parts) != 4:
            msg = "line %d of file %s: expected 4 parts, obtained %d" % (lineno, filename, len(parts))
            raise ExceptionNoStackTraceNeeded(msg)
        content, address, port, datadir = parts
        check_values(lineno, address=address, port=port, datadir=datadir, content=content)
        return {
            'address': address,
            'port': port,
            'dataDirectory': datadir,
            'contentId': content,
            'lineno': lineno
        }

    def __getMirrorsToBuildFromConfigFile(self, gpArray):
        filename = self.__options.mirrorConfigFile
        rows = []
        with open(filename) as f:
            for lineno, line in line_reader(f):
                rows.append(self._getParsedRow(filename, lineno, line))

        allAddresses = [row["address"] for row in rows]
        #
        # build up the output now
        #
        toBuild = []
        primaries = [seg for seg in gpArray.getDbList() if seg.isSegmentPrimary(current_role=False)]
        segsByContentId = GpArray.getSegmentsByContentId(primaries)

        # note: passed port offset in this call should not matter
        calc = GpMirrorBuildCalculator(gpArray, [], self.__options)

        for row in rows:
            contentId = int(row['contentId'])
            address = row['address']
            dataDir = normalizeAndValidateInputPath(row['dataDirectory'], "in config file", row['lineno'])
            # FIXME: hostname probably should not be address, but to do so, "hostname" should be added to gpaddmirrors config file
            hostName = address

            primary = segsByContentId[contentId]
            if primary is None:
                raise Exception("Invalid content %d specified in input file" % contentId)

            calc.addMirror(toBuild, primary[0], hostName, address, int(row['port']), dataDir)

        if len(toBuild) != len(primaries):
            raise Exception("Wrong number of mirrors specified (specified %s mirror(s) for %s primarie(s))" % \
                            (len(toBuild), len(primaries)))

        return GpMirrorListToBuild(toBuild, self.__pool, self.__options.quiet, self.__options.parallelDegree)

    def __outputToFile(self, mirrorBuilder, file, gpArray):
        """
        """
        lines = []

        #
        # now a line for each mirror
        #
        for i, toBuild in enumerate(mirrorBuilder.getMirrorsToBuild()):
            mirror = toBuild.getFailoverSegment()

            line = '%d|%s|%d|%s' % \
                   (mirror.getSegmentContentId(), \
                    canonicalize_address(mirror.getSegmentAddress()), \
                    mirror.getSegmentPort(), \
                    mirror.getSegmentDataDirectory())

            lines.append(line)
        writeLinesToFile(self.__options.outputSampleConfigFile, lines)

    def __getDataDirectoriesForMirrors(self, maxPrimariesPerHost, gpArray):
        dirs = []

        configFile = self.__options.mirrorDataDirConfigFile
        if configFile is not None:

            #
            # load from config file
            #
            lines = readAllLinesFromFile(configFile, stripLines=True, skipEmptyLines=True)

            labelOfPathsBeingRead = "data"
            index = 0
            for line in lines:
                if index == maxPrimariesPerHost:
                    raise Exception('Number of %s directories must equal %d but more were read from %s' % \
                                    (labelOfPathsBeingRead, maxPrimariesPerHost, configFile))

                path = normalizeAndValidateInputPath(line, "config file")
                dirs.append(path)
                index += 1
            if index < maxPrimariesPerHost:
                raise Exception('Number of %s directories must equal %d but %d were read from %s' % \
                                (labelOfPathsBeingRead, maxPrimariesPerHost, index, configFile))
        else:

            #
            # get from stdin
            #
            while len(dirs) < maxPrimariesPerHost:
                print 'Enter mirror segment data directory location %d of %d >' % (len(dirs) + 1, maxPrimariesPerHost)
                line = raw_input().strip()
                if len(line) > 0:
                    try:
                        dirs.append(normalizeAndValidateInputPath(line))
                    except PathNormalizationException, e:
                        print "\n%s\n" % e

        return dirs

    def __generateMirrorsToBuild(self, gpEnv, gpArray):
        toBuild = []

        maxPrimariesPerHost = 0
        segments = [seg for seg in gpArray.getDbList() if seg.isSegmentPrimary(False)]
        for hostName, hostSegments in GpArray.getSegmentsByHostName(segments).iteritems():
            if len(hostSegments) > maxPrimariesPerHost:
                maxPrimariesPerHost = len(hostSegments)

        dataDirs = self.__getDataDirectoriesForMirrors(maxPrimariesPerHost, gpArray)
        calc = GpMirrorBuildCalculator(gpArray, dataDirs, self.__options)
        if self.__options.spreadMirroring:
            toBuild = calc.getSpreadMirrors()
        else:
            toBuild = calc.getGroupMirrors()

        gpPrefix = gp_utils.get_gp_prefix(gpEnv.getMasterDataDir())
        if not gpPrefix:
            gpPrefix = 'gp'

        for mirToBuild in toBuild:
            # mirToBuild is a GpMirrorToBuild object
            mir = mirToBuild.getFailoverSegment()

            dataDir = utils.createSegmentSpecificPath(mir.getSegmentDataDirectory(), gpPrefix, mir)
            mir.setSegmentDataDirectory(dataDir)

        return GpMirrorListToBuild(toBuild, self.__pool, self.__options.quiet, self.__options.parallelDegree)

    def __getMirrorsToBuildBasedOnOptions(self, gpEnv, gpArray):
        """
        returns a GpMirrorListToBuild object
        """

        if self.__options.mirrorConfigFile is not None:
            return self.__getMirrorsToBuildFromConfigFile(gpArray)
        else:
            return self.__generateMirrorsToBuild(gpEnv, gpArray)

    def __displayAddMirrors(self, gpEnv, mirrorBuilder, gpArray):
        logger.info('Greenplum Add Mirrors Parameters')
        logger.info('---------------------------------------------------------')
        logger.info('Greenplum master data directory          = %s' % gpEnv.getMasterDataDir())
        logger.info('Greenplum master port                    = %d' % gpEnv.getMasterPort())
        logger.info('Parallel batch limit                     = %d' % self.__options.parallelDegree)

        total = len(mirrorBuilder.getMirrorsToBuild())
        for i, toRecover in enumerate(mirrorBuilder.getMirrorsToBuild()):
            logger.info('---------------------------------------------------------')
            logger.info('Mirror %d of %d' % (i + 1, total))
            logger.info('---------------------------------------------------------')

            tabLog = TableLogger()
            programIoUtils.appendSegmentInfoForOutput("Primary", gpArray, toRecover.getLiveSegment(), tabLog)
            programIoUtils.appendSegmentInfoForOutput("Mirror", gpArray, toRecover.getFailoverSegment(), tabLog)
            tabLog.outputTable()

        logger.info('---------------------------------------------------------')

    def checkMirrorOffset(self, gpArray):
        """
        return an array of the ports to use to begin mirror port
        """

        maxAllowedPort = 61000
        minAllowedPort = 6432

        minPort = min([seg.getSegmentPort() for seg in gpArray.getDbList()])
        maxPort = max([seg.getSegmentPort() for seg in gpArray.getDbList()])

        if self.__options.mirrorOffset < 0:
            minPort = minPort + 3 * self.__options.mirrorOffset
            maxPort = maxPort + self.__options.mirrorOffset
        else:
            minPort = minPort + self.__options.mirrorOffset
            maxPort = maxPort + 3 * self.__options.mirrorOffset

        if maxPort > maxAllowedPort or minPort < minAllowedPort:
            raise ProgramArgumentValidationException( \
                'Value of port offset supplied via -p option produces ports outside of the valid range' \
                'Mirror port base range must be between %d and %d' % (minAllowedPort, maxAllowedPort))


    def validate_heap_checksums(self, gpArray):
        num_workers = min(len(gpArray.get_hostlist()), MAX_PARALLEL_ADD_MIRRORS)
        heap_checksum_util = heapchecksum.HeapChecksum(gparray=gpArray, num_workers=num_workers, logger=logger)
        successes, failures = heap_checksum_util.get_segments_checksum_settings()
        if len(successes) == 0:
            logger.fatal("No segments responded to ssh query for heap checksum. Not expanding the cluster.")
            return 1

        consistent, inconsistent, master_heap_checksum = heap_checksum_util.check_segment_consistency(successes)

        inconsistent_segment_msgs = []
        for segment in inconsistent:
            inconsistent_segment_msgs.append("dbid: %s "
                                             "checksum set to %s differs from master checksum set to %s" %
                                             (segment.getSegmentDbId(), segment.heap_checksum,
                                              master_heap_checksum))

        if not heap_checksum_util.are_segments_consistent(consistent, inconsistent):
            logger.fatal("Cluster heap checksum setting differences reported")
            logger.fatal("Heap checksum settings on %d of %d segment instances do not match master <<<<<<<<"
                              % (len(inconsistent_segment_msgs), len(gpArray.segmentPairs)))
            logger.fatal("Review %s for details" % get_logfile())
            log_to_file_only("Failed checksum consistency validation:", logging.WARN)
            logger.fatal("gpaddmirrors error: Cluster will not be modified as checksum settings are not consistent "
                              "across the cluster.")

            for msg in inconsistent_segment_msgs:
                log_to_file_only(msg, logging.WARN)
                raise Exception("Segments have heap_checksum set inconsistently to master")
        else:
            logger.info("Heap checksum setting consistent across cluster")

    def config_primaries_for_replication(self, gpArray):
        logger.info("Starting to modify pg_hba.conf on primary segments to allow replication connections")

        try:
            for segmentPair in gpArray.getSegmentList():
                # Start with an empty string so that the later .join prepends a newline to the first entry
                entries = ['']
                # Add the samehost replication entry to support single-host development
                entries.append('host  replication {username} samehost trust'.format(username=unix.getUserName()))
                if self.__options.hba_hostnames:
                    mirror_hostname, _, _ = socket.gethostbyaddr(segmentPair.mirrorDB.getSegmentHostName())
                    entries.append("host all {username} {hostname} trust".format(username=unix.getUserName(), hostname=mirror_hostname))
                    entries.append("host replication {username} {hostname} trust".format(username=unix.getUserName(), hostname=mirror_hostname))
                    primary_hostname, _, _ = socket.gethostbyaddr(segmentPair.primaryDB.getSegmentHostName())
                    if mirror_hostname != primary_hostname:
                        entries.append("host replication {username} {hostname} trust".format(username=unix.getUserName(), hostname=primary_hostname))
                else:
                    mirror_ips = unix.InterfaceAddrs.remote('get mirror ips', segmentPair.mirrorDB.getSegmentHostName())
                    for ip in mirror_ips:
                        cidr_suffix = '/128' if ':' in ip else '/32'
                        cidr = ip + cidr_suffix
                        hba_line_entry = "host all {username} {cidr} trust".format(username=unix.getUserName(), cidr=cidr)
                        entries.append(hba_line_entry)
                    mirror_hostname = segmentPair.mirrorDB.getSegmentHostName()
                    segment_pair_ips = gp.IfAddrs.list_addrs(mirror_hostname)
                    primary_hostname = segmentPair.primaryDB.getSegmentHostName()
                    if mirror_hostname != primary_hostname:
                        segment_pair_ips.extend(gp.IfAddrs.list_addrs(primary_hostname))
                    for ip in segment_pair_ips:
                        cidr_suffix = '/128' if ':' in ip else '/32'
                        cidr = ip + cidr_suffix
                        hba_line_entry = "host replication {username} {cidr} trust".format(username=unix.getUserName(), cidr=cidr)
                        entries.append(hba_line_entry)
                cmdStr = ". {gphome}/greenplum_path.sh; echo '{entries}' >> {datadir}/pg_hba.conf; pg_ctl -D {datadir} reload".format(
                    gphome=os.environ["GPHOME"],
                    entries="\n".join(entries),
                    datadir=segmentPair.primaryDB.datadir)
                logger.debug(cmdStr)
                cmd = Command(name="append to pg_hba.conf", cmdStr=cmdStr, ctxt=base.REMOTE, remoteHost=segmentPair.primaryDB.hostname)
                cmd.run(validateAfter=True)

        except Exception, e:
            logger.error("Failed while modifying pg_hba.conf on primary segments to allow replication connections: %s" % str(e))
            raise

        else:
            logger.info("Successfully modified pg_hba.conf on primary segments to allow replication connections")


    def run(self):
        if self.__options.parallelDegree < 1 or self.__options.parallelDegree > 64:
            raise ProgramArgumentValidationException(
                "Invalid parallelDegree provided with -B argument: %d" % self.__options.parallelDegree)

        self.__pool = base.WorkerPool(self.__options.parallelDegree)
        gpEnv = GpMasterEnvironment(self.__options.masterDataDirectory, True)

        faultProberInterface.getFaultProber().initializeProber(gpEnv.getMasterPort())
        confProvider = configInterface.getConfigurationProvider().initializeProvider(gpEnv.getMasterPort())
        gpArray = confProvider.loadSystemConfig(useUtilityMode=False)

        # check that heap_checksums is consistent across cluster, fail immediately if not
        self.validate_heap_checksums(gpArray)

        self.checkMirrorOffset(gpArray)
        
        # check that we actually have mirrors
        if gpArray.hasMirrors:
            raise ExceptionNoStackTraceNeeded( \
                "GPDB physical mirroring cannot be added.  The cluster is already configured with Mirrors.")

        # figure out what needs to be done (AND update the gpArray!)
        mirrorBuilder = self.__getMirrorsToBuildBasedOnOptions(gpEnv, gpArray)
        mirrorBuilder.checkForPortAndDirectoryConflicts(gpArray)

        if self.__options.outputSampleConfigFile is not None:
            # just output config file and done
            self.__outputToFile(mirrorBuilder, self.__options.outputSampleConfigFile, gpArray)
            logger.info('Configuration file output to %s successfully.' % self.__options.outputSampleConfigFile)
        else:
            self.__displayAddMirrors(gpEnv, mirrorBuilder, gpArray)
            if self.__options.interactive:
                if not userinput.ask_yesno(None, "\nContinue with add mirrors procedure", 'N'):
                    raise UserAbortedException()

            self.config_primaries_for_replication(gpArray)
            if not mirrorBuilder.buildMirrors("add", gpEnv, gpArray):
                return 1

            logger.info("******************************************************************")
            logger.info("Mirror segments have been added; data synchronization is in progress.")
            logger.info("Data synchronization will continue in the background.")
            logger.info("Use  gpstate -s  to check the resynchronization progress.")
            logger.info("******************************************************************")

        return 0  # success -- exit code 0!

    def cleanup(self):
        if self.__pool:
            self.__pool.haltWork()

            # -------------------------------------------------------------------------

    @staticmethod
    def createParser():

        description = ("Add mirrors to a system")
        help = [""]

        parser = OptParser(option_class=OptChecker,
                           description=' '.join(description.split()),
                           version='%prog version $Revision$')
        parser.setHelp(help)

        addStandardLoggingAndHelpOptions(parser, True)

        addTo = OptionGroup(parser, "Connection Options")
        parser.add_option_group(addTo)
        addMasterDirectoryOptionForSingleClusterProgram(addTo)

        addTo = OptionGroup(parser, "Mirroring Options")
        parser.add_option_group(addTo)
        addTo.add_option("-i", None, type="string",
                         dest="mirrorConfigFile",
                         metavar="<configFile>",
                         help="Mirroring configuration file")

        addTo.add_option("-o", None,
                         dest="outputSampleConfigFile",
                         metavar="<configFile>", type="string",
                         help="Sample configuration file name to output; "
                              "this file can be passed to a subsequent call using -i option")

        addTo.add_option("-m", None, type="string",
                         dest="mirrorDataDirConfigFile",
                         metavar="<dataDirConfigFile>",
                         help="Mirroring data directory configuration file")

        addTo.add_option('-s', default=False, action='store_true',
                         dest="spreadMirroring",
                         help="use spread mirroring for placing mirrors on hosts")

        addTo.add_option("-p", None, type="int", default=1000,
                         dest="mirrorOffset",
                         metavar="<mirrorOffset>",
                         help="Mirror port offset.  The mirror port offset will be used multiple times "
                              "to derive three sets of ports [default: %default]")

        addTo.add_option("-B", None, type="int", default=16,
                         dest="parallelDegree",
                         metavar="<parallelDegree>",
                         help="Max # of workers to use for building recovery segments.  [default: %default]")

        addTo.add_option('', '--hba-hostnames', action='store_true', dest='hba_hostnames',
                          help='use hostnames instead of CIDR in pg_hba.conf')

        parser.set_defaults()
        return parser

    @staticmethod
    def createProgram(options, args):
        if len(args) > 0:
            raise ProgramArgumentValidationException("too many arguments: only options may be specified", True)
        return GpAddMirrorsProgram(options)
