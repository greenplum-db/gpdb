import os, pickle, base64

from gppylib.utils import checkNotNone
from gppylib.gplog import *
from gppylib.commands import unix
from gppylib.commands import gp
from gppylib.commands import base
from gppylib.gparray import GpArray
from gppylib.commands.gp import SEGMENT_TIMEOUT_DEFAULT

logger = get_default_logger()

START_AS_PRIMARY_OR_MIRROR = "primaryOrMirror"
START_AS_MIRRORLESS = "asMirrorless"

MIRROR_MODE_MIRROR = "mirror"
MIRROR_MODE_MIRRORLESS = "mirrorless"
MIRROR_MODE_PRIMARY = "primary"
MIRROR_MODE_QUIESCENT = "quiescent"

class FailedSegmentResult:

    def __init__(self, segment, reason, reasonCode):
        self.__segment = segment
        self.__reason = reason
        self.__reasonCode = reasonCode

    def getReason(self):
        return self.__reason

    def getReasonCode(self):
        return self.__reasonCode

    def getSegment(self):
        return self.__segment

#
# Note that a failed segments may still be up -- it could be that the conversion to mirror/primary mode failed
#
class StartSegmentsResult:
    def __init__(self):
        self.__successfulSegments = []
        self.__failedSegments = []

    def getSuccessfulSegments(self):
        return self.__successfulSegments[:]

    def getFailedSegmentObjs(self):
        """
        Return a list of FailedSegmentResult objects
        """
        return self.__failedSegments[:]

    def addSuccess(self, segment):
        self.__successfulSegments.append(segment)

    def addFailure(self, segment, reason, reasonCode):
        self.__failedSegments.append(FailedSegmentResult(segment, reason, reasonCode))

    def clearSuccessfulSegments(self):
        self.__successfulSegments = []

class StartSegmentsOperation:
    """
       This operation, to be run from the master, will start the segments up
            and, if necessary, convert them to the proper mode

       Note that this can be used to start only a subset of the segments

    """

    def __init__(self, workerPool, quiet, gpVersion,
                 gpHome, masterDataDirectory, master_checksum_value=None, timeout=SEGMENT_TIMEOUT_DEFAULT,
                 specialMode=None, wrapper=None, wrapper_args=None, parallel=gp.DEFAULT_GPSTART_NUM_WORKERS,
                 logfileDirectory=False):
        checkNotNone("workerPool", workerPool)
        self.__workerPool = workerPool
        self.__quiet = quiet
        self.__gpVersion = gpVersion
        self.__gpHome = gpHome
        self.__masterDataDirectory = masterDataDirectory
        self.__timeout = timeout
        assert(specialMode in [None, 'upgrade', 'maintenance'])
        self.__specialMode = specialMode
        self.__wrapper = wrapper
        self.__wrapper_args = wrapper_args
        self.__parallel = parallel
        self.master_checksum_value = master_checksum_value
        self.logfileDirectory = logfileDirectory

    def startSegments(self, gpArray, segments, startMethod, era):
        """

        gpArray: the system to which the segments to start belong
        segments: the list of segments to be started
        startMethod: should be one of the START_AS_* constant values

        """
        totalToAttempt = len(segments)
        result = StartSegmentsResult()
        numContentsInCluster = gpArray.getNumSegmentContents()
        segmentsDbids = []
        for seg in segments:
            segmentsDbids.append(seg.getSegmentDbId())

        # now do the start!
        numNodes = len(gpArray.getHostList())
        numWorkers = self.__workerPool.getNumWorkers()
        if numWorkers >= numNodes:
            # We are in a situation where we can start the entire cluster at once.
            assert startMethod == START_AS_PRIMARY_OR_MIRROR or startMethod == START_AS_MIRRORLESS
            self.__runStartCommand(segments, startMethod, numContentsInCluster, result, gpArray, era)
        else:
            # We don't have enough workers to guarantee that a given primary and mirror are started at the same time.
            # Note, we could try to be really clever here and attempt to only start a set of segments on numWorkers
            # nodes that contain both a primary and mirror for a given pair, but that could be quite complex.
            # It would also be difficult to QA. The method here is simpler but possibly slower.
            if startMethod == START_AS_PRIMARY_OR_MIRROR:

                mirrorDbs = [seg for seg in segments if seg.isSegmentMirror(True)]
                primaryDbs = [seg for seg in segments if seg.isSegmentPrimary(True)]

                self.__runStartCommand(mirrorDbs, startMethod, numContentsInCluster, result, gpArray, era)
                self.__runStartCommand(primaryDbs, startMethod, numContentsInCluster, result, gpArray, era)

            elif startMethod == START_AS_MIRRORLESS:
                # bring them up in mirrorless mode
                self.__runStartCommand(segments, startMethod, numContentsInCluster, result, gpArray, era)
            else:
                raise Exception("Invalid startMethod %s" % startMethod)

        # done!
        assert totalToAttempt == len(result.getFailedSegmentObjs()) + len(result.getSuccessfulSegments())
        return result

    def __runStartCommand(self, segments, startMethod, numContentsInCluster, resultOut, gpArray, era):
        """
        Putt results into the resultOut object
        """

        if len(segments) == 0:
            return

        if startMethod == START_AS_PRIMARY_OR_MIRROR:
            logger.info("Commencing parallel primary and mirror segment instance startup, please wait...")
        else:
            logger.info("Commencing parallel segment instance startup, please wait...")

        dbIdToPeerMap = gpArray.getDbIdToPeerMap()

        mirroringModePreTransition = MIRROR_MODE_MIRRORLESS if startMethod == START_AS_MIRRORLESS else MIRROR_MODE_QUIESCENT

        # launch the start
        for hostName, segments in GpArray.getSegmentsByHostName(segments).iteritems():
            logger.debug("Dispatching command to start segments on host: %s, " \
                            "with %s contents in cluster" % (hostName, numContentsInCluster))

            pickledTransitionData = None
            if startMethod == START_AS_PRIMARY_OR_MIRROR:
                mirroringModePerSegment = []
                for seg in segments:
                    modeThisSegment = MIRROR_MODE_PRIMARY if seg.isSegmentPrimary(True) else MIRROR_MODE_MIRROR
                    mirroringModePerSegment.append(modeThisSegment)
                pickledTransitionData = self.__createPickledTransitionParameters(segments, mirroringModePerSegment, None, dbIdToPeerMap)

            #
            # This will call sbin/gpsegstart.py
            #
            cmd = gp.GpSegStartCmd("remote segment starts on host '%s'" % hostName,
                                   self.__gpHome, segments,
                                   self.__gpVersion,
                                   mirroringModePreTransition,
                                   numContentsInCluster,
                                   era,
                                   self.master_checksum_value,
                                   self.__timeout,
                                   verbose=logging_is_verbose(),
                                   ctxt=base.REMOTE,
                                   remoteHost=segments[0].getSegmentAddress(),
                                   pickledTransitionData=pickledTransitionData,
                                   specialMode=self.__specialMode,
                                   wrapper=self.__wrapper,
                                   wrapper_args=self.__wrapper_args,
                                   parallel=self.__parallel,
                                   logfileDirectory=self.logfileDirectory)
            self.__workerPool.addCommand(cmd)

        if self.__quiet:
            self.__workerPool.join()
        else:
            base.join_and_indicate_progress(self.__workerPool)

        # process results
        self.__processStartOrConvertCommands(resultOut)
        self.__workerPool.empty_completed_items()

    def __processStartOrConvertCommands(self, resultOut):
        logger.info("Process results...")
        for cmd in self.__workerPool.getCompletedItems():
            if cmd.get_results().rc == 0 or cmd.get_results().rc == 1:
            # error code 0 mean all good, 1 means it ran but at least one thing failed
                cmdout = cmd.get_results().stdout
                lines=cmdout.split('\n')
                for line in lines:
                    if line.startswith("STATUS"):
                        fields=line.split('--')

                        index = 1
                        dir = fields[index].split(':')[1]
                        index += 1

                        started = fields[index].split(':')[1]
                        index += 1

                        reasonCode = gp.SEGSTART_ERROR_UNKNOWN_ERROR
                        if fields[index].startswith("REASONCODE:"):
                            reasonCode = int(fields[index].split(":")[1])
                            index += 1

                        # The funny join and splits are because Reason could have colons or -- in the text itself
                        reasonStr = "--".join(fields[index:])
                        reasonArr = reasonStr.split(':')
                        reasonArr = reasonArr[1:]
                        reasonStr = ":".join(reasonArr)

                        if started.lower() == 'false':
                            success=False
                        else:
                            success=True

                        for segment in cmd.dblist:
                            if segment.getSegmentDataDirectory() == dir:
                                if success:
                                    resultOut.addSuccess(segment)
                                else:
                                    resultOut.addFailure(segment, reasonStr, reasonCode)
            else:
                for segment in cmd.dblist:
                    resultOut.addFailure(segment, cmd.get_results(), gp.SEGSTART_ERROR_UNKNOWN_ERROR)

    def __createPickledTransitionParameters(self, segmentsOnHost, targetModePerSegment, convertUsingFullResync, dbIdToPeerMap):
        dbIdToFullResync = {}
        if convertUsingFullResync is not None:
            for i in range(len(segmentsOnHost)):
                if convertUsingFullResync[i]:
                    dbIdToFullResync[segmentsOnHost[i].getSegmentDbId()] = True

        hostData = {}
        hostData["dbsByPort"] = {}
        for i, db in enumerate(segmentsOnHost):
            peer = dbIdToPeerMap[db.getSegmentDbId()]
            dbData = {}
            dbData["dbid"] = db.getSegmentDbId()
            dbData["mode"] = db.getSegmentMode()
            dbData["hostName"] = db.getSegmentAddress()
            dbData["peerName"] = peer.getSegmentAddress()
            dbData["peerPMPort"] = peer.getSegmentPort()
            dbData["fullResyncFlag"] = db.getSegmentDbId() in dbIdToFullResync
            dbData["targetMode"] = targetModePerSegment[i]

            hostData["dbsByPort"][db.getSegmentPort()] = dbData

        return base64.urlsafe_b64encode(pickle.dumps(hostData))

