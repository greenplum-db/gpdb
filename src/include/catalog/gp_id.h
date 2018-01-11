/*-------------------------------------------------------------------------
 *
 * gp_id.h
 *	  definition of the system "database identifier" relation (gp_dbid)
 *	  along with the relation's initial contents.
 *
 * Portions Copyright (c) 2009-2010, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/catalog/gp_id.h
 *
 * NOTES
 *    Historically this table was used to supply every segment with its
 * identification information.  However in the 4.0 release when the file
 * replication feature was added it could no longer serve this purpose
 * because it became a requirement for all tables to have the same physical
 * contents on both the primary and mirror segments.  To resolve this the
 * information is now passed to each segment on startup based on the
 * gp_segment_configuration (stored on the master only), and each segment
 * has a file in its datadirectory (gp_dbid) that uniquely identifies the
 * segment.
 *
 *   The contents of the table are now irrelevant, with the exception that
 * several tools began relying on this table for use as a method of remote
 * function invocation via gp_dist_random('gp_id') due to the fact that this
 * table was guaranteed of having exactly one row on every segment.  The
 * contents of the row have no defined meaning, but this property is still
 * relied upon.
 */
#ifndef _GP_ID_H_
#define _GP_ID_H_


#include "catalog/genbki.h"
/*
 * Defines for gp_id table
 */
#define GpIdRelationName			"gp_id"

#define GpIdRelationId	5001

CATALOG(gp_id,5001) BKI_SHARED_RELATION BKI_WITHOUT_OIDS
{
	NameData	gpname;
	int2		numsegments;
	int2		dbid;
	int2		content;
} FormData_gp_id;

/* no foreign keys */

#define Natts_gp_id				4
#define Anum_gp_id_gpname		1
#define Anum_gp_id_numsegments	2
#define Anum_gp_id_dbid			3
#define Anum_gp_id_content		4

/*
 * The contract of the gp_id table is that it must have exactly one row on
 * every segment.  The contents of the row do not matter.
 */
DATA(insert (Greenplum -1 -1 -1));

#endif /*_GP_ID_H_*/
