import abc

from gppylib.commands import unix
from gppylib.mainUtils import ExceptionNoStackTraceNeeded
from gppylib.operations.detect_unreachable_hosts import get_unreachable_segment_hosts
from gppylib.parseutils import line_reader, check_values, canonicalize_address
from gppylib.utils import checkNotNone, normalizeAndValidateInputPath
from gppylib.gparray import GpArray, Segment


class RecoverTriplet:
    """
    Represents the segments needed to perform a recovery on a given segment.
    failed   = acting mirror that needs to be recovered
    live     = acting primary to use to recover that failed segment
    failover = failed segment will be recovered here
    """
    def __init__(self, failed, live, failover):
        self.failed = failed
        self.live = live
        self.failover = failover
        self.validate(failed, live, failover)

    def __repr__(self):
        return "Failed: {0} Live: {1} Failover: {2}".format(self.failed, self.live, self.failover)

    def __eq__(self, other):
        return self.failed == other.failed and self.live == other.live and self.failover == other.failover

    @staticmethod
    def validate(failed, live, failover):

        checkNotNone("liveSegment", live)

        if failed is None and failover is None:
            raise Exception("internal error: insufficient information to recover a mirror")

        if not live.isSegmentQE():
            raise ExceptionNoStackTraceNeeded("Segment to recover from for content %s is not a correct segment "
                                              "(it is a master or standby master)" % live.getSegmentContentId())
        if not live.isSegmentPrimary(True):
            raise ExceptionNoStackTraceNeeded(
                "Segment to recover from for content %s is not a primary" % live.getSegmentContentId())
        if not live.isSegmentUp():
            raise ExceptionNoStackTraceNeeded(
                "Primary segment is not up for content %s" % live.getSegmentContentId())
        if live.unreachable:
            raise ExceptionNoStackTraceNeeded(
                "The recovery source segment %s (content %s) is unreachable." % (live.getSegmentHostName(),
                                                                                 live.getSegmentContentId()))

        if failed is not None:
            if failed.getSegmentContentId() != live.getSegmentContentId():
                raise ExceptionNoStackTraceNeeded(
                    "The primary is not of the same content as the failed mirror.  Primary content %d, "
                    "mirror content %d" % (live.getSegmentContentId(), failed.getSegmentContentId()))
            if failed.getSegmentDbId() == live.getSegmentDbId():
                raise ExceptionNoStackTraceNeeded("For content %d, the dbid values are the same.  "
                                                  "A segment may not be recovered from itself" %
                                                  live.getSegmentDbId())

        if failover is not None:
            if failover.getSegmentContentId() != live.getSegmentContentId():
                raise ExceptionNoStackTraceNeeded(
                    "The primary is not of the same content as the mirror.  Primary content %d, "
                    "mirror content %d" % (live.getSegmentContentId(), failover.getSegmentContentId()))
            if failover.getSegmentDbId() == live.getSegmentDbId():
                raise ExceptionNoStackTraceNeeded("For content %d, the dbid values are the same.  "
                                                  "A segment may not be built from itself"
                                                  % live.getSegmentDbId())
            if failover.unreachable:
                raise ExceptionNoStackTraceNeeded(
                    "The recovery target segment %s (content %s) is unreachable." % (failover.getSegmentHostName(),
                                                                                     failover.getSegmentContentId()))

        if failed is not None and failover is not None:
            # for now, we require the code to have produced this -- even when moving the segment to another
            #  location, we preserve the directory
            assert failed.getSegmentDbId() == failover.getSegmentDbId()


class MirrorBuilderFactory:
    @staticmethod
    def instance(gpArray, config_file=None, new_hosts=[], logger=None):
        """

        :param gpArray: The variable gpArray may get mutated when the getMirrorTriples function is called on this instance.
        :param config_file:
        :param new_hosts:
        :param logger:
        :return:
        """
        if config_file:
            return ConfigFileMirrorBuilder(gpArray, config_file)

        return GpArrayMirrorBuilder(gpArray, new_hosts, logger)


class MirrorBuilder:
    __metaclass__ = abc.ABCMeta

    def __init__(self, gpArray):
        """

        :param gpArray: Needs to be a shallow copy since we may return a mutated gpArray
        """
        self.gpArray = gpArray
        self.recoveryTriples = []
        self.interfaceHostnameWarnings = []

    def getInterfaceHostnameWarnings(self):
        return self.interfaceHostnameWarnings

    @abc.abstractmethod
    def getMirrorTriples(self):
        """

        :return: Returns a list of tuples that describes the following
        Failed Segment:
        Live Segment
        Failover Segment
        This function ignores the status (i.e. u or d) of the segment because this function is used by gpaddmirrors,
        gpmovemirrors and gprecoverseg. For gpaddmirrors and gpmovemirrors, the segment to be moved should not
        be marked as down whereas for gprecoverseg the segment to be recovered needs to marked as down.
        """
        pass


class GpArrayMirrorBuilder(MirrorBuilder):
    def __init__(self, gpArray, newHosts, logger):
        super(GpArrayMirrorBuilder, self).__init__(gpArray)
        self.newHosts = []
        if newHosts:
            self.newHosts = newHosts[:]
        self.logger = logger


    def getMirrorTriples(self):
        segments = self.gpArray.getSegDbList()

        failedSegments = [seg for seg in segments if seg.isSegmentDown()]
        peersForFailedSegments = _findAndValidatePeersForFailedSegments(self.gpArray, failedSegments)

        # Dictionaries used for building mapping to new hosts
        recoverAddressMap = {}
        recoverHostMap = {}

        recoverHostIdx = 0

        if self.newHosts and len(self.newHosts) > 0:
            for seg in failedSegments:
                segAddress = seg.getSegmentAddress()
                segHostname = seg.getSegmentHostName()

                # Haven't seen this hostname before so we put it on a new host
                if segHostname not in recoverHostMap:
                    try:
                        recoverHostMap[segHostname] = self.newHosts[recoverHostIdx]
                    except:
                        # TODO only catch IndexError
                        raise Exception('Not enough new recovery hosts given for recovery.')
                    recoverHostIdx += 1

                destAddress = recoverHostMap[segHostname]
                destHostname = recoverHostMap[segHostname]

                # Save off the new host/address for this address.
                recoverAddressMap[segAddress] = (destHostname, destAddress)

            new_recovery_hosts = [destHostname for (destHostname, destAddress) in recoverAddressMap.values()]
            unreachable_hosts = get_unreachable_segment_hosts(new_recovery_hosts, len(new_recovery_hosts))
            if unreachable_hosts:
                raise ExceptionNoStackTraceNeeded("Cannot recover. The following recovery target hosts are "
                                                  "unreachable: %s" % unreachable_hosts)

            for key in list(recoverAddressMap.keys()):
                (newHostname, newAddress) = recoverAddressMap[key]
                try:
                    unix.Ping.local("ping new address", newAddress)
                except:
                    # new address created is invalid, so instead use same hostname for address
                    self.logger.info("Ping of %s failed, Using %s for both hostname and address.", newAddress,
                                     newHostname)
                    newAddress = newHostname
                recoverAddressMap[key] = (newHostname, newAddress)

            if len(self.newHosts) != recoverHostIdx:
                self.interfaceHostnameWarnings.append("The following recovery hosts were not needed:")
                for h in self.newHosts[recoverHostIdx:]:
                    self.interfaceHostnameWarnings.append("\t%s" % h)

        portAssigner = _PortAssigner(self.gpArray)

        for i in range(len(failedSegments)):

            failoverSegment = None
            failedSegment = failedSegments[i]
            liveSegment = peersForFailedSegments[i]

            if self.newHosts and len(self.newHosts) > 0:
                (newRecoverHost, newRecoverAddress) = recoverAddressMap[failedSegment.getSegmentAddress()]
                # these two lines make it so that failoverSegment points to the object that is registered in gparray
                failoverSegment = failedSegment
                failedSegment = failoverSegment.copy()
                failoverSegment.unreachable = False  # recover to a new host; it is reachable as checked above.
                failoverSegment.setSegmentHostName(newRecoverHost)
                failoverSegment.setSegmentAddress(newRecoverAddress)
                port = portAssigner.findAndReservePort(newRecoverHost, newRecoverAddress)
                failoverSegment.setSegmentPort(port)
            else:
                # we are recovering to the same host("in place") and hence
                # cannot recover if the failed segment is unreachable.
                # This is equivalent to failoverSegment.unreachable that we should be doing here but
                # due to how the code is factored failoverSegment is None here.
                if failedSegment.unreachable:
                    continue

            self.recoveryTriples.append(RecoverTriplet(failedSegment, liveSegment, failoverSegment))

        return self.recoveryTriples


class ConfigFileMirrorBuilder(MirrorBuilder):
    def __init__(self, gpArray, config_file):
        super(ConfigFileMirrorBuilder, self).__init__(gpArray)
        self.config_file = config_file
        self.rows = self._parseConfigFile(self.config_file)

    def getMirrorTriples(self):
        failedSegments = []
        failoverSegments = []
        for row in self.rows:
            # find the failed segment
            failedAddress = row['failedAddress']
            failedPort = row['failedPort']
            failedDataDirectory = normalizeAndValidateInputPath(row['failedDataDirectory'],
                                                                "config file", row['lineno'])
            failedSegment = None
            for segment in self.gpArray.getDbList():
                if (segment.getSegmentAddress() == failedAddress
                        and str(segment.getSegmentPort()) == failedPort
                        and segment.getSegmentDataDirectory() == failedDataDirectory):

                    if failedSegment is not None:
                        # this could be an assertion -- configuration should not allow multiple entries!
                        raise Exception(("A segment to recover was found twice in configuration.  "
                                         "This segment is described by address|port|directory '%s|%s|%s' "
                                         "on the input line: %s") %
                                        (failedAddress, failedPort, failedDataDirectory, row['lineno']))
                    failedSegment = segment

            if failedSegment is None:
                raise Exception("A segment to recover was not found in configuration.  " \
                                "This segment is described by address|port|directory '%s|%s|%s' on the input line: %s" %
                                (failedAddress, failedPort, failedDataDirectory, row['lineno']))

            # TODO: These 2 cases have different behavior which might be confusing to the user.
            # "<failed_address>|<port>|<data_dir> <recovery_address>|<port>|<data_dir>" does full recovery
            # "<failed_address>|<port>|<data_dir>" does incremental recovery
            failoverSegment = None
            if "newAddress" in row:
                """
                When the second set was passed, the caller is going to tell us to where we need to failover, so
                  build a failover segment
                """
                # these two lines make it so that failoverSegment points to the object that is registered in gparray
                failoverSegment = failedSegment
                failedSegment = failoverSegment.copy()

                address = row["newAddress"]
                try:
                    port = int(row["newPort"])
                except ValueError:
                    raise Exception('Config file format error, invalid number value in line: %s' % (row['lineno']))

                dataDirectory = normalizeAndValidateInputPath(row["newDataDirectory"], "config file",
                                                              row['lineno'])
                # TODO: hostname probably should not be address, but to do so, "hostname" should be added to gpaddmirrors config file
                # TODO: This appears identical to __getMirrorsToBuildFromConfigFilein clsAddMirrors
                hostName = address

                # now update values in failover segment
                failoverSegment.setSegmentAddress(address)
                failoverSegment.setSegmentHostName(hostName)
                failoverSegment.setSegmentPort(port)
                failoverSegment.setSegmentDataDirectory(dataDirectory)

            # this must come AFTER the if check above because failedSegment can be adjusted to
            #   point to a different object
            failedSegments.append(failedSegment)
            failoverSegments.append(failoverSegment)

        peersForFailedSegments = _findAndValidatePeersForFailedSegments(self.gpArray, failedSegments)

        for index, failedSegment in enumerate(failedSegments):
            peerForFailedSegment = peersForFailedSegments[index]

            if failedSegment.unreachable:
                continue

            self.recoveryTriples.append(RecoverTriplet(failedSegment, peerForFailedSegment, failoverSegments[index]))

        return self.recoveryTriples

    @staticmethod
    def _parseConfigFile(config_file):
        """
        Parse the config file
        :param config_file:
        :return: List of dictionaries with each dictionary containing the failed and failover information??
        """
        rows = []
        with open(config_file) as f:
            for lineno, line in line_reader(f):

                groups = line.split()  # NOT line.split(' ') due to MPP-15675
                if len(groups) not in [1, 2]:
                    msg = "line %d of file %s: expected 1 or 2 groups but found %d" % (lineno, config_file, len(groups))
                    raise ExceptionNoStackTraceNeeded(msg)
                parts = groups[0].split('|')
                if len(parts) != 3:
                    msg = "line %d of file %s: expected 3 parts on failed segment group, obtained %d" % (
                        lineno, config_file, len(parts))
                    raise ExceptionNoStackTraceNeeded(msg)
                address, port, datadir = parts
                check_values(lineno, address=address, port=port, datadir=datadir)
                row = {
                    'failedAddress': address,
                    'failedPort': port,
                    'failedDataDirectory': datadir,
                    'lineno': lineno
                }
                if len(groups) == 2:
                    parts2 = groups[1].split('|')
                    if len(parts2) != 3:
                        msg = "line %d of file %s: expected 3 parts on new segment group, obtained %d" % (
                            lineno, config_file, len(parts2))
                        raise ExceptionNoStackTraceNeeded(msg)
                    address2, port2, datadir2 = parts2
                    check_values(lineno, address=address2, port=port2, datadir=datadir2)
                    row.update({
                        'newAddress': address2,
                        'newPort': port2,
                        'newDataDirectory': datadir2
                    })

                rows.append(row)

        ConfigFileMirrorBuilder._validate(rows)

        return rows

    @staticmethod
    def _validate(rows):
        """
        Runs checks for making sure all the rows are consistent
        :param rows:
        :return:
        """
        failed = {}
        new = {}
        for row in rows:
            address, port, datadir, lineno = \
                row['failedAddress'], row['failedPort'], row['failedDataDirectory'], row['lineno']

            if address+datadir in failed:
                msg = 'config file lines {0} and {1} conflict: ' \
                      'Cannot recover the same failed segment {2} and data directory {3} twice.' \
                    .format(failed[address+datadir], lineno, address, datadir)
                raise ExceptionNoStackTraceNeeded(msg)

            failed[address+datadir] = lineno

            if 'newAddress' not in row:
                if address+datadir in new:
                    msg = 'config file lines {0} and {1} conflict: ' \
                          'Cannot recover segment {2} with data directory {3} in place if it is used as a recovery segment.' \
                        .format(new[address+datadir], lineno, address, datadir)
                    raise ExceptionNoStackTraceNeeded(msg)

                continue

            address2, port2, datadir2 = row['newAddress'], row['newPort'], row['newDataDirectory']

            if address2+datadir2 in new:
                msg = 'config file lines {0} and {1} conflict: ' \
                      'Cannot recover to the same segment {2} and data directory {3} twice.' \
                    .format(new[address2+datadir2], lineno, address2, datadir2)
                raise ExceptionNoStackTraceNeeded(msg)

            new[address2+datadir2] = lineno


def _findAndValidatePeersForFailedSegments(gpArray, failedSegments):
    dbIdToPeerMap = gpArray.getDbIdToPeerMap()
    peersForFailedSegments = [dbIdToPeerMap.get(seg.getSegmentDbId()) for seg in failedSegments]

    for i in range(len(failedSegments)):
        peer = peersForFailedSegments[i]
        if peer is None:
            raise Exception("No peer found for dbid %s" % failedSegments[i].getSegmentDbId())
        elif peer.isSegmentDown():
            raise Exception(
                "Both segments for content %s are down; Try restarting Greenplum DB and running the program again." %
                (peer.getSegmentContentId()))
    return peersForFailedSegments

class _PortAssigner:
    """
    Used to assign new ports to segments on a host

    Note that this could be improved so that we re-use ports for segments that are being recovered but this
      does not seem necessary.

    """

    MAX_PORT_EXCLUSIVE = 65536

    def __init__(self, gpArray):
        #
        # determine port information for recovering to a new host --
        #   we need to know the ports that are in use and the valid range of ports
        #
        segments = gpArray.getDbList()
        ports = [seg.getSegmentPort() for seg in segments if seg.isSegmentQE()]
        if len(ports) > 0:
            self.__minPort = min(ports)
        else:
            raise Exception("No segment ports found in array.")
        self.__usedPortsByHostName = {}

        byHost = GpArray.getSegmentsByHostName(segments)

        for hostName, segments in byHost.iteritems():
            usedPorts = self.__usedPortsByHostName[hostName] = {}
            for seg in segments:
                usedPorts[seg.getSegmentPort()] = True

    def findAndReservePort(self, hostName, address):
        """
        Find a port not used by any postmaster process.
        When found, add an entry:  usedPorts[port] = True   and return the port found
        Otherwise raise an exception labeled with the given address
        """
        if hostName not in self.__usedPortsByHostName:
            self.__usedPortsByHostName[hostName] = {}
        usedPorts = self.__usedPortsByHostName[hostName]

        minPort = self.__minPort
        for port in range(minPort, _PortAssigner.MAX_PORT_EXCLUSIVE):
            if port not in usedPorts:
                usedPorts[port] = True
                return port
        raise Exception("Unable to assign port on %s" % address)
