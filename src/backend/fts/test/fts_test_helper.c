#include "postgres.h"

#include "catalog/gp_segment_config.h"
#include "cdb/cdbutil.h"

/*
 * Function to help create representation of gp_segment_configuration, for
 * ease of testing different scenarios. Using the same can mock out different
 * configuration layouts.
 *
 * --------------------------------
 * Inputs:
 *    segCnt - number of primary segments the configuration should mock
 *    has_mirrors - controls if mirrors corresponding to primary are created
 * --------------------------------
 *
 * Function always adds master to the configuration. Also, all the segments
 * are by default marked up. Tests leverage to create initial configuration
 * using this and then modify the same as per needs to mock different
 * scenarios like mirror down, primary down, etc...
 */
CdbComponentDatabases *
InitTestCdb(int segCnt, bool has_mirrors, char default_mode)
{
	int			i = 0;
	int			mirror_multiplier = 1;

	if (has_mirrors)
		mirror_multiplier = 2;

	CdbComponentDatabases *cdb =
	(CdbComponentDatabases *) palloc(sizeof(CdbComponentDatabases));

	cdb->total_entry_dbs = 1;
	cdb->total_segment_dbs = segCnt * mirror_multiplier;	/* with mirror? */
	cdb->total_segments = segCnt;
	cdb->my_dbid = 1;
	cdb->my_segindex = -1;
	cdb->my_isprimary = true;
	cdb->entry_db_info = palloc(
								sizeof(CdbComponentDatabaseInfo) * cdb->total_entry_dbs);
	cdb->segment_db_info = palloc(
								  sizeof(CdbComponentDatabaseInfo) * cdb->total_segment_dbs);


	/* create the master entry_db_info */
	CdbComponentDatabaseInfo *cdbinfo = &cdb->entry_db_info[0];

	cdbinfo->dbid = 1;
	cdbinfo->segindex = '-1';

	cdbinfo->role = GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY;
	cdbinfo->preferred_role = GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY;
	cdbinfo->status = GP_SEGMENT_CONFIGURATION_STATUS_UP;

	/* create the segment_db_info entries */
	for (i = 0; i < cdb->total_segment_dbs; i++)
	{
		CdbComponentDatabaseInfo *cdbinfo = &cdb->segment_db_info[i];

		cdbinfo->dbid = i + 2;
		cdbinfo->segindex = i / 2;
		cdbinfo->hostip = palloc(4);
		snprintf(cdbinfo->hostip, 4, "%d", cdbinfo->dbid);
		cdbinfo->port = cdbinfo->dbid;

		if (has_mirrors)
		{
			cdbinfo->role = i % 2 ?
				GP_SEGMENT_CONFIGURATION_ROLE_MIRROR :
				GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY;

			cdbinfo->preferred_role = i % 2 ?
				GP_SEGMENT_CONFIGURATION_ROLE_MIRROR :
				GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY;
		}
		else
		{
			cdbinfo->role = GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY;
			cdbinfo->preferred_role = GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY;
		}
		cdbinfo->status = GP_SEGMENT_CONFIGURATION_STATUS_UP;
		cdbinfo->mode = default_mode;
	}

	return cdb;
}
