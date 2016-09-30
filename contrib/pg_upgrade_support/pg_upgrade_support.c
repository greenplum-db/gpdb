/*
 *	pg_upgrade_sysoids.c
 *
 *	server-side functions to set backend global variables
 *	to control oid and relfilenode assignment
 *
 *	Copyright (c) 2010, PostgreSQL Global Development Group
 *	$PostgreSQL: pgsql/contrib/pg_upgrade_support/pg_upgrade_support.c,v 1.5 2010/07/06 19:18:55 momjian Exp $
 */

#include "postgres.h"

#include "fmgr.h"
#include "catalog/dependency.h"
#include "catalog/pg_class.h"

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

extern PGDLLIMPORT Oid binary_upgrade_next_pg_type_oid;
extern PGDLLIMPORT Oid binary_upgrade_next_array_pg_type_oid;
extern PGDLLIMPORT Oid binary_upgrade_next_toast_pg_type_oid;
extern PGDLLIMPORT Oid binary_upgrade_next_aosegments_pg_type_oid;
extern PGDLLIMPORT Oid binary_upgrade_next_aoblockdir_pg_type_oid;
extern PGDLLIMPORT Oid binary_upgrade_next_aovisimap_pg_type_oid;

extern PGDLLIMPORT Oid binary_upgrade_next_heap_pg_class_oid;
extern PGDLLIMPORT Oid binary_upgrade_next_index_pg_class_oid;
extern PGDLLIMPORT Oid binary_upgrade_next_toast_pg_class_oid;
extern PGDLLIMPORT Oid binary_upgrade_next_toast_index_pg_class_oid;
extern PGDLLIMPORT Oid binary_upgrade_next_aosegments_pg_class_oid;
extern PGDLLIMPORT Oid binary_upgrade_next_aosegments_index_pg_class_oid;
extern PGDLLIMPORT Oid binary_upgrade_next_aoblockdir_pg_class_oid;
extern PGDLLIMPORT Oid binary_upgrade_next_aoblockdir_index_pg_class_oid;
extern PGDLLIMPORT Oid binary_upgrade_next_aovisimap_pg_class_oid;
extern PGDLLIMPORT Oid binary_upgrade_next_aovisimap_index_pg_class_oid;

extern PGDLLIMPORT Oid binary_upgrade_next_pg_authid_oid;

Datum		set_next_pg_type_oid(PG_FUNCTION_ARGS);
Datum		set_next_array_pg_type_oid(PG_FUNCTION_ARGS);
Datum		set_next_toast_pg_type_oid(PG_FUNCTION_ARGS);
Datum		set_next_aosegments_pg_type_oid(PG_FUNCTION_ARGS);
Datum		set_next_aoblockdir_pg_type_oid(PG_FUNCTION_ARGS);
Datum		set_next_aovisimap_pg_type_oid(PG_FUNCTION_ARGS);

Datum		set_next_heap_pg_class_oid(PG_FUNCTION_ARGS);
Datum		set_next_index_pg_class_oid(PG_FUNCTION_ARGS);
Datum		set_next_toast_pg_class_oid(PG_FUNCTION_ARGS);
Datum		set_next_toast_index_pg_class_oid(PG_FUNCTION_ARGS);
Datum		set_next_aosegments_pg_class_oid(PG_FUNCTION_ARGS);
Datum		set_next_aosegments_index_pg_class_oid(PG_FUNCTION_ARGS);
Datum		set_next_aoblockdir_pg_class_oid(PG_FUNCTION_ARGS);
Datum		set_next_aoblockdir_index_pg_class_oid(PG_FUNCTION_ARGS);
Datum		set_next_aovisimap_pg_class_oid(PG_FUNCTION_ARGS);
Datum		set_next_aovisimap_index_pg_class_oid(PG_FUNCTION_ARGS);

Datum		set_next_pg_authid_oid(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(set_next_pg_type_oid);
PG_FUNCTION_INFO_V1(set_next_array_pg_type_oid);
PG_FUNCTION_INFO_V1(set_next_toast_pg_type_oid);
PG_FUNCTION_INFO_V1(set_next_aosegments_pg_type_oid);
PG_FUNCTION_INFO_V1(set_next_aoblockdir_pg_type_oid);
PG_FUNCTION_INFO_V1(set_next_aovisimap_pg_type_oid);

PG_FUNCTION_INFO_V1(set_next_heap_pg_class_oid);
PG_FUNCTION_INFO_V1(set_next_index_pg_class_oid);
PG_FUNCTION_INFO_V1(set_next_toast_pg_class_oid);
PG_FUNCTION_INFO_V1(set_next_toast_index_pg_class_oid);
PG_FUNCTION_INFO_V1(set_next_aosegments_pg_class_oid);
PG_FUNCTION_INFO_V1(set_next_aosegments_index_pg_class_oid);
PG_FUNCTION_INFO_V1(set_next_aoblockdir_pg_class_oid);
PG_FUNCTION_INFO_V1(set_next_aoblockdir_index_pg_class_oid);
PG_FUNCTION_INFO_V1(set_next_aovisimap_pg_class_oid);
PG_FUNCTION_INFO_V1(set_next_aovisimap_index_pg_class_oid);

PG_FUNCTION_INFO_V1(set_next_pg_authid_oid);

Datum
set_next_pg_type_oid(PG_FUNCTION_ARGS)
{
	Oid			typoid = PG_GETARG_OID(0);

	binary_upgrade_next_pg_type_oid = typoid;

	PG_RETURN_VOID();
}

Datum
set_next_array_pg_type_oid(PG_FUNCTION_ARGS)
{
	Oid			typoid = PG_GETARG_OID(0);

	binary_upgrade_next_array_pg_type_oid = typoid;

	PG_RETURN_VOID();
}

Datum
set_next_toast_pg_type_oid(PG_FUNCTION_ARGS)
{
	Oid			typoid = PG_GETARG_OID(0);

	binary_upgrade_next_toast_pg_type_oid = typoid;

	PG_RETURN_VOID();
}

Datum
set_next_aosegments_pg_type_oid(PG_FUNCTION_ARGS)
{
	Oid			typoid = PG_GETARG_OID(0);

	binary_upgrade_next_aosegments_pg_type_oid = typoid;

	PG_RETURN_VOID();
}

Datum
set_next_aoblockdir_pg_type_oid(PG_FUNCTION_ARGS)
{
	Oid			typoid = PG_GETARG_OID(0);

	binary_upgrade_next_aoblockdir_pg_type_oid = typoid;

	PG_RETURN_VOID();
}

Datum
set_next_aovisimap_pg_type_oid(PG_FUNCTION_ARGS)
{
	Oid			typoid = PG_GETARG_OID(0);

	binary_upgrade_next_aovisimap_pg_type_oid = typoid;

	PG_RETURN_VOID();
}


Datum
set_next_heap_pg_class_oid(PG_FUNCTION_ARGS)
{
	Oid			reloid = PG_GETARG_OID(0);

	binary_upgrade_next_heap_pg_class_oid = reloid;

	PG_RETURN_VOID();
}

Datum
set_next_index_pg_class_oid(PG_FUNCTION_ARGS)
{
	Oid			reloid = PG_GETARG_OID(0);

	binary_upgrade_next_index_pg_class_oid = reloid;

	PG_RETURN_VOID();
}

Datum
set_next_toast_pg_class_oid(PG_FUNCTION_ARGS)
{
	Oid			reloid = PG_GETARG_OID(0);

	binary_upgrade_next_toast_pg_class_oid = reloid;

	PG_RETURN_VOID();
}

/*
 * In PostgreSQL, we use set_next_index_pg_class_oid() to set the
 * index OID for a toast index. In GPDB, we couldn't do that for
 * aosegments, aoblockidr and aovisimap, because you could have all
 * of those in addition to the toast table. For clarity, we have
 * a separate function for the toast index in GPDB as well.
 */
Datum
set_next_toast_index_pg_class_oid(PG_FUNCTION_ARGS)
{
	Oid			reloid = PG_GETARG_OID(0);

	binary_upgrade_next_toast_pg_class_oid = reloid;

	PG_RETURN_VOID();
}

Datum
set_next_aosegments_pg_class_oid(PG_FUNCTION_ARGS)
{
	Oid			reloid = PG_GETARG_OID(0);

	binary_upgrade_next_aosegments_pg_class_oid = reloid;

	PG_RETURN_VOID();
}

Datum
set_next_aosegments_index_pg_class_oid(PG_FUNCTION_ARGS)
{
	Oid			reloid = PG_GETARG_OID(0);

	binary_upgrade_next_aosegments_index_pg_class_oid = reloid;

	PG_RETURN_VOID();
}

Datum
set_next_aoblockdir_pg_class_oid(PG_FUNCTION_ARGS)
{
	Oid			reloid = PG_GETARG_OID(0);

	binary_upgrade_next_aoblockdir_pg_class_oid = reloid;

	PG_RETURN_VOID();
}

Datum
set_next_aoblockdir_index_pg_class_oid(PG_FUNCTION_ARGS)
{
	Oid			reloid = PG_GETARG_OID(0);

	binary_upgrade_next_aoblockdir_index_pg_class_oid = reloid;

	PG_RETURN_VOID();
}

Datum
set_next_aovisimap_pg_class_oid(PG_FUNCTION_ARGS)
{
	Oid			reloid = PG_GETARG_OID(0);

	binary_upgrade_next_aovisimap_pg_class_oid = reloid;

	PG_RETURN_VOID();
}

Datum
set_next_aovisimap_index_pg_class_oid(PG_FUNCTION_ARGS)
{
	Oid			reloid = PG_GETARG_OID(0);

	binary_upgrade_next_aovisimap_index_pg_class_oid = reloid;

	PG_RETURN_VOID();
}

Datum
set_next_pg_authid_oid(PG_FUNCTION_ARGS)
{
	Oid			authoid = PG_GETARG_OID(0);

	binary_upgrade_next_pg_authid_oid = authoid;
	PG_RETURN_VOID();
}
