/*-------------------------------------------------------------------------
 *
 * cdbfilerepdefs.c
 *  
 * Portions Copyright (c) 2009-2010, Greenplum Inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/access/transam/filerepdefs.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/filerepdefs.h"

char * 
MirrorDataLossTrackingState_Name(MirrorDataLossTrackingState state)
{
	switch (state) {
		case MirrorDataLossTrackingState_MirrorNotConfigured:
			return "Not Configured";
			
		case MirrorDataLossTrackingState_MirrorCurrentlyUpInResync:
			return "Currently Up (In Resync)";
			
		case MirrorDataLossTrackingState_MirrorCurrentlyUpInSync:
			return "Currently Up (In Sync)";
			
		case MirrorDataLossTrackingState_MirrorDown:
			return "Mirror Down";

		default:
			return "Unknown";
	}	
}

