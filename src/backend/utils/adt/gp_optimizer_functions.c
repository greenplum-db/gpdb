/*
 * gp_optimizer_functions.c
 *    Defines builtin transformation functions for the optimizer.
 *
 * enable_xform: This function wraps EnableXform.
 *
 * disable_xform: This function wraps DisableXform.
 *
 * gp_opt_version: This function wraps LibraryVersion. 
 *
 * Copyright(c) 2012 - present, EMC/Greenplum
 */

#include "postgres.h"

#include "funcapi.h"
#include "utils/builtins.h"

extern Datum EnableXform(PG_FUNCTION_ARGS);

/*
* Enables transformations in the optimizer.
*/
Datum
enable_xform(PG_FUNCTION_ARGS)
{
#ifdef USE_ORCA
	return EnableXform(fcinfo);
#else
	return CStringGetTextDatum("Server has been compiled without ORCA");
#endif
}

extern Datum DisableXform(PG_FUNCTION_ARGS);

/* 
* Disables transformations in the optimizer.
*/
Datum
disable_xform(PG_FUNCTION_ARGS)
{
#ifdef USE_ORCA
	return DisableXform(fcinfo);
#else
	return CStringGetTextDatum("Server has been compiled without ORCA");
#endif
}

extern Datum LibraryVersion();
	
/*
* Returns the optimizer and gpos library versions.
*/
Datum
gp_opt_version(PG_FUNCTION_ARGS pg_attribute_unused())
{
#ifdef USE_ORCA
	return LibraryVersion();
#else
	return CStringGetTextDatum("Server has been compiled without ORCA");
#endif
}
