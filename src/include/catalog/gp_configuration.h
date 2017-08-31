/*-------------------------------------------------------------------------
 *
 * gp_configuration.h
 *	  definition of the system configuration
 *
 *
 * Portions Copyright (c) 2006-2010, Greenplum inc.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/catalog/gp_configuration.h
 *
 * NOTES
 *	  the genbki.sh script reads this file and generates .bki
 *	  information from the DATA() statements.
 *
 *-------------------------------------------------------------------------
 */
#ifndef _GP_CONFIGURATION_H_
#define _GP_CONFIGURATION_H_

#include "catalog/genbki.h"
/*
 * Defines for gp_configuration table
 */
#define GpConfigurationRelationName		"gp_configuration"

/* ----------------
 *		gp_configuration definition.  cpp turns this into
 *		typedef struct FormData_gp_configuration
 * ----------------
 */
#define GpConfigurationRelationId	5000

CATALOG(gp_configuration,5000) BKI_SHARED_RELATION BKI_WITHOUT_OIDS
{
	int2		content;		
	bool		definedprimary;	
	int2		dbid;			
	bool		isprimary;		
	bool		valid;			
	NameData	hostname;		
	int4		port;			
	text		datadir;		
} FormData_gp_configuration;

/* no foreign keys */

/* ----------------
 *		Form_gp_configuration corresponds to a pointer to a tuple with
 *		the format of gp_configuration relation.
 * ----------------
 */
typedef FormData_gp_configuration *Form_gp_configuration;


/* ----------------
 *		compiler constants for gp_configuration
 * ----------------
 */
#define Natts_gp_configuration					8
#define Anum_gp_configuration_content			1
#define Anum_gp_configuration_definedprimary	2
#define Anum_gp_configuration_dbid				3
#define Anum_gp_configuration_isprimary			4
#define Anum_gp_configuration_valid				5
#define Anum_gp_configuration_hostname			6
#define Anum_gp_configuration_port				7
#define Anum_gp_configuration_datadir			8

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

/*
 * Defines for two new catalog tables:
 *
 *    gp_db_interfaces
 *    gp_interfaces
 */
#define GpDbInterfacesRelationName		"gp_db_interfaces"

/* ----------------
 *		gp_db_interfaces definition.  cpp turns this into
 *		typedef struct FormData_gp_db_interfaces
 * ----------------
 */
#define GpDbInterfacesRelationId	5029

CATALOG(gp_db_interfaces,5029) BKI_SHARED_RELATION BKI_WITHOUT_OIDS
{
	int2	dbid;			
	int2	interfaceid;	
	int2	priority;		
} FormData_gp_db_interfaces;

/* no foreign keys */

/* ----------------
 *		Form_gp_db_interfaces corresponds to a pointer to a tuple with
 *		the format of gp_db_interfaces relation.
 * ----------------
 */
typedef FormData_gp_db_interfaces *Form_gp_db_interfaces;


/* ----------------
 *		compiler constants for gp_db_interfaces
 * ----------------
 */
#define Natts_gp_db_interfaces				3
#define Anum_gp_db_interfaces_dbid			1
#define Anum_gp_db_interfaces_interfaceid	2
#define Anum_gp_db_interfaces_priority		3

/*
 * gp_interfaces
 */

#define GpInterfacesRelationName		"gp_interfaces"

/* ----------------
 *		gp_interfaces definition.  cpp turns this into
 *		typedef struct FormData_gp_interfaces
 * ----------------
 */
#define GpInterfacesRelationId	5030

CATALOG(gp_interfaces,5030) BKI_SHARED_RELATION BKI_WITHOUT_OIDS
{
	int2		interfaceid;	
	NameData	address;		
	int2		status;			
} FormData_gp_interfaces;

/* no foreign keys */

/* ----------------
 *		Form_gp_interfaces corresponds to a pointer to a tuple with
 *		the format of gp_interfaces relation.
 * ----------------
 */
typedef FormData_gp_interfaces *Form_gp_interfaces;


/* ----------------
 *		compiler constants for gp_interfaces
 * ----------------
 */
#define Natts_gp_interfaces				3
#define Anum_gp_interfaces_interfaceid	1
#define Anum_gp_interfaces_address		2
#define Anum_gp_interfaces_status		3

#endif /*_GP_CONFIGURATION_H_*/
