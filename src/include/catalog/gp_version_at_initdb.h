#ifndef GP_VERSION_AT_INITDB_H
#define GP_VERSION_AT_INITDB_H

#include "catalog/genbki.h"
#include "catalog/gp_version_at_initdb_d.h"

/*
 * Defines for gp_version_at_initdb table
 */
CATALOG(gp_version_at_initdb,5103,GpVersionRelationId) BKI_SHARED_RELATION
{
	int16		schemaversion;
	text		productversion;
} FormData_gp_version_at_initdb;

/* no foreign key */

#endif			/* GP_VERSION_AT_INITDB_H */
