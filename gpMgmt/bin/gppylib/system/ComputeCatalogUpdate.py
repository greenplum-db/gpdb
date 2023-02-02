#!/usr/bin/env python3
# Line too long - pylint: disable=C0301
# Invalid name  - pylint: disable=C0103

"""
  ComputeCatalogUpdate.py
  Copyright (c) EMC/Greenplum Inc 2011. All Rights Reserved. 

  Used by updateSystemConfig() to compare the db state and 
  goal state of a gpArray containing the Greenplum segment 
  configruation details and computes appropriate changes.
"""
import copy
from gppylib.gplog import *
from gppylib.gparray import ROLE_PRIMARY, ROLE_MIRROR, COORDINATOR_CONTENT_ID

logger = get_default_logger()

class ComputeCatalogUpdate:
    """
    Helper class for GpConfigurationProvider.updateSystemConfig().
    
    This computes seven lists of GpDb objects (commonly referenced as 'seg') 
    from a GpArray, reflecting the logical changes that need to be made
    to the database catalog to make it match the system as defined.
    The names of the lists are reasonably descriptive:

    mirror_to_remove	     - to be removed (e.g. via gp_remove_segment_mirror())
    primary_to_remove	     - to be removed (e.g. via gp_remove_segment())
    primary_to_add	     - to be added (e.g. via gp_add_segment_primary())
    mirror_to_add	     - to be added (e.g. via gp_add_segment_mirror())
    mirror_to_remove_and_add - change or force list requires this mirror
                                to be removed and then added back
    segment_to_update	     - to be updated (e.g. via SQL)
    segment_unchanged	     - needs no update (included for validation)
    """

    def __init__(self, gpArray, forceMap, useUtilityMode, allowPrimary):
        """
        This class just holds lists of objects in the underlying gpArray.  
        As such, it has no methods - the constructor does all the computation.

        @param gpArray the array containing the goal and db segment states.
        @param forceMap a map of dbid->True for mirrors for which we should force updating via remove/add
        @param useUtilityMode True if the operations we're doing are expected to run via utility mode
        @param allowPrimary True if caller authorizes add/remove primary operations (e.g. gpexpand)
        """

        forceMap = forceMap or {}
        self.useUtilityMode = useUtilityMode
        self.allowPrimary = allowPrimary

        # 'dbsegmap' reflects the current state of the catalog
        self.dbsegmap    = dict([(seg.getSegmentDbId(), seg) for seg in gpArray.getSegmentsAsLoadedFromDb()])

        # 'goalsegmap' reflects the desired state of the catalog
        self.goalsegmap  = dict([(seg.getSegmentDbId(), seg) for seg in gpArray.getDbList(includeExpansionSegs=True)])

        # find mirrors and primaries to remove
        self.mirror_to_remove = [
            seg for seg in list(self.dbsegmap.values())		        # segment in database
                 if seg.isSegmentMirror()                               # segment is a mirror
                and (seg.getSegmentDbId() not in self.goalsegmap)       # but not in goal configuration
        ]
        self.debuglog("mirror_to_remove:          %s", self.mirror_to_remove)

        self.primary_to_remove = [
            seg for seg in list(self.dbsegmap.values())              		# segment is database
                 if seg.isSegmentPrimary()                      	# segment is a primary
                and (seg.getSegmentDbId() not in self.goalsegmap)       # but not in goal configuration
        ]
        self.debuglog("primary_to_remove:         %s", self.primary_to_remove)

        # find primaries and mirrors to add
        self.primary_to_add = [
            seg for seg in list(self.goalsegmap.values())			# segment in goal configuration
                 if seg.isSegmentPrimary()				# segment is a primary
                and (seg.getSegmentDbId() not in self.dbsegmap)		# but not in the database
        ]
        self.debuglog("primary_to_add:            %s", self.primary_to_add)

        self.mirror_to_add = [
            seg for seg in list(self.goalsegmap.values())			# segment in goal configuration
                 if seg.isSegmentMirror()				# segment is a mirror
                and (seg.getSegmentDbId() not in self.dbsegmap)		# but not in the database
        ]
        self.debuglog("mirror_to_add:             %s", self.mirror_to_add)

        # find segments to update
        initial_segment_to_update = [
            seg for seg in list(self.goalsegmap.values())			# segment in goal configuration
                 if (seg.getSegmentDbId() in self.dbsegmap)          	# and also in the database 
                and (seg != self.dbsegmap[ seg.getSegmentDbId() ])   	# but some attributes differ
        ]
        self.debuglog("initial_segment_to_update: %s", initial_segment_to_update)

        # create a map of the segments which we can't update in the 
        # ordinary way either because they were on the forceMap or
        # they differ in an attribute other than mode or status
        removeandaddmap = {}
        for seg in initial_segment_to_update:
            dbid = seg.getSegmentDbId()
            if dbid in forceMap:
                removeandaddmap[dbid] = seg
                continue
            if not seg.equalIgnoringModeAndStatus(self.dbsegmap[dbid]):
                removeandaddmap[dbid] = seg
                continue

        # create list of mirrors to update via remove/add
        self.mirror_to_remove_and_add = [seg for seg in list(removeandaddmap.values())]
        self.debuglog("mirror_to_remove_and_add:  %s", self.mirror_to_remove_and_add)

        # find segments to update in the ordinary way
        self.segment_to_update = [
            seg for seg in initial_segment_to_update			# segments to update
                 if seg.getSegmentDbId() not in removeandaddmap 	# that don't require remove/add
        ]
        self.debuglog("segment_to_update:         %s", self.segment_to_update)

        # find segments that don't need change
        self.segment_unchanged = [
            seg for seg in list(self.goalsegmap.values())			# segment in goal configuration
                 if (seg.getSegmentDbId() in self.dbsegmap)          	# and also in the database 
                and (seg == self.dbsegmap[ seg.getSegmentDbId() ])   	# and attribtutes are all the same
        ]
        self.debuglog("segment_unchanged:         %s", self.segment_unchanged)


                
    def final_segments(self):
        """
        Generate a series of segments appearing in the final configuration.
        """
        for seg in self.primary_to_add:
            yield seg
        for seg in self.mirror_to_add:
            yield seg
        for seg in self.mirror_to_remove_and_add:
            yield seg
        for seg in self.segment_to_update:
            yield seg
        for seg in self.segment_unchanged:
            yield seg


    def validate(self):
        """
        Check that the operation and new configuration is valid.
        """

        # Validate that we're not adding or removing primaries unless authorized
        #
        if not self.allowPrimary:
            if len(self.primary_to_add) > 0:
                p = self.primary_to_add[0]
                raise Exception("Internal error: Operation may not add primary: %s" % repr(p))

            if len(self.primary_to_remove) > 0:
                p = self.primary_to_remove[0]
                raise Exception("Internal error: Operation may not remove primary: %s" % repr(p))


        # Validate that operations do not result in a contentid with a pair of segments in same preferred role
        #
        final = { ROLE_PRIMARY:{}, ROLE_MIRROR:{} }
        for seg in self.final_segments():
            subset = final[ seg.getSegmentPreferredRole() ]
            other  = subset.get( seg.getSegmentContentId() )
            if other is not None:
                raise Exception("Segments sharing a content id may not have same preferred role: %s and %s" % (repr(seg), repr(other)))
            subset[ seg.getSegmentContentId() ] = seg


        # Validate that if we have any mirrors, that all primaries have mirrors
        #
        only_contains_standby_mirror = (len(final[ ROLE_MIRROR ]) == 1 and final[ ROLE_MIRROR ].get(-1) != None)
        if len(final[ ROLE_MIRROR ]) > 0 and not only_contains_standby_mirror:
            for contentId in final[ ROLE_PRIMARY ]:
                if contentId != COORDINATOR_CONTENT_ID and final[ ROLE_MIRROR ].get( contentId ) is None:
                    seg = final[ ROLE_PRIMARY ][ contentId ]
                    raise Exception("Primary must have mirror when mirroring enabled: %s" % repr(seg))


        # Validate that the remove/add list contains only qualified mirrors.
        # In particular, we disallow remove/add of the coordinator, standby or a primary.
        #
        for seg in self.mirror_to_remove_and_add:
            originalSeg = self.dbsegmap.get(seg.getSegmentDbId())

            # filespace and other core has changed, or it's a mirror and we are recovering full
            #     (in which case we want to call removeMirror and addMirror so we mark
            #      the primary as full-resyncing)
            #
            if seg.isSegmentCoordinator(current_role=True) or seg.isSegmentStandby(current_role=True):

                #
                # Assertion here -- user should not be allowed to change coordinator/standby info.
                #
                raise Exception("Internal error: Can only change core details of segments, not coordinators" \
                                " (on segment %s) (seg %s vs original %s)" %
                                (seg.getSegmentDbId(), repr(seg), repr(originalSeg)))

            if not seg.isSegmentMirror(current_role=True):
                #
                # Assertion here -- user should not be allowed to change primary info.
                #
                raise Exception("Internal error: Can only change core details of mirrors, not primaries" \
                                " (on segment %s) (seg %s vs original %s)" %
                                (seg.getSegmentDbId(), repr(seg), repr(originalSeg)))

            if self.useUtilityMode:
                raise Exception("Cannot change core details of mirrors in utility mode")



    def debuglog(self, msg, seglist):
        """
        Write debugging details about the specified segment list.
        """
        logger.debug(msg % ("%s segments" % len(seglist)))
        for seg in seglist:
            logger.debug(msg % repr(seg))

