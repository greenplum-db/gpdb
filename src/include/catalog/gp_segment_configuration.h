/*-------------------------------------------------------------------------
 *
 * gp_segment_configuration.h
 *    a segment configuration table
 *
 * Portions Copyright (c) 2006-2011, Greenplum Inc.
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/include/catalog/gp_segment_configuration.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef GP_SEGMENT_CONFIGURATION_H
#define GP_SEGMENT_CONFIGURATION_H

#include "catalog/genbki.h"
#include "catalog/gp_segment_configuration_d.h"

/*
 * Defines for gp_segment_configuration table
 */
#define GpSegmentConfigRelationName		"gp_segment_configuration"

#define MASTER_CONTENT_ID (-1)

#define GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY 'p'
#define GP_SEGMENT_CONFIGURATION_ROLE_MIRROR 'm'

#define GP_SEGMENT_CONFIGURATION_STATUS_UP 'u'
#define GP_SEGMENT_CONFIGURATION_STATUS_DOWN 'd'

#define GP_SEGMENT_CONFIGURATION_MODE_INSYNC 's'
#define GP_SEGMENT_CONFIGURATION_MODE_NOTINSYNC 'n'

/* ----------------
 *		gp_segment_configuration definition.  cpp turns this into
 *		typedef struct FormData_gp_segment_configuration
 * ----------------
 */
CATALOG(gp_segment_configuration,5036,GpSegmentConfigRelationId) BKI_SHARED_RELATION
{
	int16		dbid;				/* up to 32767 segment databases */
	int16		content;			/* up to 32767 contents -- only 16384 usable with mirroring (see dbid) */

	char		role;
	char		preferred_role;
	char		mode;
	char		status;
	int32		port;

#ifdef CATALOG_VARLEN
	text		hostname;
	text		address;

	text		datadir;
#endif
} FormData_gp_segment_configuration;

/* no foreign keys */

/* ----------------
 *		Form_gp_segment_configuration corresponds to a pointer to a tuple with
 *		the format of gp_segment_configuration relation.
 * ----------------
 */
typedef FormData_gp_segment_configuration *Form_gp_segment_configuration;

extern bool gp_segment_config_has_mirrors(void);

#endif /*_GP_SEGMENT_CONFIGURATION_H_*/
