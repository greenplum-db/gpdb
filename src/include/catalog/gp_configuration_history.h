/*-------------------------------------------------------------------------
 *
 * gp_configuration_history.h
 *	  changes in Greenplum system configuration captured in chronological order
 *
 *
 * Portions Copyright (c) 2006-2010, Greenplum inc.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/catalog/gp_configuration_history.h
 *
 * NOTES
 *	  the genbki.sh script reads this file and generates .bki
 *	  information from the DATA() statements.
 *
 *-------------------------------------------------------------------------
 */
#ifndef _GP_CONFIGURATION_HISTORY_H_
#define _GP_CONFIGURATION_HISTORY_H_

#include "catalog/genbki.h"

/*
 * Defines for gp_configuration_history table
 * 
 * Used by fault-management components to record a "change history" description
 * with timestamp.
 */

#define GpConfigHistoryRelName		"gp_configuration_history"

/*
 * The CATALOG definition has to refer to the type of "time" as
 * "timestamptz" (lower case) so that bootstrap mode recognizes it.  But
 * the C header files define this type as TimestampTz.	Since the field is
 * potentially-null and therefore cannot be accessed directly from C code,
 * there is no particular need for the C struct definition to show the
 * field type as TimestampTz --- instead we just make it Datum.
 */

#define timestamptz Datum

/* ----------------
 *		gp_configuration_history definition.  cpp turns this into
 *		typedef struct FormData_gp_configuration_history
 * ----------------
 */
#define GpConfigHistoryRelationId	5006

CATALOG(gp_configuration_history,5006) BKI_SHARED_RELATION BKI_WITHOUT_OIDS
{
	timestamptz	time;	
	int2		dbid;	
	text		desc;	
} FormData_gp_configuration_history;

/* no foreign keys */

#undef timestamptz


/* ----------------
 *		Form_gp_configuration_history corresponds to a pointer to a tuple with
 *		the format of gp_configuration_history relation.
 * ----------------
 */
typedef FormData_gp_configuration_history *Form_gp_configuration_history;

/* ----------------
 *		compiler constants for gp_configuration_history
 * ----------------
 */
#define Natts_gp_configuration_history		3
#define Anum_gp_configuration_history_time	1
#define Anum_gp_configuration_history_dbid	2
#define Anum_gp_configuration_history_desc	3

#endif /*_GP_CONFIGURATION_HISTORY_H_*/
