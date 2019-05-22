/*
 * pxf_option.c
 *		  Foreign-data wrapper option handling for PXF (Platform Extension Framework)
 *
 * IDENTIFICATION
 *		  contrib/pxf_fdw/pxf_option.c
 */

#include "postgres.h"

#include "access/reloptions.h"
#include "catalog/pg_foreign_data_wrapper.h"
#include "catalog/pg_foreign_server.h"
#include "catalog/pg_user_mapping.h"
#include "commands/copy.h"
#include "commands/defrem.h"

static char *const FDW_OPTION_PROTOCOL = "protocol";

extern Datum
pxf_fdw_validator(PG_FUNCTION_ARGS);

/*
 * SQL functions
 */
PG_FUNCTION_INFO_V1(pxf_fdw_validator);

/*
 * Validate the generic options given to a FOREIGN DATA WRAPPER, SERVER,
 * USER MAPPING or FOREIGN TABLE that uses file_fdw.
 *
 * Raise an ERROR if the option or its value is considered invalid.
 *
 */
Datum
pxf_fdw_validator(PG_FUNCTION_ARGS)
{
	char     *protocol      = NULL;
	List     *options_list  = untransformRelOptions(PG_GETARG_DATUM(0));
	Oid      catalog        = PG_GETARG_OID(1);
	List     *other_options = NIL;
	ListCell *cell;

	foreach(cell, options_list)
	{
		DefElem *def = (DefElem *) lfirst(cell);

		/*
		 * Separate out protocol and column-specific options
		 */
		if (strcmp(def->defname, FDW_OPTION_PROTOCOL) == 0)
		{
			protocol = defGetString(def);

			if (catalog == ForeignServerRelationId)
				ereport(ERROR,
				        (errcode(ERRCODE_FDW_INVALID_OPTION_NAME),
					        errmsg(
						        "the protocol option cannot be defined at the server level")));
			if (catalog == UserMappingRelationId)
				ereport(ERROR,
				        (errcode(ERRCODE_FDW_INVALID_OPTION_NAME),
					        errmsg(
						        "the protocol option cannot be defined at the user-mapping level")));

		}
		else
			other_options = lappend(other_options, def);
	}

	if (catalog == ForeignDataWrapperRelationId &&
		(protocol == NULL || strcmp(protocol, "") == 0))
	{
		ereport(ERROR,
		        (errcode(ERRCODE_FDW_DYNAMIC_PARAMETER_VALUE_NEEDED),
			        errmsg(
				        "the protocol option is required for PXF foreign-data wrappers")));
	}

	/*
	 * Apply the core COPY code's validation logic for more checks.
	 */
	ProcessCopyOptions(NULL, true, other_options, 0, true);

	PG_RETURN_VOID();
}