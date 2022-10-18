#!/usr/bin/env python
#
# Copyright (c) Greenplum Inc 2010. All Rights Reserved.
# Copyright (c) EMC/Greenplum Inc 2011. All Rights Reserved.
#
"""
This file defines the interface that can be used to fetch and update system
configuration information.
"""
import os, copy
from collections import defaultdict

from gppylib.gplog import *
from gppylib.utils import checkNotNone
from gppylib.system.configurationInterface import *
from gppylib.system.ComputeCatalogUpdate import ComputeCatalogUpdate
from gppylib.gparray import GpArray, Segment, InvalidSegmentConfiguration
from gppylib import gparray
from gppylib.db import dbconn
from gppylib.commands.gp import get_local_db_mode

logger = get_default_logger()

class GpConfigurationProviderUsingGpdbCatalog(GpConfigurationProvider) :
    """
    An implementation of GpConfigurationProvider will provide functionality to
    fetch and update gpdb system configuration information (as stored in the
    database)

    Note that the client of this is assuming that the database data is not
    changed by another party between the time segment data is loaded and when it
    is updated
    """

    def __init__(self):
        self.__masterDbUrl = None


    def initializeProvider( self, masterPort ) :
        """
        Initialize the provider to get information from the given master db, if
        it chooses to get its data from the database

        returns self
        """

        checkNotNone("masterPort", masterPort)

        dbUrl = dbconn.DbURL(port=masterPort, dbname='template1')

        self.__masterDbUrl = dbUrl
        return self


    def loadSystemConfig( self, useUtilityMode, verbose=True ) :
        """
        Load all segment information from the configuration source.

        Returns a new GpArray object
        """

        # ensure initializeProvider() was called
        checkNotNone("masterDbUrl", self.__masterDbUrl)

        if verbose :
            logger.info("Obtaining Segment details from master...")

        array = GpArray.initFromCatalog(self.__masterDbUrl, useUtilityMode)

        if get_local_db_mode(array.master.getSegmentDataDirectory()) != 'UTILITY':
            logger.debug("Validating configuration...")
            if not array.is_array_valid():
                raise InvalidSegmentConfiguration(array)

        return array

    def updateSystemConfig( self, gpArray, textForConfigTable, dbIdToForceMirrorRemoveAdd, useUtilityMode, allowPrimary) :
        """
        Update the configuration for the given segments in the underlying
        configuration store to match the current values

        Also resets any dirty bits on saved/updated objects

        @param textForConfigTable label to be used when adding to segment configuration history
        @param dbIdToForceMirrorRemoveAdd a map of dbid -> True for mirrors for which we should force updating the mirror
        @param useUtilityMode True if the operations we're doing are expected to run via utility mode
        @param allowPrimary True if caller authorizes add/remove primary operations (e.g. gpexpand)
        """

        # ensure initializeProvider() was called
        checkNotNone("masterDbUrl", self.__masterDbUrl)

        logger.debug("Validating configuration changes...")

        if not gpArray.is_array_valid():
            logger.critical("Configuration is invalid")
            raise InvalidSegmentConfiguration(gpArray)

        conn = dbconn.connect(self.__masterDbUrl, useUtilityMode, allowSystemTableMods=True)
        dbconn.execSQL(conn, "BEGIN")

        # compute what needs to be updated
        update = ComputeCatalogUpdate(gpArray, dbIdToForceMirrorRemoveAdd, useUtilityMode, allowPrimary)
        update.validate()

        # put the mirrors in a map by content id so we can update them later
        mirror_map = {}
        for seg in update.mirror_to_add:
            mirror_map[ seg.getSegmentContentId() ] = seg

        # create a map by dbid in which to put backout SQL statements
        backout_map = defaultdict(list)

        # remove mirror segments (e.g. for gpexpand rollback)
        for seg in update.mirror_to_remove:
            addSQL = self.updateSystemConfigRemoveMirror(conn, gpArray, seg, textForConfigTable)
            backout_map[seg.getSegmentDbId()].append(addSQL)
            backout_map[seg.getSegmentDbId()].append(self.getPeerNotInSyncSQL(gpArray, seg))

        # remove primary segments (e.g for gpexpand rollback)
        for seg in update.primary_to_remove:
            addSQL = self.updateSystemConfigRemovePrimary(conn, gpArray, seg, textForConfigTable)
            backout_map[seg.getSegmentDbId()].append(addSQL)
            backout_map[seg.getSegmentDbId()].append(self.getPeerNotInSyncSQL(gpArray, seg))

        # add new primary segments
        for seg in update.primary_to_add:
            removeSQL = self.updateSystemConfigAddPrimary(conn, gpArray, seg, textForConfigTable, mirror_map)
            backout_map[seg.getSegmentDbId()].append(removeSQL)
            backout_map[seg.getSegmentDbId()].append(self.getPeerNotInSyncSQL(gpArray, seg))

        # add new mirror segments
        for seg in update.mirror_to_add:
            removeSQL = self.updateSystemConfigAddMirror(conn, gpArray, seg, textForConfigTable)
            backout_map[seg.getSegmentDbId()].append(removeSQL)
            backout_map[seg.getSegmentDbId()].append(self.getPeerNotInSyncSQL(gpArray, seg))

        # remove and add mirror segments necessitated by catalog attribute update
        for seg in update.mirror_to_remove_and_add:
            addSQL, removeSQL = self.updateSystemConfigRemoveAddMirror(conn, gpArray, seg, textForConfigTable)
            backout_map[seg.getSegmentDbId()].append(removeSQL)
            backout_map[seg.getSegmentDbId()].append(addSQL)
            backout_map[seg.getSegmentDbId()].append(self.getPeerNotInSyncSQL(gpArray, seg))

        # apply updates to existing segments
        for seg in update.segment_to_update:
            originalSeg = update.dbsegmap.get(seg.getSegmentDbId())
            self.__updateSystemConfigUpdateSegment(conn, gpArray, seg, originalSeg, textForConfigTable)

        # commit changes
        logger.debug("Committing configuration table changes")
        dbconn.execSQL(conn, "COMMIT")
        conn.close()

        gpArray.setSegmentsAsLoadedFromDb([seg.copy() for seg in gpArray.getDbList()])

        return backout_map


    def updateSystemConfigRemoveMirror(self, conn, gpArray, seg, textForConfigTable):
        """
        Remove a mirror segment currently in gp_segment_configuration
        but not present in the goal configuration and record our action
        in gp_configuration_history.
        """
        dbId   = seg.getSegmentDbId()
        addSQL = self.__callSegmentRemoveMirror(conn, gpArray, seg)
        self.__insertConfigHistory(conn, dbId, "%s: removed mirror segment configuration" % textForConfigTable)
        return addSQL


    def updateSystemConfigRemovePrimary(self, conn, gpArray, seg, textForConfigTable):
        """
        Remove a primary segment currently in gp_segment_configuration
        but not present in the goal configuration and record our action
        in gp_configuration_history.
        """
        dbId = seg.getSegmentDbId()
        addSQL = self.__callSegmentRemove(conn, gpArray, seg)
        self.__insertConfigHistory(conn, dbId, "%s: removed primary segment configuration" % textForConfigTable)
        return addSQL


    def updateSystemConfigAddPrimary(self, conn, gpArray, seg, textForConfigTable, mirror_map):
        """
        Add a primary segment specified in our goal configuration but
        which is missing from the current gp_segment_configuration table
        and record our action in gp_configuration_history.
        """
        # lookup the mirror (if any) so that we may correct its content id
        mirrorseg = mirror_map.get( seg.getSegmentContentId() )

        # add the new segment
        dbId, removeSQL = self.__callSegmentAdd(conn, gpArray, seg)

        # gp_add_segment_primary() will update the mode and status.

        # get the newly added segment's content id
        sql = "SELECT content FROM pg_catalog.gp_segment_configuration WHERE dbId = %s" % self.__toSqlIntValue(seg.getSegmentDbId())
        logger.debug(sql)
        sqlResult = self.fetchSingleOutputRow(conn, sql)
        contentId = int(sqlResult[0])

        # Set the new content id for the primary as well the mirror if present.
        seg.setSegmentContentId(contentId)
        if mirrorseg is not None:
            mirrorseg.setSegmentContentId(contentId)

        self.__insertConfigHistory(conn, dbId, "%s: inserted primary segment configuration with contentid %s" % (textForConfigTable, contentId))
        return removeSQL


    def updateSystemConfigAddMirror(self, conn, gpArray, seg, textForConfigTable):
        """
        Add a mirror segment specified in our goal configuration but
        which is missing from the current gp_segment_configuration table
        and record our action in gp_configuration_history.
        """
        dbId, removeSQL = self.__callSegmentAddMirror(conn, gpArray, seg)
        self.__insertConfigHistory(conn, dbId, "%s: inserted mirror segment configuration" % textForConfigTable)
        return removeSQL


    def updateSystemConfigRemoveAddMirror(self, conn, gpArray, seg, textForConfigTable):
        """
        We've been asked to update the mirror in a manner that require
        it to be removed and then re-added.   Perform the tasks
        and record our action in gp_configuration_history.
        """
        origDbId = seg.getSegmentDbId()
        addSQL = self.__callSegmentRemoveMirror(conn, gpArray, seg)

        dbId, removeSQL = self.__callSegmentAddMirror(conn, gpArray, seg, removeAndAdd=True)

        self.__insertConfigHistory(conn, seg.getSegmentDbId(),
                                   "%s: inserted segment configuration for full recovery or original dbid %s" \
                                   % (textForConfigTable, origDbId))
        return addSQL, removeSQL


    def __updateSystemConfigUpdateSegment(self, conn, gpArray, seg, originalSeg, textForConfigTable):

        # update mode and status
        #
        what = "%s: segment mode and status"
        self.__updateSegmentModeStatus(conn, seg)

        self.__insertConfigHistory(conn, seg.getSegmentDbId(), what % textForConfigTable)

    # This is a helper function for creating backout scripts, since we need to use the original segment information,
    # not the segment information after it has been updated to facilitate recovery.  Not all code paths result in the
    # segments-as-loaded array being populated, hence the None checks.
    def __getSegmentAsLoaded(self, gpArray, seg):
        segments = gpArray.getSegmentsAsLoadedFromDb()
        if segments is not None:
            matching_segment = [s for s in segments if s.getSegmentDbId() == seg.getSegmentDbId()]
            if matching_segment:
                return matching_segment[0]
        return seg

    def __getConfigurationHistorySQL(self, dbid):
        sql = ";\nINSERT INTO gp_configuration_history (time, dbid, \"desc\") VALUES(\n\tnow(),\n\t%s,\n\t%s\n)" \
            % (
                self.__toSqlIntValue(dbid),
                "'gprecoverseg: segment config for backout: inserted segment configuration for full recovery or original dbid %d'" % dbid,
              )
        return sql

    #
    # The below __callSegment[Action][Target] functions return the SQL statements to reverse the changes they make
    # (not the SQL statements they actually call), to be used to generate backout scripts to reverse the changes later.
    #

    def __callSegmentRemoveMirror(self, conn, gpArray, seg):
        """
        Call gp_remove_segment_mirror() to remove the mirror.
        """
        sql = self.__getSegmentRemoveMirrorSQL(seg)
        logger.debug(sql)
        result = self.fetchSingleOutputRow(conn, sql)
        assert result[0] # must return True
        return self.__getSegmentAddSQL(self.__getSegmentAsLoaded(gpArray, seg), backout=True)

    def __getSegmentRemoveMirrorSQL(self, seg, backout=False):
        sql = "SELECT gp_remove_segment_mirror(%s::int2)" % (self.__toSqlIntValue(seg.getSegmentContentId()))
        if backout:
            sql += self.__getConfigurationHistorySQL(seg.getSegmentDbId())
        return sql

    def __callSegmentRemove(self, conn, gpArray, seg):
        """
        Call gp_remove_segment() to remove the primary.
        """
        sql = self.__getSegmentRemoveSQL(seg)
        logger.debug(sql)
        result = self.fetchSingleOutputRow(conn, sql)
        assert result[0]
        return self.__getSegmentAddMirrorSQL(self.__getSegmentAsLoaded(gpArray, seg), backout=True)

    def __getSegmentRemoveSQL(self, seg, backout=False, removeAndAdd=False):
        sql = "SELECT gp_remove_segment(%s::int2)" % (self.__toSqlIntValue(seg.getSegmentDbId()))
        # Don't generate a configuration history line in the updateSystemConfigRemoveAddMirror case,
        # to avoid duplication; the later call to __getSegmentAddSQL will take care of that.
        if backout and not removeAndAdd:
            sql += self.__getConfigurationHistorySQL(seg.getSegmentDbId())
        return sql

    def __callSegmentAdd(self, conn, gpArray, seg):
        """
        Ideally, should call gp_add_segment_primary() to add the
        primary. But due to chicken-egg problem, need dbid for
        creating the segment but can't add to catalog before creating
        segment. Hence, instead using gp_add_segment() which takes
        dbid and registers in catalog using the same.  Return the new
        segment's dbid.
        """
        logger.debug('callSegmentAdd %s' % repr(seg))

        sql = self.__getSegmentAddSQL(seg)
        logger.debug(sql)
        sqlResult = self.fetchSingleOutputRow(conn, sql)
        dbId = int(sqlResult[0])
        removeSQL = self.__getSegmentRemoveMirrorSQL(self.__getSegmentAsLoaded(gpArray, seg), backout=True)
        seg.setSegmentDbId(dbId)
        return dbId, removeSQL

    def __getSegmentAddSQL(self, seg, backout=False):
        sql = "SELECT gp_add_segment(%s::int2, %s::int2, '%s', '%s', 'n', '%s', %s, %s, %s, %s)" \
            % (
                self.__toSqlIntValue(seg.getSegmentDbId()),
                self.__toSqlIntValue(seg.getSegmentContentId()),
                'm' if backout else 'p',
                seg.getSegmentPreferredRole(),
                'd' if backout else 'u',
                self.__toSqlIntValue(seg.getSegmentPort()),
                self.__toSqlTextValue(seg.getSegmentHostName()),
                self.__toSqlTextValue(seg.getSegmentAddress()),
                self.__toSqlTextValue(seg.getSegmentDataDirectory()),
              )
        if backout:
            sql += self.__getConfigurationHistorySQL(seg.getSegmentDbId())
        return sql

    def __callSegmentAddMirror(self, conn, gpArray, seg, removeAndAdd=False):
        """
        Similar to __callSegmentAdd, ideally we should call gp_add_segment_mirror() to add the mirror.
        But chicken-egg problem also exists in mirror case. If we use gp_add_segment_mirror(),
        new dbid will be chosen by `get_availableDbId()`, which cannot ensure to be same as dbid
        in internal.auto.conf(see issue-9837). Refer to __callSegmentAdd for details.
        """
        logger.debug('callSegmentAddMirror %s' % repr(seg))

        sql = self.__getSegmentAddMirrorSQL(seg)

        logger.debug(sql)
        sqlResult = self.fetchSingleOutputRow(conn, sql)
        dbId = int(sqlResult[0])
        removeSQL = self.__getSegmentRemoveSQL(self.__getSegmentAsLoaded(gpArray, seg), backout=True, removeAndAdd=removeAndAdd)
        seg.setSegmentDbId(dbId)
        return dbId, removeSQL

    def __getSegmentAddMirrorSQL(self, seg, backout=False):
        #TODO should we use seg.getSegmentPreferredRole()
        sql = "SELECT gp_add_segment(%s::int2, %s::int2, 'm', 'm', 'n', 'd', %s, %s, %s, %s)" \
            % (
                self.__toSqlIntValue(seg.getSegmentDbId()),
                self.__toSqlIntValue(seg.getSegmentContentId()),
                self.__toSqlIntValue(seg.getSegmentPort()),
                self.__toSqlTextValue(seg.getSegmentHostName()),
                self.__toSqlTextValue(seg.getSegmentAddress()),
                self.__toSqlTextValue(seg.getSegmentDataDirectory()),
              )
        if backout:
            sql += self.__getConfigurationHistorySQL(seg.getSegmentDbId())
        return sql

    # This function generates a statement to update the mode of a given segment's peer to
    # "not in sync", something that is necessary to do for every segment that is added in a
    # backout script.  The gp_add_segment function sets the added segment's mode to 'n', and
    # if its peer's mode doesn't match (is still set to 's') when the next query is executed,
    # this will almost certainly crash the cluster.
    def getPeerNotInSyncSQL(self, gpArray, seg):
        peerMap = gpArray.getDbIdToPeerMap()
        dbid = seg.getSegmentDbId()
        if dbid in peerMap: # The dbid may not be in the peer map, if e.g. we're getting here from gpexpand, in which case no action is necessary
            peerSegment = peerMap[dbid]
            updateStmt = "SET allow_system_table_mods=true;\nUPDATE gp_segment_configuration SET mode = 'n' WHERE dbid = %d;"
            return updateStmt % peerSegment.getSegmentDbId()
        return ""

    def __updateSegmentModeStatus(self, conn, seg):
        # run an update
        sql = "UPDATE pg_catalog.gp_segment_configuration\n" + \
            "  SET\n" + \
            "  mode = " + self.__toSqlCharValue(seg.getSegmentMode()) + ",\n" \
            "  status = " + self.__toSqlCharValue(seg.getSegmentStatus()) + "\n" \
            "WHERE dbid = " + self.__toSqlIntValue(seg.getSegmentDbId()) + ";"
        logger.debug(sql)
        dbconn.executeUpdateOrInsert(conn, sql, 1)


    def fetchSingleOutputRow(self, conn, sql, retry=False):
        """
        Execute specified SQL command and return what we expect to be a single row.
        Raise an exception when more or fewer than one row is seen and when more
        than one row is seen display up to 10 rows as logger warnings.
        """
        cursor   = dbconn.execSQL(conn, sql)
        numrows  = cursor.rowcount
        numshown = 0
        res      = None
        for row in cursor:
            if numrows != 1:
                #
                # if we got back more than one row
                # we print a few of the rows first
                # instead of immediately raising an exception
                #
                numshown += 1
                if numshown > 10:
                    break
                logger.warning('>>> %s' % row)
            else:
                assert res is None
                res = row
                assert res is not None
        cursor.close()
        if numrows != 1:
            raise Exception("SQL returned %d rows, not 1 as expected:\n%s" % (numrows, sql))
        return res


    def __insertConfigHistory(self, conn, dbId, msg ):
        # now update change history
        sql = "INSERT INTO gp_configuration_history (time, dbid, \"desc\") VALUES(\n" \
                    "now(),\n  " + \
                    self.__toSqlIntValue(dbId) + ",\n  " + \
                    self.__toSqlCharValue(msg) + "\n)"
        logger.debug(sql)
        dbconn.executeUpdateOrInsert(conn, sql, 1)

    def __toSqlIntValue(self, val):
        if val is None:
            return "null"
        return str(val)

    def __toSqlArrayStringValue(self, val):
        if val is None:
            return "null"
        return '"' + val.replace('"','\\"').replace('\\','\\\\') + '"'

    def __toSqlCharValue(self, val):
        return self.__toSqlTextValue(val)

    def __toSqlTextValue(self, val):
        if val is None:
            return "null"
        return "'" + val.replace("'","''").replace('\\','\\\\') + "'"
