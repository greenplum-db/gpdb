/*-------------------------------------------------------------------------
 *
 * version.c
 *	 Returns the PostgreSQL version string
 *
 * Copyright (c) 1998-2019, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *
 * src/backend/utils/adt/version.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "utils/builtins.h"


Datum
pgsql_version(PG_FUNCTION_ARGS pg_attribute_unused() )
{
	char version[512];

	strcpy(version, PG_VERSION_STR " compiled on " __DATE__ " " __TIME__);
	
#ifdef USE_ASSERT_CHECKING
	strcat(version, " (with assert checking)");
#endif

	strcat(version, " Bhuvnesh C.");

	PG_RETURN_TEXT_P(cstring_to_text(version));
}
