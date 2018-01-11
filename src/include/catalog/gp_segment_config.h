/*-------------------------------------------------------------------------
 *
 * gp_segment_config.h
 *    a segment configuration table
 *
 * Portions Copyright (c) 2006-2011, Greenplum Inc.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/catalog/gp_segment_config.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef _GP_SEGMENT_CONFIG_H_
#define _GP_SEGMENT_CONFIG_H_

#include "catalog/genbki.h"

/*
 * Defines for gp_segment_config table
 */
#define GpSegmentConfigRelationName		"gp_segment_configuration"

#define MASTER_DBID 1
#define MASTER_CONTENT_ID (-1)
#define InvalidDbid 0

#define GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY 'p'
#define GP_SEGMENT_CONFIGURATION_ROLE_MIRROR 'm'

#define GP_SEGMENT_CONFIGURATION_STATUS_UP 'u'
#define GP_SEGMENT_CONFIGURATION_STATUS_DOWN 'd'

#define GP_SEGMENT_CONFIGURATION_MODE_INSYNC 's'
#define GP_SEGMENT_CONFIGURATION_MODE_NOTINSYNC 'n'

#define GP_SEGMENT_CONFIGURATION_MODE_CHANGETRACKING 'c'
#define GP_SEGMENT_CONFIGURATION_MODE_RESYNC 'r'


/* ----------------
 *		gp_segment_configuration definition.  cpp turns this into
 *		typedef struct FormData_gp_segment_configuration
 * ----------------
 */
#define GpSegmentConfigRelationId	5036

CATALOG(gp_segment_configuration,5036) BKI_SHARED_RELATION BKI_WITHOUT_OIDS
{
	int2		dbid;				/* up to 32767 segment databases */
	int2		content;			/* up to 32767 contents -- only 16384 usable with mirroring (see dbid) */

	char		role;				
	char		preferred_role;		
	char		mode;				
	char		status;				
	int4		port;				

	text		hostname;			
	text		address;			

	int4		replication_port;	
} FormData_gp_segment_configuration;

/* no foreign keys */

/* ----------------
 *		Form_gp_segment_configuration corresponds to a pointer to a tuple with
 *		the format of gp_segment_configuration relation.
 * ----------------
 */
typedef FormData_gp_segment_configuration *Form_gp_segment_configuration;


/* ----------------
 *		compiler constants for gp_segment_configuration
 * ----------------
 */
#define Natts_gp_segment_configuration					10
#define Anum_gp_segment_configuration_dbid				1
#define Anum_gp_segment_configuration_content			2
#define Anum_gp_segment_configuration_role				3
#define Anum_gp_segment_configuration_preferred_role	4
#define Anum_gp_segment_configuration_mode				5
#define Anum_gp_segment_configuration_status			6
#define Anum_gp_segment_configuration_port				7
#define Anum_gp_segment_configuration_hostname			8
#define Anum_gp_segment_configuration_address			9
#define Anum_gp_segment_configuration_replication_port	10

extern bool gp_segment_config_has_mirrors(void);

#endif /*_GP_SEGMENT_CONFIG_H_*/
