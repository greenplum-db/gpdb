import datetime
#TODO: check if pickle is better
import json

from gppylib import gplog


class RecoveryInfo(object):
    """
    This class encapsulates the information needed on a segment host
    to run full/incremental recovery for a segment.

    Note: we don't have target hostname, since an object of this class will be accessed by the target host directly
    """
    def __init__(self, target_datadir, target_port, target_segment_dbid, source_hostname, source_port,
                 is_full_recovery, progress_file):
        self.target_datadir = target_datadir
        self.target_port = target_port
        self.target_segment_dbid = target_segment_dbid

        # TODO: use address instead of hostname ?
        self.source_hostname = source_hostname
        self.source_port = source_port
        self.is_full_recovery = is_full_recovery
        self.progress_file = progress_file

    def __str__(self):
        return json.dumps(self, default=lambda o: o.__dict__)

    def __eq__(self, cmp_recovery_info):
        return str(self) == str(cmp_recovery_info)


def serialize_recovery_info_list(recovery_info_list):
    return json.dumps(recovery_info_list, default=lambda o: o.__dict__)


def deserialize_recovery_info_list(serialized_string):
    deserialized_list = json.loads(serialized_string)
    return [RecoveryInfo(**i) for i in deserialized_list]


def build_recovery_info(mirrors_to_build):
    """
    This function is used to format recovery information to send to each segment host

    @param mirrors_to_build:  list of mirrors that need recovery

    @return A dictionary with the following format:

            Key   =   <host name>
            Value =   list of RecoveryInfos - one RecoveryInfo per segment on that host
    """
    timestamp = datetime.datetime.today().strftime('%Y%m%d_%H%M%S')

    recovery_info_by_host = {}
    for to_recover in mirrors_to_build:

        source_segment = to_recover.getLiveSegment()
        target_segment = to_recover.getFailoverSegment() or to_recover.getFailedSegment()

        # TODO: move the progress file naming to gpsegrecovery
        process_name = 'pg_basebackup' if to_recover.isFullSynchronization() else 'pg_rewind'
        progress_file = '{}/{}.{}.dbid{}.out'.format(gplog.get_logger_dir(), process_name, timestamp,
                                             target_segment.getSegmentDbId())

        hostname = target_segment.getSegmentHostName()

        if hostname not in recovery_info_by_host:
            recovery_info_by_host[hostname] = []
        recovery_info_by_host[hostname].append(RecoveryInfo(
            target_segment.getSegmentDataDirectory(), target_segment.getSegmentPort(),
            target_segment.getSegmentDbId(), source_segment.getSegmentHostName(),
            source_segment.getSegmentPort(), to_recover.isFullSynchronization(),
            progress_file))
    return recovery_info_by_host
