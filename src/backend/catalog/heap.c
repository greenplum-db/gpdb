/*-------------------------------------------------------------------------
 *
 * heap.c
 *	  code to create and destroy POSTGRES heap relations
 *
 * Portions Copyright (c) 2005-2010, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/catalog/heap.c,v 1.327.2.1 2009/02/24 01:38:49 tgl Exp $
 *
 *
 * INTERFACE ROUTINES
 *		heap_create()			- Create an uncataloged heap relation
 *		heap_create_with_catalog() - Create a cataloged relation
 *		heap_drop_with_catalog() - Removes named relation from catalogs
 *
 * NOTES
 *	  this code taken from access/heap/create.c, which contains
 *	  the old heap_create_with_catalog, amcreate, and amdestroy.
 *	  those routines will soon call these routines using the function
 *	  manager,
 *	  just like the poorly named "NewXXX" routines do.	The
 *	  "New" routines are all going to die soon, once and for all!
 *		-cim 1/13/91
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/genam.h"
#include "access/heapam.h"
#include "access/transam.h"
#include "access/reloptions.h"
#include "access/xact.h"
#include "catalog/catalog.h"
#include "catalog/dependency.h"
#include "catalog/gp_policy.h"
#include "catalog/heap.h"
#include "catalog/index.h"
#include "catalog/indexing.h"
#include "catalog/namespace.h"
#include "catalog/pg_appendonly_fn.h"
#include "catalog/pg_attrdef.h"
#include "catalog/pg_attribute_encoding.h"
#include "catalog/pg_authid.h"
#include "catalog/pg_auth_members.h"
#include "catalog/pg_constraint.h"
#include "catalog/pg_database.h"
#include "catalog/pg_exttable.h"
#include "catalog/pg_inherits.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_partition.h"
#include "catalog/pg_partition_rule.h"
#include "catalog/pg_statistic.h"
#include "catalog/pg_tablespace.h"
#include "catalog/pg_type.h"
#include "cdb/cdbpartition.h"
#include "cdb/cdbsreh.h"
#include "commands/tablecmds.h"
#include "commands/typecmds.h"
#include "miscadmin.h"
#include "optimizer/clauses.h"
#include "optimizer/var.h"
#include "parser/parse_coerce.h"
#include "parser/parse_expr.h"
#include "parser/parse_relation.h"
#include "storage/smgr.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/inval.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"             /* CDB: GetMemoryChunkContext */
#include "utils/relcache.h"
#include "utils/syscache.h"

#include "cdb/cdbvars.h"

#include "cdb/cdbmirroredfilesysobj.h"
#include "cdb/cdbpersistentfilesysobj.h"
#include "catalog/gp_persistent.h"

#include "utils/guc.h"

static void MetaTrackAddUpdInternal(Oid			classid,
									Oid			objoid,
									Oid			relowner,
									char*		actionname,
									char*		subtype,
									Relation	rel,
									HeapTuple	old_tuple);



static void AddNewRelationTuple(Relation pg_class_desc,
					Relation new_rel_desc,
					Oid new_rel_oid, Oid new_type_oid,
					Oid relowner,
					char relkind,
					char relstorage,
					Datum reloptions);
static Oid AddNewRelationType(const char *typeName,
				   Oid typeNamespace,
				   Oid new_rel_oid,
				   char new_rel_kind,
				   Oid ownerid,
				   Oid new_array_type);
static void RelationRemoveInheritance(Oid relid);
static void StoreRelCheck(Relation rel, char *ccname, char *ccbin);
static Node* cookConstraint (ParseState *pstate,
							 Node 		*raw_constraint,
							 char		*relname);
static List *insert_ordered_unique_oid(List *list, Oid datum);

/* ----------------------------------------------------------------
 *				XXX UGLY HARD CODED BADNESS FOLLOWS XXX
 *
 *		these should all be moved to someplace in the lib/catalog
 *		module, if not obliterated first.
 * ----------------------------------------------------------------
 */


/*
 * Note:
 *		Should the system special case these attributes in the future?
 *		Advantage:	consume much less space in the ATTRIBUTE relation.
 *		Disadvantage:  special cases will be all over the place.
 */

static FormData_pg_attribute a1 = {
	0, {"ctid"}, TIDOID, 0, sizeof(ItemPointerData),
	SelfItemPointerAttributeNumber, 0, -1, -1,
	false, 'p', 's', true, false, false, true, 0
};

static FormData_pg_attribute a2 = {
	0, {"oid"}, OIDOID, 0, sizeof(Oid),
	ObjectIdAttributeNumber, 0, -1, -1,
	true, 'p', 'i', true, false, false, true, 0
};

static FormData_pg_attribute a3 = {
	0, {"xmin"}, XIDOID, 0, sizeof(TransactionId),
	MinTransactionIdAttributeNumber, 0, -1, -1,
	true, 'p', 'i', true, false, false, true, 0
};

static FormData_pg_attribute a4 = {
	0, {"cmin"}, CIDOID, 0, sizeof(CommandId),
	MinCommandIdAttributeNumber, 0, -1, -1,
	true, 'p', 'i', true, false, false, true, 0
};

static FormData_pg_attribute a5 = {
	0, {"xmax"}, XIDOID, 0, sizeof(TransactionId),
	MaxTransactionIdAttributeNumber, 0, -1, -1,
	true, 'p', 'i', true, false, false, true, 0
};

static FormData_pg_attribute a6 = {
	0, {"cmax"}, CIDOID, 0, sizeof(CommandId),
	MaxCommandIdAttributeNumber, 0, -1, -1,
	true, 'p', 'i', true, false, false, true, 0
};

/*
 * We decided to call this attribute "tableoid" rather than say
 * "classoid" on the basis that in the future there may be more than one
 * table of a particular class/type. In any case table is still the word
 * used in SQL.
 */
static FormData_pg_attribute a7 = {
	0, {"tableoid"}, OIDOID, 0, sizeof(Oid),
	TableOidAttributeNumber, 0, -1, -1,
	true, 'p', 'i', true, false, false, true, 0
};

/*CDB*/
static FormData_pg_attribute a8 = {
	0, {"gp_segment_id"}, INT4OID, 0, sizeof(gpsegmentId),
	GpSegmentIdAttributeNumber, 0, -1, -1,
	true, 'p', 'i', true, false, false, true, 0
};

static const Form_pg_attribute SysAtt[] = {&a1, &a2, &a3, &a4, &a5, &a6, &a7, &a8};

/*
 * This function returns a Form_pg_attribute pointer for a system attribute.
 * Note that we elog if the presented attno is invalid, which would only
 * happen if there's a problem upstream.
 */
Form_pg_attribute
SystemAttributeDefinition(AttrNumber attno, bool relhasoids)
{
	if (attno >= 0 || attno < -(int) lengthof(SysAtt))
		elog(ERROR, "invalid system attribute number %d", attno);
	if (attno == ObjectIdAttributeNumber && !relhasoids)
		elog(ERROR, "invalid system attribute number %d", attno);
	return SysAtt[-attno - 1];
}

/*
 * If the given name is a system attribute name, return a Form_pg_attribute
 * pointer for a prototype definition.	If not, return NULL.
 */
Form_pg_attribute
SystemAttributeByName(const char *attname, bool relhasoids)
{
	int			j;

	for (j = 0; j < (int) lengthof(SysAtt); j++)
	{
		Form_pg_attribute att = SysAtt[j];

		if (relhasoids || att->attnum != ObjectIdAttributeNumber)
		{
			if (strcmp(NameStr(att->attname), attname) == 0)
				return att;
		}
	}

	return NULL;
}


/* ----------------------------------------------------------------
 *				XXX END OF UGLY HARD CODED BADNESS XXX
 * ---------------------------------------------------------------- */


/* ----------------------------------------------------------------
 *		heap_create		- Create an uncataloged heap relation
 *
 *		Note API change: the caller must now always provide the OID
 *		to use for the relation.
 *
 *		rel->rd_rel is initialized by RelationBuildLocalRelation,
 *		and is mostly zeroes at return.
 * ----------------------------------------------------------------
 */
Relation
heap_create(const char *relname,
			Oid relnamespace,
			Oid reltablespace,
			Oid relid,
			TupleDesc tupDesc,
			Oid relam,
			char relkind,
			char relstorage,
			bool shared_relation,
			bool allow_system_table_mods,
			bool bufferPoolBulkLoad)
{
	bool		create_storage;
	Relation	rel;

	/* The caller must have provided an OID for the relation. */
	Assert(OidIsValid(relid));

	/*
	 * sanity checks
	 */
	if (!allow_system_table_mods &&
		(IsSystemNamespace(relnamespace) || IsToastNamespace(relnamespace) ||
		 IsAoSegmentNamespace(relnamespace)) && IsNormalProcessingMode())
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 errmsg("permission denied to create \"%s.%s\"",
						get_namespace_name(relnamespace), relname),
		errdetail("System catalog modifications are currently disallowed.")));

	/*
	 * Decide if we need storage or not, and handle a couple other special
	 * cases for particular relkinds.
	 */
	switch (relkind)
	{
		case RELKIND_VIEW:
		case RELKIND_COMPOSITE_TYPE:
			create_storage = false;

			/*
			 * Force reltablespace to zero if the relation has no physical
			 * storage.  This is mainly just for cleanliness' sake.
			 */
			reltablespace = InvalidOid;
			break;
		case RELKIND_SEQUENCE:
			create_storage = true;

			/*
			 * Force reltablespace to zero for sequences, since we don't
			 * support moving them around into different tablespaces.
			 */
			reltablespace = InvalidOid;
			break;
		default:
			if(relstorage_is_external(relstorage))
				create_storage = false;
			else
				create_storage = true;
			break;
	}

	/*
	 * Never allow a pg_class entry to explicitly specify the database's
	 * default tablespace in reltablespace; force it to zero instead. This
	 * ensures that if the database is cloned with a different default
	 * tablespace, the pg_class entry will still match where CREATE DATABASE
	 * will put the physically copied relation.
	 *
	 * Yes, this is a bit of a hack.
	 */
	if (reltablespace == MyDatabaseTableSpace)
		reltablespace = InvalidOid;

	/*
	 * build the relcache entry.
	 */
	rel = RelationBuildLocalRelation(relname,
									 relnamespace,
									 tupDesc,
									 relid,
									 reltablespace,
                                     relkind,           /*CDB*/
									 shared_relation);

	/*
	 * have the storage manager create the relation's disk file, if needed.
	 */
	if (create_storage)
	{
		bool isAppendOnly;
		bool skipCreatingSharedTable = false;

		/*
		 * We save the persistent TID and serial number in pg_class so we
		 * can supply them to the Storage Manager if the object is subsequently
		 * dropped.
		 *
		 * For shared table (that we created during upgrade), we create it once in every
		 * database, but they will all point to the same file. So, the file might have already
		 * been created.
		 *
		 * Note that we have not tried creating shared AO table.
		 *
		 * For non-shared table, we should always need to create a file.
		 */
		// WARNING: Do not use the rel structure -- it doesn't have relstorage set...
		isAppendOnly = (relstorage == RELSTORAGE_AOROWS || relstorage == RELSTORAGE_AOCOLS);

		if (!skipCreatingSharedTable)
		{
			if (!isAppendOnly)
			{
				PersistentFileSysRelStorageMgr localRelStorageMgr;
				PersistentFileSysRelBufpoolKind relBufpoolKind;

				GpPersistentRelationNode_GetRelationInfo(
													relkind,
													relstorage,
													relam,
													&localRelStorageMgr,
													&relBufpoolKind);
				Assert(localRelStorageMgr == PersistentFileSysRelStorageMgr_BufferPool);

				Assert(rel->rd_smgr == NULL);
				RelationOpenSmgr(rel);

				MirroredFileSysObj_TransactionCreateBufferPoolFile(
													rel->rd_smgr,
													relBufpoolKind,
													rel->rd_isLocalBuf,
													rel->rd_rel->relname.data,
													/* doJustInTimeDirCreate */ true,
													bufferPoolBulkLoad,
													&rel->rd_segfile0_relationnodeinfo.persistentTid,
													&rel->rd_segfile0_relationnodeinfo.persistentSerialNum);
			}
			else
			{
				MirroredFileSysObj_TransactionCreateAppendOnlyFile(
													&rel->rd_node,
													/* segmentFileNum */ 0,
													rel->rd_rel->relname.data,
													/* doJustInTimeDirCreate */ true,
													&rel->rd_segfile0_relationnodeinfo.persistentTid,
													&rel->rd_segfile0_relationnodeinfo.persistentSerialNum);
			}
		}
		

		if (!Persistent_BeforePersistenceWork() &&
			PersistentStore_IsZeroTid(&rel->rd_segfile0_relationnodeinfo.persistentTid))
		{	
			elog(ERROR, 
				 "setNewRelfilenodeCommon has invalid TID (0,0) into relation %u/%u/%u '%s', serial number " INT64_FORMAT,
				 rel->rd_node.spcNode,
				 rel->rd_node.dbNode,
				 rel->rd_node.relNode,
				 NameStr(rel->rd_rel->relname),
				 rel->rd_segfile0_relationnodeinfo.persistentSerialNum);
		}

		rel->rd_segfile0_relationnodeinfo.isPresent = true;

		if (Debug_persistent_print)
			elog(Persistent_DebugPrintLevel(), 
			     "heap_create: '%s', Append-Only '%s', persistent TID %s and serial number " INT64_FORMAT " for CREATE",
				 relpath(rel->rd_node),
				 (isAppendOnly ? "true" : "false"),
				 ItemPointerToString(&rel->rd_segfile0_relationnodeinfo.persistentTid),
				 rel->rd_segfile0_relationnodeinfo.persistentSerialNum);
	}

	return rel;
}

/* ----------------------------------------------------------------
 *		heap_create_with_catalog		- Create a cataloged relation
 *
 *		this is done in multiple steps:
 *
 *		1) CheckAttributeNamesTypes() is used to make certain the tuple
 *		   descriptor contains a valid set of attribute names and types
 *
 *		2) pg_class is opened and get_relname_relid()
 *		   performs a scan to ensure that no relation with the
 *		   same name already exists.
 *
 *		3) heap_create() is called to create the new relation on disk.
 *
 *		4) TypeCreate() is called to define a new type corresponding
 *		   to the new relation.
 *
 *		5) AddNewRelationTuple() is called to register the
 *		   relation in pg_class.
 *
 *		6) AddNewAttributeTuples() is called to register the
 *		   new relation's schema in pg_attribute.
 *
 *		7) StoreConstraints is called ()		- vadim 08/22/97
 *
 *		8) the relations are closed and the new relation's oid
 *		   is returned.
 *
 * ----------------------------------------------------------------
 */

/* --------------------------------
 *		CheckAttributeNamesTypes
 *
 *		this is used to make certain the tuple descriptor contains a
 *		valid set of attribute names and datatypes.  a problem simply
 *		generates ereport(ERROR) which aborts the current transaction.
 * --------------------------------
 */
void
CheckAttributeNamesTypes(TupleDesc tupdesc, char relkind)
{
	int			i;
	int			j;
	int			natts = tupdesc->natts;

	/* Sanity check on column count */
	if (natts < 0 || natts > MaxHeapAttributeNumber)
		ereport(ERROR,
				(errcode(ERRCODE_TOO_MANY_COLUMNS),
				 errmsg("tables can have at most %d columns",
						MaxHeapAttributeNumber)));

	/*
	 * first check for collision with system attribute names
	 *
	 * Skip this for a view or type relation, since those don't have system
	 * attributes.
	 */
	if (relkind != RELKIND_VIEW && relkind != RELKIND_COMPOSITE_TYPE)
	{
		for (i = 0; i < natts; i++)
		{
			if (SystemAttributeByName(NameStr(tupdesc->attrs[i]->attname),
									  tupdesc->tdhasoid) != NULL)
				ereport(ERROR,
						(errcode(ERRCODE_DUPLICATE_COLUMN),
						 errmsg("column name \"%s\" conflicts with a system column name",
								NameStr(tupdesc->attrs[i]->attname))));
		}
	}

	/*
	 * next check for repeated attribute names
	 */
	for (i = 1; i < natts; i++)
	{
		for (j = 0; j < i; j++)
		{
			if (strcmp(NameStr(tupdesc->attrs[j]->attname),
					   NameStr(tupdesc->attrs[i]->attname)) == 0)
				ereport(ERROR,
						(errcode(ERRCODE_DUPLICATE_COLUMN),
						 errmsg("column name \"%s\" specified more than once",
								NameStr(tupdesc->attrs[j]->attname))));
		}
	}

	/*
	 * next check the attribute types
	 */
	for (i = 0; i < natts; i++)
	{
		CheckAttributeType(NameStr(tupdesc->attrs[i]->attname),
						   tupdesc->attrs[i]->atttypid,
						   NIL /* assume we're creating a new rowtype */);
	}
}

/* --------------------------------
 *		CheckAttributeType
 *
 *		Verify that the proposed datatype of an attribute is legal.
 *		This is needed mainly because there are types (and pseudo-types)
 *		in the catalogs that we do not support as elements of real tuples.
 *		We also check some other properties required of a table column.
 *
 * If the attribute is being proposed for addition to an existing table or
 * composite type, pass a one-element list of the rowtype OID as
 * containing_rowtypes.  When checking a to-be-created rowtype, it's
 * sufficient to pass NIL, because there could not be any recursive reference
 * to a not-yet-existing rowtype.
 * --------------------------------
 */
void
CheckAttributeType(const char *attname, Oid atttypid,
				   List *containing_rowtypes)
{
	char		att_typtype = get_typtype(atttypid);
	Oid			att_typelem;

	if (Gp_role == GP_ROLE_EXECUTE)
	{
		/*
		 * In executor nodes, don't bother checking, as the dispatcher should've
		 * checked this already.
		 */
		return;
	}

	if (atttypid == UNKNOWNOID)
	{
		/*
		 * Warn user, but don't fail, if column to be created has UNKNOWN type
		 * (usually as a result of a 'retrieve into' - jolly)
		 */
		ereport(WARNING,
				(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
				 errmsg("column \"%s\" has type \"unknown\"", attname),
				 errdetail("Proceeding with relation creation anyway.")));
	}
	else if (att_typtype == TYPTYPE_PSEUDO)
	{
		/*
		 * Refuse any attempt to create a pseudo-type column, except for a
		 * special hack for pg_statistic: allow ANYARRAY during initdb
		 */
		if (atttypid != ANYARRAYOID || IsUnderPostmaster)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
					 errmsg("column \"%s\" has pseudo-type %s",
							attname, format_type_be(atttypid))));
	}
	else if (att_typtype == TYPTYPE_COMPOSITE)
	{
		/*
		 * For a composite type, recurse into its attributes.  You might think
		 * this isn't necessary, but since we allow system catalogs to break
		 * the rule, we have to guard against the case.
		 */
		Relation	relation;
		TupleDesc	tupdesc;
		int			i;

		/*
		 * Check for self-containment.  Eventually we might be able to allow
		 * this (just return without complaint, if so) but it's not clear how
		 * many other places would require anti-recursion defenses before it
		 * would be safe to allow tables to contain their own rowtype.
		 */
		if (list_member_oid(containing_rowtypes, atttypid))
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
					 errmsg("composite type %s cannot be made a member of itself",
							format_type_be(atttypid))));

		containing_rowtypes = lcons_oid(atttypid, containing_rowtypes);

		relation = relation_open(get_typ_typrelid(atttypid), AccessShareLock);

		tupdesc = RelationGetDescr(relation);

		for (i = 0; i < tupdesc->natts; i++)
		{
			Form_pg_attribute attr = tupdesc->attrs[i];

			if (attr->attisdropped)
				continue;
			CheckAttributeType(NameStr(attr->attname), attr->atttypid,
							   containing_rowtypes);
		}

		relation_close(relation, AccessShareLock);

		containing_rowtypes = list_delete_first(containing_rowtypes);
	}
	else if (OidIsValid((att_typelem = get_element_type(atttypid))))
	{
		/*
		 * Must recurse into array types, too, in case they are composite.
		 */
		CheckAttributeType(attname, att_typelem,
						   containing_rowtypes);
	}
}

/* MPP-6929: metadata tracking */
/* --------------------------------
 *		MetaTrackAddObject
 *
 *		Track creation of object in pg_stat_last_operation. The
 *		arguments are:
 *
 *		classid		- the oid of the table containing the object, eg
 *					  "pg_class" for a relation
 *		objoid		- the oid of the object itself in the specified table
 *		relowner	- role ? user ?
 *		actionname	- generally CREATE for this case
 *		subtype		- some generic descriptive, eg TABLE for a "CREATE TABLE"
 *
 *
 * --------------------------------
 */

static void MetaTrackAddUpdInternal(Oid			classid,
									Oid			objoid,
									Oid			relowner,
									char*		actionname,
									char*		subtype,
									Relation	rel,
									HeapTuple	old_tuple)
{
	HeapTuple	new_tuple;
	Datum		values[Natts_pg_statlastop];
	bool		isnull[Natts_pg_statlastop];
	bool		new_record_repl[Natts_pg_statlastop];
	NameData	uname;
	NameData	aname;
	HeapTuple	roletup;

	MemSet(isnull, 0, sizeof(bool) * Natts_pg_statlastop);
	MemSet(new_record_repl, 0, sizeof(bool) * Natts_pg_statlastop);

	values[Anum_pg_statlastop_classid - 1] = ObjectIdGetDatum(classid);
	values[Anum_pg_statlastop_objid - 1] = ObjectIdGetDatum(objoid);

	aname.data[0] = '\0';
	namestrcpy(&aname, actionname);
	values[Anum_pg_statlastop_staactionname - 1] = NameGetDatum(&aname);

	values[Anum_pg_statlastop_stasysid - 1] = ObjectIdGetDatum(relowner);
	/* set this column to update */
	new_record_repl[Anum_pg_statlastop_stasysid - 1] = true;

	uname.data[0] = '\0';

	roletup = SearchSysCache(AUTHOID,
							 ObjectIdGetDatum(relowner),
							 0, 0, 0);
	if (HeapTupleIsValid(roletup))
	{
		Form_pg_authid authid_tup = (Form_pg_authid) GETSTRUCT(roletup);

		namecpy(&uname, &authid_tup->rolname);
		ReleaseSysCache(roletup);
	}
	else
	{
		/* Generate numeric OID if we don't find an entry */
		sprintf(NameStr(uname), "%u", relowner);
	}

	values[Anum_pg_statlastop_stausename - 1] = NameGetDatum(&uname);
	/* set this column to update */
	new_record_repl[Anum_pg_statlastop_stausename - 1] = true;

	values[Anum_pg_statlastop_stasubtype - 1] = CStringGetTextDatum(subtype);
	/* set this column to update */
	new_record_repl[Anum_pg_statlastop_stasubtype - 1] = true;

	values[Anum_pg_statlastop_statime - 1] = GetCurrentTimestamp();
	/* set this column to update */
	new_record_repl[Anum_pg_statlastop_statime - 1] = true;

	if (HeapTupleIsValid(old_tuple))
	{
		new_tuple = heap_modify_tuple(old_tuple, RelationGetDescr(rel),
									  values,
									  isnull, new_record_repl);
		simple_heap_update(rel, &old_tuple->t_self, new_tuple);
		CatalogUpdateIndexes(rel, new_tuple);
	}
	else
	{
		new_tuple = heap_form_tuple(RelationGetDescr(rel), values, isnull);

		simple_heap_insert(rel, new_tuple);
		CatalogUpdateIndexes(rel, new_tuple);
	}

	if (HeapTupleIsValid(old_tuple))
		heap_freetuple(new_tuple);

} /* end MetaTrackAddUpdInternal */


void MetaTrackAddObject(Oid		classid, 
						Oid		objoid, 
						Oid		relowner,
						char*	actionname,
						char*	subtype)
{
	Relation	rel;

	if (IsBootstrapProcessingMode())
		return;

	if (IsSharedRelation(classid))
	{
		rel = heap_open(StatLastShOpRelationId, RowExclusiveLock);
	}
	else
	{
		rel = heap_open(StatLastOpRelationId, RowExclusiveLock);
	}

	MetaTrackAddUpdInternal(classid, objoid, relowner,
							actionname, subtype,
							rel, NULL);

	heap_close(rel, RowExclusiveLock);

/*	CommandCounterIncrement(); */

} /* end MetaTrackAddObject */

void MetaTrackUpdObject(Oid		classid, 
						Oid		objoid, 
						Oid		relowner,
						char*	actionname,
						char*	subtype)
{
	HeapTuple	tuple;
	ScanKeyData key[3];
	SysScanDesc desc;
	Relation	rel;
	int			ii = 0;

	if (IsBootstrapProcessingMode())
		return;

	if (IsSharedRelation(classid))
	{
		rel = heap_open(StatLastShOpRelationId, RowExclusiveLock);

		ScanKeyInit(&key[0],
					Anum_pg_statlastshop_classid,
					BTEqualStrategyNumber, F_OIDEQ,
					ObjectIdGetDatum(classid));
		ScanKeyInit(&key[1],
					Anum_pg_statlastshop_objid,
					BTEqualStrategyNumber, F_OIDEQ,
					ObjectIdGetDatum(objoid));
		ScanKeyInit(&key[2],
					Anum_pg_statlastshop_staactionname,
					BTEqualStrategyNumber, F_NAMEEQ,
					CStringGetDatum(actionname));

		desc = systable_beginscan(rel,
								  StatLastShOpClassidObjidStaactionnameIndexId,
								  true,
								  SnapshotNow, 3, key);
	}
	else
	{
		rel = heap_open(StatLastOpRelationId, RowExclusiveLock);

		ScanKeyInit(&key[0],
					Anum_pg_statlastop_classid,
					BTEqualStrategyNumber, F_OIDEQ,
					ObjectIdGetDatum(classid));
		ScanKeyInit(&key[1],
					Anum_pg_statlastop_objid,
					BTEqualStrategyNumber, F_OIDEQ,
					ObjectIdGetDatum(objoid));
		ScanKeyInit(&key[2],
					Anum_pg_statlastop_staactionname,
					BTEqualStrategyNumber, F_NAMEEQ,
					CStringGetDatum(actionname));

		desc = systable_beginscan(rel,
								  StatLastOpClassidObjidStaactionnameIndexId,
								  true,
								  SnapshotNow, 3, key);
	}

	/* should be a unique index - only 1 answer... */
	while (HeapTupleIsValid(tuple = systable_getnext(desc)))
	{
		MetaTrackAddUpdInternal(classid, objoid, relowner,
								actionname, subtype,
								rel, tuple);
		ii++;
	}
	systable_endscan(desc);
	heap_close(rel, RowExclusiveLock);

	/* add it if it didn't already exist */
	if (!ii)
		MetaTrackAddObject(classid, 
						   objoid, 
						   relowner,
						   actionname,
						   subtype);

} /* end MetaTrackUpdObject */
void MetaTrackDropObject(Oid		classid, 
						 Oid		objoid)
{
	HeapTuple	tuple;
	ScanKeyData key[3];
	SysScanDesc desc;
	Relation	rel;

	if (IsSharedRelation(classid))
	{
		/* DELETE FROM pg_stat_last_shoperation WHERE classid = :1 AND objid = :2 */

		rel = heap_open(StatLastShOpRelationId, RowExclusiveLock);

		ScanKeyInit(&key[0],
					Anum_pg_statlastshop_classid,
					BTEqualStrategyNumber, F_OIDEQ,
					ObjectIdGetDatum(classid));
		ScanKeyInit(&key[1],
					Anum_pg_statlastshop_objid,
					BTEqualStrategyNumber, F_OIDEQ,
					ObjectIdGetDatum(objoid));

		desc = systable_beginscan(rel,
								  StatLastShOpClassidObjidStaactionnameIndexId,
								  true,
								  SnapshotNow, 2, key);
	}
	else
	{
		/* DELETE FROM pg_stat_last_operation WHERE classid = :1 AND objid = :2 */
		rel = heap_open(StatLastOpRelationId, RowExclusiveLock);

		ScanKeyInit(&key[0],
					Anum_pg_statlastop_classid,
					BTEqualStrategyNumber, F_OIDEQ,
					ObjectIdGetDatum(classid));
		ScanKeyInit(&key[1],
					Anum_pg_statlastop_objid,
					BTEqualStrategyNumber, F_OIDEQ,
					ObjectIdGetDatum(objoid));

		desc = systable_beginscan(rel,
								  StatLastOpClassidObjidStaactionnameIndexId,
								  true,
								  SnapshotNow, 2, key);
	}

	while (HeapTupleIsValid(tuple = systable_getnext(desc)))
		simple_heap_delete(rel, &tuple->t_self);

	systable_endscan(desc);
	heap_close(rel, RowExclusiveLock);

} /* end MetaTrackDropObject */



/* --------------------------------
 *		AddNewAttributeTuples
 *
 *		this registers the new relation's schema by adding
 *		tuples to pg_attribute.
 * --------------------------------
 */
static void
AddNewAttributeTuples(Oid new_rel_oid,
					  TupleDesc tupdesc,
					  char relkind,
					  bool oidislocal,
					  int oidinhcount)
{
	const Form_pg_attribute *dpp;
	int			i;
	HeapTuple	tup;
	Relation	rel;
	CatalogIndexState indstate;
	int			natts = tupdesc->natts;
	ObjectAddress myself,
				referenced;

	/*
	 * open pg_attribute and its indexes.
	 */
	rel = heap_open(AttributeRelationId, RowExclusiveLock);

	indstate = CatalogOpenIndexes(rel);

	/*
	 * First we add the user attributes.  This is also a convenient place to
	 * add dependencies on their datatypes.
	 */
	dpp = tupdesc->attrs;
	for (i = 0; i < natts; i++)
	{
		/* Fill in the correct relation OID */
		(*dpp)->attrelid = new_rel_oid;
		/* Make sure these are OK, too */
		(*dpp)->attstattarget = -1;
		(*dpp)->attcacheoff = -1;

		tup = heap_addheader(Natts_pg_attribute,
							 false,
							 ATTRIBUTE_TUPLE_SIZE,
							 (void *) *dpp);

		simple_heap_insert(rel, tup);

		CatalogIndexInsert(indstate, tup);

		heap_freetuple(tup);

		myself.classId = RelationRelationId;
		myself.objectId = new_rel_oid;
		myself.objectSubId = i + 1;
		referenced.classId = TypeRelationId;
		referenced.objectId = (*dpp)->atttypid;
		referenced.objectSubId = 0;
		recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);

		dpp++;
	}

	/*
	 * Next we add the system attributes.  Skip OID if rel has no OIDs. Skip
	 * all for a view or type relation.  We don't bother with making datatype
	 * dependencies here, since presumably all these types are pinned.
	 */
	if (relkind != RELKIND_VIEW && relkind != RELKIND_COMPOSITE_TYPE)
	{
		dpp = SysAtt;
		for (i = 0; i < -1 - FirstLowInvalidHeapAttributeNumber; i++)
		{
			if (tupdesc->tdhasoid ||
				(*dpp)->attnum != ObjectIdAttributeNumber)
			{
				Form_pg_attribute attStruct;

				tup = heap_addheader(Natts_pg_attribute,
									 false,
									 ATTRIBUTE_TUPLE_SIZE,
									 (void *) *dpp);
				attStruct = (Form_pg_attribute) GETSTRUCT(tup);

				/* Fill in the correct relation OID in the copied tuple */
				attStruct->attrelid = new_rel_oid;

				/* Fill in correct inheritance info for the OID column */
				if (attStruct->attnum == ObjectIdAttributeNumber)
				{
					attStruct->attislocal = oidislocal;
					attStruct->attinhcount = oidinhcount;
				}

				/*
				 * Unneeded since they should be OK in the constant data
				 * anyway
				 */
				/* attStruct->attstattarget = 0; */
				/* attStruct->attcacheoff = -1; */

				simple_heap_insert(rel, tup);

				CatalogIndexInsert(indstate, tup);

				heap_freetuple(tup);
			}
			dpp++;
		}
	}

	/*
	 * clean up
	 */
	CatalogCloseIndexes(indstate);

	heap_close(rel, RowExclusiveLock);
}

/* --------------------------------
 *		InsertPgClassTuple
 *
 *		Construct and insert a new tuple in pg_class.
 *
 * Caller has already opened and locked pg_class.
 * Tuple data is taken from new_rel_desc->rd_rel, except for the
 * variable-width fields which are not present in a cached reldesc.
 * We always initialize relacl to NULL (i.e., default permissions),
 * and reloptions is set to the passed-in text array (if any).
 * --------------------------------
 */
void
InsertPgClassTuple(Relation pg_class_desc,
				   Relation new_rel_desc,
				   Oid new_rel_oid,
				   Datum reloptions)
{
	Form_pg_class rd_rel = new_rel_desc->rd_rel;
	Datum		values[Natts_pg_class];
	bool		nulls[Natts_pg_class];
	HeapTuple	tup;

	Assert(should_have_valid_relfrozenxid(
			   new_rel_oid, rd_rel->relkind, rd_rel->relstorage) ?
		   (rd_rel->relfrozenxid != InvalidTransactionId) :
		   (rd_rel->relfrozenxid == InvalidTransactionId));

	/* This is a tad tedious, but way cleaner than what we used to do... */
	memset(values, 0, sizeof(values));
	memset(nulls, false, sizeof(nulls));

	values[Anum_pg_class_relname - 1] = NameGetDatum(&rd_rel->relname);
	values[Anum_pg_class_relnamespace - 1] = ObjectIdGetDatum(rd_rel->relnamespace);
	values[Anum_pg_class_reltype - 1] = ObjectIdGetDatum(rd_rel->reltype);
	values[Anum_pg_class_relowner - 1] = ObjectIdGetDatum(rd_rel->relowner);
	values[Anum_pg_class_relam - 1] = ObjectIdGetDatum(rd_rel->relam);
	values[Anum_pg_class_relfilenode - 1] = ObjectIdGetDatum(rd_rel->relfilenode);
	values[Anum_pg_class_reltablespace - 1] = ObjectIdGetDatum(rd_rel->reltablespace);
	values[Anum_pg_class_relpages - 1] = Int32GetDatum(rd_rel->relpages);
	values[Anum_pg_class_reltuples - 1] = Float4GetDatum(rd_rel->reltuples);
	values[Anum_pg_class_reltoastrelid - 1] = ObjectIdGetDatum(rd_rel->reltoastrelid);
	values[Anum_pg_class_reltoastidxid - 1] = ObjectIdGetDatum(rd_rel->reltoastidxid);
	values[Anum_pg_class_relhasindex - 1] = BoolGetDatum(rd_rel->relhasindex);
	values[Anum_pg_class_relisshared - 1] = BoolGetDatum(rd_rel->relisshared);
	values[Anum_pg_class_relkind - 1] = CharGetDatum(rd_rel->relkind);
	values[Anum_pg_class_relstorage - 1] = CharGetDatum(rd_rel->relstorage);
	values[Anum_pg_class_relnatts - 1] = Int16GetDatum(rd_rel->relnatts);
	values[Anum_pg_class_relchecks - 1] = Int16GetDatum(rd_rel->relchecks);
	values[Anum_pg_class_reltriggers - 1] = Int16GetDatum(rd_rel->reltriggers);
	values[Anum_pg_class_relukeys - 1] = Int16GetDatum(rd_rel->relukeys);
	values[Anum_pg_class_relfkeys - 1] = Int16GetDatum(rd_rel->relfkeys);
	values[Anum_pg_class_relrefs - 1] = Int16GetDatum(rd_rel->relrefs);
	values[Anum_pg_class_relhasoids - 1] = BoolGetDatum(rd_rel->relhasoids);
	values[Anum_pg_class_relhaspkey - 1] = BoolGetDatum(rd_rel->relhaspkey);
	values[Anum_pg_class_relhasrules - 1] = BoolGetDatum(rd_rel->relhasrules);
	values[Anum_pg_class_relhassubclass - 1] = BoolGetDatum(rd_rel->relhassubclass);
	values[Anum_pg_class_relfrozenxid - 1] = TransactionIdGetDatum(rd_rel->relfrozenxid);
	/* start out with empty permissions */
	nulls[Anum_pg_class_relacl - 1] = true;
	if (reloptions != (Datum) 0)
		values[Anum_pg_class_reloptions - 1] = reloptions;
	else
		nulls[Anum_pg_class_reloptions - 1] = true;

	tup = heap_form_tuple(RelationGetDescr(pg_class_desc), values, nulls);

	/*
	 * The new tuple must have the oid already chosen for the rel.	Sure would
	 * be embarrassing to do this sort of thing in polite company.
	 */
	HeapTupleSetOid(tup, new_rel_oid);

	/* finally insert the new tuple, update the indexes, and clean up */
	simple_heap_insert(pg_class_desc, tup);

	CatalogUpdateIndexes(pg_class_desc, tup);

	heap_freetuple(tup);
}

/* --------------------------------
 *		AddNewRelationTuple
 *
 *		this registers the new relation in the catalogs by
 *		adding a tuple to pg_class.
 * --------------------------------
 */
static void
AddNewRelationTuple(Relation pg_class_desc,
					Relation new_rel_desc,
					Oid new_rel_oid,
					Oid new_type_oid,
					Oid relowner,
					char relkind,
					char relstorage,
					Datum reloptions)
{
	Form_pg_class new_rel_reltup;

	/*
	 * first we update some of the information in our uncataloged relation's
	 * relation descriptor.
	 */
	new_rel_reltup = new_rel_desc->rd_rel;

	switch (relkind)
	{
		case RELKIND_RELATION:
		case RELKIND_INDEX:
		case RELKIND_TOASTVALUE:
		case RELKIND_AOSEGMENTS:
		case RELKIND_AOBLOCKDIR:
		case RELKIND_AOVISIMAP:
			/* The relation is real, but as yet empty */
			new_rel_reltup->relpages = 0;
			new_rel_reltup->reltuples = 0;

			/* estimated stats for external tables */
			/* NOTE: look at cdb_estimate_rel_size() if changing these values */
			if(relstorage_is_external(relstorage))
			{
				new_rel_reltup->relpages = 1000;
				new_rel_reltup->reltuples = 1000000;
			}
			break;
		case RELKIND_SEQUENCE:
			/* Sequences always have a known size */
			new_rel_reltup->relpages = 1;
			new_rel_reltup->reltuples = 1;
			break;
		default:
			/* Views, etc, have no disk storage */
			new_rel_reltup->relpages = 0;
			new_rel_reltup->reltuples = 0;
			break;
	}

	/* Initialize relfrozenxid */
	if (should_have_valid_relfrozenxid(new_rel_oid, relkind, relstorage))
	{
		/*
		 * Initialize to the minimum XID that could put tuples in the table.
		 * We know that no xacts older than RecentXmin are still running, so
		 * that will do.
		 */
		new_rel_reltup->relfrozenxid = RecentXmin;
	}
	else
	{
		/*
		 * Other relation types will not contain XIDs, so set relfrozenxid to
		 * InvalidTransactionId.  (Note: a sequence does contain a tuple, but
		 * we force its xmin to be FrozenTransactionId always; see
		 * commands/sequence.c.)
		 */
		new_rel_reltup->relfrozenxid = InvalidTransactionId;
	}

	new_rel_reltup->relowner = relowner;
	new_rel_reltup->reltype = new_type_oid;
	new_rel_reltup->relkind = relkind;
	new_rel_reltup->relstorage = relstorage;

	new_rel_desc->rd_att->tdtypeid = new_type_oid;

	/* Now build and insert the tuple */
	InsertPgClassTuple(pg_class_desc, new_rel_desc, new_rel_oid, reloptions);
}


/* --------------------------------
 *		AddNewRelationType -
 *
 *		define a composite type corresponding to the new relation
 * --------------------------------
 */
static Oid
AddNewRelationType(const char *typeName,
				   Oid typeNamespace,
				   Oid new_rel_oid,
				   char new_rel_kind,
				   Oid ownerid,
				   Oid new_array_type)
{
	return
		TypeCreate(InvalidOid,	/* no predetermined OID */
				   typeName,	/* type name */
				   typeNamespace,		/* type namespace */
				   new_rel_oid, /* relation oid */
				   new_rel_kind,	/* relation kind */
				   ownerid,		/* owner's ID */
				   -1,			/* internal size (varlena) */
				   'c',			/* type-type (complex) */
				   DEFAULT_TYPDELIM,	/* default array delimiter */
				   F_RECORD_IN, /* input procedure */
				   F_RECORD_OUT,	/* output procedure */
				   F_RECORD_RECV,		/* receive procedure */
				   F_RECORD_SEND,		/* send procedure */
				   InvalidOid,	/* typmodin procedure - none */
				   InvalidOid,	/* typmodout procedure - none */
				   InvalidOid,	/* analyze procedure - default */
				   InvalidOid,	/* array element type - irrelevant */
				   false,		/* this is not an array type */
				   new_array_type,		/* array type if any */
				   InvalidOid,	/* domain base type - irrelevant */
				   NULL,		/* default value - none */
				   NULL,		/* default binary representation */
				   false,		/* passed by reference */
				   'd',			/* alignment - must be the largest! */
				   'x',			/* fully TOASTable */
				   -1,			/* typmod */
				   0,			/* array dimensions for typBaseType */
				   false);		/* Type NOT NULL */
}

void
InsertGpRelationNodeTuple(
	Relation 		gp_relation_node,
	Oid				relationId,
	char			*relname,
	Oid				tablespaceOid,
	Oid				relfilenode,
	int32			segmentFileNum,
	bool			updateIndex,
	ItemPointer		persistentTid,
	int64			persistentSerialNum)
{
	Datum		values[Natts_gp_relation_node];
	bool		nulls[Natts_gp_relation_node];
	HeapTuple	tuple;

	if (!Persistent_BeforePersistenceWork() &&
		PersistentStore_IsZeroTid(persistentTid))
	{	
		elog(ERROR, 
			 "Inserting with invalid TID (0,0) into relation id %u '%s', relfilenode %u, segment file #%d, serial number " INT64_FORMAT,
			 relationId,
			 relname,
			 relfilenode,
			 segmentFileNum,
			 persistentSerialNum);
	}

	memset(values, 0, sizeof(values));
	memset(nulls, false, sizeof(nulls));

	if (Debug_persistent_print)
		elog(Persistent_DebugPrintLevel(), 
			 "InsertGpRelationNodeTuple: Inserting into relation id %u '%s', relfilenode %u, segment file #%d, serial number " INT64_FORMAT ", TID %s",
			 relationId,
			 relname,
			 relfilenode,
			 segmentFileNum,
			 persistentSerialNum,
			 ItemPointerToString(persistentTid));

	/*
	 * gp_relation_node stores tablespaceOId in pg_class fashion, which means
	 * defaultTablespace is represented as "0".
	 */
	Assert (tablespaceOid != MyDatabaseTableSpace);
	
	GpRelationNode_SetDatumValues(
								values,
								tablespaceOid,
								relfilenode,
								segmentFileNum,
								/* createMirrorDataLossTrackingSessionNum */ 0,
								persistentTid,
								persistentSerialNum);

	/* XXX XXX: note optional index update */
	tuple = heap_form_tuple(RelationGetDescr(gp_relation_node), values, nulls);

	/* finally insert the new tuple, update the indexes, and clean up */
	simple_heap_insert(gp_relation_node, tuple);

	if (updateIndex)
	{
		CatalogUpdateIndexes(gp_relation_node, tuple);
	}

	heap_freetuple(tuple);
}

void
UpdateGpRelationNodeTuple(
	Relation 	gp_relation_node,
	HeapTuple 	tuple,
	Oid         tablespaceOid,
	Oid			relfilenode,
	int32		segmentFileNum,
	ItemPointer persistentTid,
	int64 		persistentSerialNum)
{
	Datum		repl_val[Natts_gp_relation_node];
	bool		repl_null[Natts_gp_relation_node];
	bool		repl_repl[Natts_gp_relation_node];
	HeapTuple	newtuple;

	if (!Persistent_BeforePersistenceWork() &&
		PersistentStore_IsZeroTid(persistentTid))
	{	
		elog(ERROR, 
			 "Updating with invalid TID (0,0) in relfilenode %u, segment file #%d, serial number " INT64_FORMAT,
			 relfilenode,
			 segmentFileNum,
			 persistentSerialNum);
	}

	if (Debug_persistent_print)
		elog(Persistent_DebugPrintLevel(), 
			 "UpdateGpRelationNodeTuple: Updating relfilenode %u, segment file #%d, serial number " INT64_FORMAT " at TID %s",
			 relfilenode,
			 segmentFileNum,
			 persistentSerialNum,
			 ItemPointerToString(persistentTid));

	memset(repl_val, 0, sizeof(repl_val));
	memset(repl_null, false, sizeof(repl_null));
	memset(repl_repl, false, sizeof(repl_null));

	repl_repl[Anum_gp_relation_node_tablespace_oid - 1] = true;
	repl_val[Anum_gp_relation_node_tablespace_oid - 1] = ObjectIdGetDatum(tablespaceOid);

	repl_repl[Anum_gp_relation_node_relfilenode_oid - 1] = true;
	repl_val[Anum_gp_relation_node_relfilenode_oid - 1] = ObjectIdGetDatum(relfilenode);
	
	repl_repl[Anum_gp_relation_node_segment_file_num - 1] = true;
	repl_val[Anum_gp_relation_node_segment_file_num - 1] = Int32GetDatum(segmentFileNum);

	// UNDONE: createMirrorDataLossTrackingSessionNum

	repl_repl[Anum_gp_relation_node_persistent_tid- 1] = true;
	repl_val[Anum_gp_relation_node_persistent_tid- 1] = PointerGetDatum(persistentTid);
	
	repl_repl[Anum_gp_relation_node_persistent_serial_num - 1] = true;
	repl_val[Anum_gp_relation_node_persistent_serial_num - 1] = Int64GetDatum(persistentSerialNum);

	newtuple = heap_modify_tuple(tuple, RelationGetDescr(gp_relation_node), repl_val, repl_null, repl_repl);
	
	simple_heap_update(gp_relation_node, &newtuple->t_self, newtuple);

	CatalogUpdateIndexes(gp_relation_node, newtuple);

	heap_freetuple(newtuple);
}


static void
AddNewRelationNodeTuple(
						Relation gp_relation_node,
						Relation new_rel)
{
	if (new_rel->rd_segfile0_relationnodeinfo.isPresent)
	{
		InsertGpRelationNodeTuple(
							gp_relation_node,
							new_rel->rd_id,
							new_rel->rd_rel->relname.data,
							new_rel->rd_rel->reltablespace,
							new_rel->rd_rel->relfilenode,
							/* segmentFileNum */ 0,
							/* updateIndex */ true,
							&new_rel->rd_segfile0_relationnodeinfo.persistentTid,
							new_rel->rd_segfile0_relationnodeinfo.persistentSerialNum);
							
	}
}

/* --------------------------------
 *		heap_create_with_catalog
 *
 *		creates a new cataloged relation.  see comments above.
 * --------------------------------
 */
Oid
heap_create_with_catalog(const char *relname,
						 Oid relnamespace,
						 Oid reltablespace,
						 Oid relid,
						 Oid ownerid,
						 TupleDesc tupdesc,
						 Oid relam,
						 char relkind,
						 char relstorage,
						 bool shared_relation,
						 bool oidislocal,
						 bool bufferPoolBulkLoad,
						 int oidinhcount,
						 OnCommitAction oncommit,
                         const struct GpPolicy *policy,
						 Datum reloptions,
						 bool allow_system_table_mods,
						 bool valid_opts,
						 ItemPointer persistentTid,
						 int64 *persistentSerialNum)
{
	Relation	pg_class_desc;
	Relation	gp_relation_node_desc;
	Relation	new_rel_desc;
	Oid			old_type_oid;
	Oid			new_type_oid;
	Oid			new_array_oid = InvalidOid;
	bool		appendOnlyRel;
	StdRdOptions *stdRdOptions;
	int			safefswritesize = gp_safefswritesize;
	Oid			existing_rowtype_oid = InvalidOid;
	char	   *relarrayname = NULL;

	/*
	 * Don't create the row type if the bootstrapper tells us it already
	 * knows what it is.
	 */
	if (IsBootstrapProcessingMode())
	{
		/*
		 * Some relations need to have a fixed relation type
		 * OID, because it is referenced in code.
		 *
		 * GPDB_90_MERGE_FIXME: In PostgreSQL 9.0, there's a
		 * new BKI directive, BKI_ROWTYPE_OID(<oid>), for
		 * doing the same. Replace this hack with that once
		 * we merge with 9.0.
		 */
		switch (relid)
		{
			case GpPersistentRelationNodeRelationId:
				existing_rowtype_oid = GP_PERSISTENT_RELATION_NODE_OID;
				break;
			case GpPersistentDatabaseNodeRelationId:
				existing_rowtype_oid = GP_PERSISTENT_DATABASE_NODE_OID;
				break;
			case GpPersistentTablespaceNodeRelationId:
				existing_rowtype_oid = GP_PERSISTENT_TABLESPACE_NODE_OID;
				break;
			case GpPersistentFilespaceNodeRelationId:
				existing_rowtype_oid = GP_PERSISTENT_FILESPACE_NODE_OID;
				break;
			case GpRelationNodeRelationId:
				existing_rowtype_oid = GP_RELATION_NODE_OID;
				break;

			case GpGlobalSequenceRelationId:
				existing_rowtype_oid = GP_GLOBAL_SEQUENCE_RELTYPE_OID;
				break;

			case DatabaseRelationId:
				existing_rowtype_oid = PG_DATABASE_RELTYPE_OID;
				break;

			case AuthIdRelationId:
				existing_rowtype_oid = PG_AUTHID_RELTYPE_OID;
				break;

			case AuthMemRelationId:
				existing_rowtype_oid = PG_AUTH_MEMBERS_RELTYPE_OID;
				break;

			default:
				break;
		}
	}

	pg_class_desc = heap_open(RelationRelationId, RowExclusiveLock);

	if (!IsBootstrapProcessingMode())
		gp_relation_node_desc = heap_open(GpRelationNodeRelationId, RowExclusiveLock);
	else
		gp_relation_node_desc = NULL;

	/*
	 * sanity checks
	 */
	Assert(IsNormalProcessingMode() || IsBootstrapProcessingMode());

	/*
	 * Was "appendonly" specified in the relopts? If yes, fix our relstorage.
	 * Also, check for override (debug) GUCs.
	 */
	stdRdOptions = (StdRdOptions*) heap_reloptions(
			relkind, reloptions, !valid_opts);
	appendOnlyRel = stdRdOptions->appendonly;
	validateAppendOnlyRelOptions(appendOnlyRel,
								 stdRdOptions->blocksize,
								 safefswritesize,
								 stdRdOptions->compresslevel,
								 stdRdOptions->compresstype,
								 stdRdOptions->checksum,
								 relkind,
								 stdRdOptions->columnstore);
	if(appendOnlyRel)
	{
		if(stdRdOptions->columnstore)
            relstorage = RELSTORAGE_AOCOLS;
		else
			relstorage = RELSTORAGE_AOROWS;
		reloptions = transformAOStdRdOptions(stdRdOptions, reloptions);
	}

	/* MPP-8058: disallow OIDS on column-oriented tables */
	if (tupdesc->tdhasoid && 
		IsNormalProcessingMode() &&
        (Gp_role == GP_ROLE_DISPATCH))
	{
		if (relstorage == RELSTORAGE_AOCOLS)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg(
							 "OIDS=TRUE is not allowed on tables that "
							 "use column-oriented storage. Use OIDS=FALSE"
							 )));
		else
			ereport(NOTICE,
					(errmsg(
							 "OIDS=TRUE is not recommended for user-created "
							 "tables. Use OIDS=FALSE to prevent wrap-around "
							 "of the OID counter"
							 )));
	}

	CheckAttributeNamesTypes(tupdesc, relkind);

	if (get_relname_relid(relname, relnamespace))
		ereport(ERROR,
				(errcode(ERRCODE_DUPLICATE_TABLE),
				 errmsg("relation \"%s\" already exists", relname)));

	/*
	 * Since we are going to create a rowtype as well, also check for
	 * collision with an existing type name.  If there is one and it's an
	 * autogenerated array, we can rename it out of the way; otherwise we can
	 * at least give a good error message.
	 */
	old_type_oid = GetSysCacheOid(TYPENAMENSP,
								  CStringGetDatum(relname),
								  ObjectIdGetDatum(relnamespace),
								  0, 0);
	if (OidIsValid(old_type_oid) && !OidIsValid(existing_rowtype_oid))
	{
		if (!moveArrayTypeName(old_type_oid, relname, relnamespace))
			ereport(ERROR,
					(errcode(ERRCODE_DUPLICATE_OBJECT),
					 errmsg("type \"%s\" already exists", relname),
			   errhint("A relation has an associated type of the same name, "
					   "so you must use a name that doesn't conflict "
					   "with any existing type.")));
	}

	/*
	 * Validate shared/non-shared tablespace (must check this before doing
	 * GetNewRelFileNode, to prevent Assert therein)
	 */
	if (shared_relation)
	{
		if (reltablespace != GLOBALTABLESPACE_OID)
			/* elog since this is not a user-facing error */
			elog(ERROR,
				 "shared relations must be placed in pg_global tablespace");
	}
	else
	{
		if (reltablespace == GLOBALTABLESPACE_OID)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("only shared relations can be placed in pg_global tablespace")));
	}

	/*
	 * Get preassigned OID for GP_ROLE_EXECUTE or binary upgrade
	 */
	if (!OidIsValid(relid) && (Gp_role == GP_ROLE_EXECUTE || IsBinaryUpgrade))
		relid = GetPreassignedOidForRelation(relnamespace, relname);

	/*
	 * GP_ROLE_DISPATCH and GP_ROLE_UTILITY do not have preassigned OIDs.
	 * Allocate new OIDs here.
	 *
	 * For sequence relations, the relfilenode has to be the same as OID.
	 * To accomplish this, we have a special function GetSequenceRelationOid
	 * which locks both the Oid and relfilenode counter, syncs them, and
	 * allocates the synced value to here.
	 */
	if (!OidIsValid(relid) && Gp_role != GP_ROLE_EXECUTE)
	{
		if (relkind == RELKIND_SEQUENCE)
			relid = GetNewSequenceRelationOid(pg_class_desc);
		else
			relid = GetNewOid(pg_class_desc);
	}

	/*
	 * Create the relcache entry (mostly dummy at this point) and the physical
	 * disk file.  (If we fail further down, it's the smgr's responsibility to
	 * remove the disk file again.)
	 */
	new_rel_desc = heap_create(relname,
							   relnamespace,
							   reltablespace,
							   relid,
							   tupdesc,
							   relam,
							   relkind,
							   relstorage,
							   shared_relation,
							   allow_system_table_mods,
							   bufferPoolBulkLoad);

	Assert(relid == RelationGetRelid(new_rel_desc));

	if (persistentTid != NULL)
	{
		*persistentTid = new_rel_desc->rd_segfile0_relationnodeinfo.persistentTid;
		*persistentSerialNum = new_rel_desc->rd_segfile0_relationnodeinfo.persistentSerialNum;
	}

	/*
	 * Decide whether to create an array type over the relation's rowtype. We
	 * do not create any array types for system catalogs (ie, those made
	 * during initdb).	We create array types for regular relations, views,
	 * and composite types ... but not, eg, for toast tables or sequences.
	 *
	 * Also not for the auxiliary heaps created for bitmap indexes or append-
	 * only tables.
	 */
	if (IsUnderPostmaster && ((relkind == RELKIND_RELATION && !appendOnlyRel) ||
							  relkind == RELKIND_VIEW ||
							  relkind == RELKIND_COMPOSITE_TYPE) &&
		relnamespace != PG_BITMAPINDEX_NAMESPACE)
	{
		/* OK, so pre-assign a type OID for the array type */
		Relation	pg_type = heap_open(TypeRelationId, AccessShareLock);

		relarrayname = makeArrayTypeName(relname, relnamespace);

		/*
		 * If we are expected to get a preassigned Oid but receive InvalidOid,
		 * get a new Oid. This can happen during upgrades from GPDB4 to 5 where
		 * array types over relation rowtypes were introduced so there are no
		 * pre-existing array types to dump from the old cluster
		 */
		if (Gp_role == GP_ROLE_EXECUTE || IsBinaryUpgrade)
		{
			new_array_oid = GetPreassignedOidForType(relnamespace, relarrayname);

			if (new_array_oid == InvalidOid && IsBinaryUpgrade)
				new_array_oid = GetNewOid(pg_type);
		}
		else
			new_array_oid = GetNewOid(pg_type);
		heap_close(pg_type, AccessShareLock);
	}

	/*
	 * Since defining a relation also defines a complex type, we add a new
	 * system type corresponding to the new relation.
	 *
	 * NOTE: we could get a unique-index failure here, in case someone else is
	 * creating the same type name in parallel but hadn't committed yet when
	 * we checked for a duplicate name above.
	 */
	if (existing_rowtype_oid != InvalidOid)
		new_type_oid = existing_rowtype_oid;
	else
	{
		new_type_oid = AddNewRelationType(relname,
										  relnamespace,
										  relid,
										  relkind,
										  ownerid,
										  new_array_oid);
	}

	/*
	 * Now make the array type if wanted.
	 */
	if (OidIsValid(new_array_oid))
	{
		if (!relarrayname)
			relarrayname = makeArrayTypeName(relname, relnamespace);

		TypeCreate(new_array_oid,		/* force the type's OID to this */
				   relarrayname,	/* Array type name */
				   relnamespace,	/* Same namespace as parent */
				   InvalidOid,	/* Not composite, no relationOid */
				   0,			/* relkind, also N/A here */
				   ownerid,		/* owner's ID */
				   -1,			/* Internal size (varlena) */
				   TYPTYPE_BASE,	/* Not composite - typelem is */
				   DEFAULT_TYPDELIM,	/* default array delimiter */
				   F_ARRAY_IN,	/* array input proc */
				   F_ARRAY_OUT, /* array output proc */
				   F_ARRAY_RECV,	/* array recv (bin) proc */
				   F_ARRAY_SEND,	/* array send (bin) proc */
				   InvalidOid,	/* typmodin procedure - none */
				   InvalidOid,	/* typmodout procedure - none */
				   InvalidOid,	/* analyze procedure - default */
				   new_type_oid,	/* array element type - the rowtype */
				   true,		/* yes, this is an array type */
				   InvalidOid,	/* this has no array type */
				   InvalidOid,	/* domain base type - irrelevant */
				   NULL,		/* default value - none */
				   NULL,		/* default binary representation */
				   false,		/* passed by reference */
				   'd',			/* alignment - must be the largest! */
				   'x',			/* fully TOASTable */
				   -1,			/* typmod */
				   0,			/* array dimensions for typBaseType */
				   false);		/* Type NOT NULL */

		pfree(relarrayname);
	}

	/*
	 * now create an entry in pg_class for the relation.
	 *
	 * NOTE: we could get a unique-index failure here, in case someone else is
	 * creating the same relation name in parallel but hadn't committed yet
	 * when we checked for a duplicate name above.
	 */
	AddNewRelationTuple(pg_class_desc,
						new_rel_desc,
						relid,
						new_type_oid,
						ownerid,
						relkind,
						relstorage,
						reloptions);

	if (gp_relation_node_desc != NULL)
	{
		AddNewRelationNodeTuple(gp_relation_node_desc,
			                    new_rel_desc);

		heap_close(gp_relation_node_desc, RowExclusiveLock);
	}


	/*
	 * if this is an append-only relation, add an entry in pg_appendonly.
	 */
	if(appendOnlyRel)
	{
		InsertAppendOnlyEntry(relid,
							  stdRdOptions->blocksize,
							  safefswritesize,
							  stdRdOptions->compresslevel,
							  stdRdOptions->checksum,
                              stdRdOptions->columnstore,
							  stdRdOptions->compresstype,
							  InvalidOid,
							  InvalidOid,
							  InvalidOid,
							  InvalidOid,
							  InvalidOid);
	}


	/*
	 * now add tuples to pg_attribute for the attributes in our new relation.
	 */
	AddNewAttributeTuples(relid, new_rel_desc->rd_att, relkind,
						  oidislocal, oidinhcount);

	/*
	 * Make a dependency link to force the relation to be deleted if its
	 * namespace is.  Also make a dependency link to its owner.
	 *
	 * For composite types, these dependencies are tracked for the pg_type
	 * entry, so we needn't record them here.  Likewise, TOAST tables don't
	 * need a namespace dependency (they live in a pinned namespace) nor an
	 * owner dependency (they depend indirectly through the parent table).
	 * Also, skip this in bootstrap mode, since we don't make dependencies
	 * while bootstrapping.
	 */
	if (relkind != RELKIND_COMPOSITE_TYPE &&
		relkind != RELKIND_TOASTVALUE &&
		!IsBootstrapProcessingMode())
	{
		ObjectAddress myself,
					referenced;

		myself.classId = RelationRelationId;
		myself.objectId = relid;
		myself.objectSubId = 0;
		referenced.classId = NamespaceRelationId;
		referenced.objectId = relnamespace;
		referenced.objectSubId = 0;
		recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);

		recordDependencyOnOwner(RelationRelationId, relid, ownerid);

		recordDependencyOnCurrentExtension(&myself, false);
	}

	/*
	 * We used to store pre-cooked constraints and defaults here.
	 * We now store them along with raw constraints and defaults to ensure that
	 * 	we have oid of pg_constraint and pg_attrdef consistent across segments
	 */

	/*
	 * If there's a special on-commit action, remember it
	 */
	if (oncommit != ONCOMMIT_NOOP)
		register_on_commit_action(relid, oncommit);

	/*
     * CDB: If caller gave us a distribution policy, store the distribution
     * key column list in the gp_distribution_policy catalog and attach a
     * copy to the relcache entry.
     */
    if (policy && (Gp_role == GP_ROLE_DISPATCH || IsBinaryUpgrade))
    {
        Assert(relkind == RELKIND_RELATION);
        new_rel_desc->rd_cdbpolicy = GpPolicyCopy(GetMemoryChunkContext(new_rel_desc), policy);
        GpPolicyStore(relid, policy);
    }

	if (Gp_role == GP_ROLE_DISPATCH) /* MPP-11313: */
	{
		bool doIt = true;
		char *subtyp = "TABLE";

		switch (relkind)
		{
			case RELKIND_RELATION:
				break;
			case RELKIND_INDEX:
				subtyp = "INDEX";
				break;
			case RELKIND_SEQUENCE:
				subtyp = "SEQUENCE";
				break;
			case RELKIND_VIEW:
				subtyp = "VIEW";
				break;
			default:
				doIt = false;
		}

		/* MPP-7576: don't track internal namespace tables */
		switch (relnamespace) 
		{
			case PG_CATALOG_NAMESPACE:
				/* MPP-7773: don't track objects in system namespace
				 * if modifying system tables (eg during upgrade)  
				 */
				if (allowSystemTableModsDDL)
					doIt = false;
				break;

			case PG_TOAST_NAMESPACE:
			case PG_BITMAPINDEX_NAMESPACE:
			case PG_AOSEGMENT_NAMESPACE:
				doIt = false;
				break;
			default:
				break;
		}

		/* MPP-7572: not valid if in any temporary namespace */
		if (doIt)
			doIt = (!(isAnyTempNamespace(relnamespace)));

		/* MPP-6929: metadata tracking */
		if (doIt)
			MetaTrackAddObject(RelationRelationId,
							   relid, GetUserId(), /* not ownerid */
							   "CREATE", subtyp
					);
	}

	/*
	 * ok, the relation has been cataloged, so close our relations and return
	 * the OID of the newly created relation.
	 */
	heap_close(new_rel_desc, NoLock);	/* do not unlock till end of xact */
	heap_close(pg_class_desc, RowExclusiveLock);

	return relid;
}


/*
 *		RelationRemoveInheritance
 *
 * Formerly, this routine checked for child relations and aborted the
 * deletion if any were found.	Now we rely on the dependency mechanism
 * to check for or delete child relations.	By the time we get here,
 * there are no children and we need only remove any pg_inherits rows
 * linking this relation to its parent(s).
 */
static void
RelationRemoveInheritance(Oid relid)
{
	Relation	catalogRelation;
	SysScanDesc scan;
	ScanKeyData key;
	HeapTuple	tuple;

	catalogRelation = heap_open(InheritsRelationId, RowExclusiveLock);

	ScanKeyInit(&key,
				Anum_pg_inherits_inhrelid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(relid));

	scan = systable_beginscan(catalogRelation, InheritsRelidSeqnoIndexId, true,
							  SnapshotNow, 1, &key);

	while (HeapTupleIsValid(tuple = systable_getnext(scan)))
		simple_heap_delete(catalogRelation, &tuple->t_self);

	systable_endscan(scan);
	heap_close(catalogRelation, RowExclusiveLock);
}

static void
RemovePartitioning(Oid relid)
{
	Relation rel;
	SysScanDesc scan;
	ScanKeyData key;
	HeapTuple tuple;
	Relation pgrule;

	if (Gp_role == GP_ROLE_EXECUTE)
		return;

	RemovePartitionEncodingByRelid(relid);

	/* loop through all matches in pg_partition */
	rel = heap_open(PartitionRelationId, RowExclusiveLock);
	pgrule = heap_open(PartitionRuleRelationId,
					   RowExclusiveLock);

	ScanKeyInit(&key,
				Anum_pg_partition_parrelid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(relid));

	scan = systable_beginscan(rel, PartitionParrelidIndexId, true,
							  SnapshotNow, 1, &key);
	while (HeapTupleIsValid(tuple = systable_getnext(scan)))
	{
		Oid			paroid = HeapTupleGetOid(tuple);
		SysScanDesc rule_scan;
		ScanKeyData rule_key;
		HeapTuple	rule_tuple;

		/* remove all rows for pg_partition_rule */
		ScanKeyInit(&rule_key,
					Anum_pg_partition_rule_paroid,
					BTEqualStrategyNumber, F_OIDEQ,
					ObjectIdGetDatum(paroid));
		rule_scan = systable_beginscan(pgrule, PartitionRuleParoidParparentruleParruleordIndexId, true,
									   SnapshotNow, 1, &rule_key);
		while (HeapTupleIsValid(rule_tuple = systable_getnext(rule_scan)))
			simple_heap_delete(pgrule, &rule_tuple->t_self);
		systable_endscan(rule_scan);

		/* remove ourself */
		simple_heap_delete(rel, &tuple->t_self);
	}
	systable_endscan(scan);
	heap_close(rel, NoLock);

	/* we might be a leaf partition: delete any records */

	ScanKeyInit(&key,
				Anum_pg_partition_rule_parchildrelid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(relid));

	scan = systable_beginscan(pgrule, PartitionRuleParchildrelidIndexId, true,
							  SnapshotNow, 1, &key);
	while (HeapTupleIsValid(tuple = systable_getnext(scan)))
		simple_heap_delete(pgrule, &tuple->t_self);
	systable_endscan(scan);
	heap_close(pgrule, NoLock);

	CommandCounterIncrement();
}

/*
 *		DeleteRelationTuple
 *
 * Remove pg_class row for the given relid.
 *
 * Note: this is shared by relation deletion and index deletion.  It's
 * not intended for use anyplace else.
 */
void
DeleteRelationTuple(Oid relid)
{
	Relation	pg_class_desc;
	HeapTuple	tup;

	/* Grab an appropriate lock on the pg_class relation */
	pg_class_desc = heap_open(RelationRelationId, RowExclusiveLock);

	tup = SearchSysCache(RELOID,
						 ObjectIdGetDatum(relid),
						 0, 0, 0);
	if (!HeapTupleIsValid(tup))
		elog(ERROR, "cache lookup failed for relation %u", relid);

	/* delete the relation tuple from pg_class, and finish up */
	simple_heap_delete(pg_class_desc, &tup->t_self);

	ReleaseSysCache(tup);

	heap_close(pg_class_desc, RowExclusiveLock);
}

/*
 *		DeleteAttributeTuples
 *
 * Remove pg_attribute rows for the given relid.
 *
 * Note: this is shared by relation deletion and index deletion.  It's
 * not intended for use anyplace else.
 */
void
DeleteAttributeTuples(Oid relid)
{
	Relation	attrel;
	SysScanDesc scan;
	ScanKeyData key[1];
	HeapTuple	atttup;

	/* Grab an appropriate lock on the pg_attribute relation */
	attrel = heap_open(AttributeRelationId, RowExclusiveLock);

	/* Use the index to scan only attributes of the target relation */
	ScanKeyInit(&key[0],
				Anum_pg_attribute_attrelid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(relid));

	scan = systable_beginscan(attrel, AttributeRelidNumIndexId, true,
							  SnapshotNow, 1, key);

	/* Delete all the matching tuples */
	while ((atttup = systable_getnext(scan)) != NULL)
		simple_heap_delete(attrel, &atttup->t_self);

	/* Clean up after the scan */
	systable_endscan(scan);
	heap_close(attrel, RowExclusiveLock);
}

/*
 *		RemoveAttributeById
 *
 * This is the guts of ALTER TABLE DROP COLUMN: actually mark the attribute
 * deleted in pg_attribute.  We also remove pg_statistic entries for it.
 * (Everything else needed, such as getting rid of any pg_attrdef entry,
 * is handled by dependency.c.)
 */
void
RemoveAttributeById(Oid relid, AttrNumber attnum)
{
	Relation	rel;
	Relation	attr_rel;
	HeapTuple	tuple;
	Form_pg_attribute attStruct;
	char		newattname[NAMEDATALEN];

	/*
	 * Grab an exclusive lock on the target table, which we will NOT release
	 * until end of transaction.  (In the simple case where we are directly
	 * dropping this column, AlterTableDropColumn already did this ... but
	 * when cascading from a drop of some other object, we may not have any
	 * lock.)
	 */
	rel = relation_open(relid, AccessExclusiveLock);

	attr_rel = heap_open(AttributeRelationId, RowExclusiveLock);

	tuple = SearchSysCacheCopy(ATTNUM,
							   ObjectIdGetDatum(relid),
							   Int16GetDatum(attnum),
							   0, 0);
	if (!HeapTupleIsValid(tuple))		/* shouldn't happen */
		elog(ERROR, "cache lookup failed for attribute %d of relation %u",
			 attnum, relid);
	attStruct = (Form_pg_attribute) GETSTRUCT(tuple);

	if (attnum < 0)
	{
		/* System attribute (probably OID) ... just delete the row */

		simple_heap_delete(attr_rel, &tuple->t_self);
	}
	else
	{
		/* Dropping user attributes is lots harder */

		/* Mark the attribute as dropped */
		attStruct->attisdropped = true;

		/*
		 * Set the type OID to invalid.  A dropped attribute's type link
		 * cannot be relied on (once the attribute is dropped, the type might
		 * be too). Fortunately we do not need the type row --- the only
		 * really essential information is the type's typlen and typalign,
		 * which are preserved in the attribute's attlen and attalign.  We set
		 * atttypid to zero here as a means of catching code that incorrectly
		 * expects it to be valid.
		 */
		attStruct->atttypid = InvalidOid;

		/* Remove any NOT NULL constraint the column may have */
		attStruct->attnotnull = false;

		/* We don't want to keep stats for it anymore */
		attStruct->attstattarget = 0;

		/*
		 * Change the column name to something that isn't likely to conflict
		 */
		snprintf(newattname, sizeof(newattname),
				 "........pg.dropped.%d........", attnum);
		namestrcpy(&(attStruct->attname), newattname);

		simple_heap_update(attr_rel, &tuple->t_self, tuple);

		/* keep the system catalog indexes current */
		CatalogUpdateIndexes(attr_rel, tuple);
	}

	/*
	 * Because updating the pg_attribute row will trigger a relcache flush for
	 * the target relation, we need not do anything else to notify other
	 * backends of the change.
	 */

	heap_close(attr_rel, RowExclusiveLock);

	if (attnum > 0)
		RemoveStatistics(relid, attnum);

	relation_close(rel, NoLock);
}

/*
 *		RemoveAttrDefault
 *
 * If the specified relation/attribute has a default, remove it.
 * (If no default, raise error if complain is true, else return quietly.)
 */
void
RemoveAttrDefault(Oid relid, AttrNumber attnum,
				  DropBehavior behavior, bool complain)
{
	Relation	attrdef_rel;
	ScanKeyData scankeys[2];
	SysScanDesc scan;
	HeapTuple	tuple;
	bool		found = false;

	attrdef_rel = heap_open(AttrDefaultRelationId, RowExclusiveLock);

	ScanKeyInit(&scankeys[0],
				Anum_pg_attrdef_adrelid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(relid));
	ScanKeyInit(&scankeys[1],
				Anum_pg_attrdef_adnum,
				BTEqualStrategyNumber, F_INT2EQ,
				Int16GetDatum(attnum));

	scan = systable_beginscan(attrdef_rel, AttrDefaultIndexId, true,
							  SnapshotNow, 2, scankeys);

	/* There should be at most one matching tuple, but we loop anyway */
	while (HeapTupleIsValid(tuple = systable_getnext(scan)))
	{
		ObjectAddress object;

		object.classId = AttrDefaultRelationId;
		object.objectId = HeapTupleGetOid(tuple);
		object.objectSubId = 0;

		performDeletion(&object, behavior);

		found = true;
	}

	systable_endscan(scan);
	heap_close(attrdef_rel, RowExclusiveLock);

	if (complain && !found)
		elog(ERROR, "could not find attrdef tuple for relation %u attnum %d",
			 relid, attnum);
}

/*
 *		RemoveAttrDefaultById
 *
 * Remove a pg_attrdef entry specified by OID.	This is the guts of
 * attribute-default removal.  Note it should be called via performDeletion,
 * not directly.
 */
void
RemoveAttrDefaultById(Oid attrdefId)
{
	Relation	attrdef_rel;
	Relation	attr_rel;
	Relation	myrel;
	ScanKeyData scankeys[1];
	SysScanDesc scan;
	HeapTuple	tuple;
	Oid			myrelid;
	AttrNumber	myattnum;

	/* Grab an appropriate lock on the pg_attrdef relation */
	attrdef_rel = heap_open(AttrDefaultRelationId, RowExclusiveLock);

	/* Find the pg_attrdef tuple */
	ScanKeyInit(&scankeys[0],
				ObjectIdAttributeNumber,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(attrdefId));

	scan = systable_beginscan(attrdef_rel, AttrDefaultOidIndexId, true,
							  SnapshotNow, 1, scankeys);

	tuple = systable_getnext(scan);
	if (!HeapTupleIsValid(tuple))
		elog(ERROR, "could not find tuple for attrdef %u", attrdefId);

	myrelid = ((Form_pg_attrdef) GETSTRUCT(tuple))->adrelid;
	myattnum = ((Form_pg_attrdef) GETSTRUCT(tuple))->adnum;

	/* Get an exclusive lock on the relation owning the attribute */
	myrel = relation_open(myrelid, AccessExclusiveLock);

	/* Now we can delete the pg_attrdef row */
	simple_heap_delete(attrdef_rel, &tuple->t_self);

	systable_endscan(scan);
	heap_close(attrdef_rel, RowExclusiveLock);

	/* Fix the pg_attribute row */
	attr_rel = heap_open(AttributeRelationId, RowExclusiveLock);

	tuple = SearchSysCacheCopy(ATTNUM,
							   ObjectIdGetDatum(myrelid),
							   Int16GetDatum(myattnum),
							   0, 0);
	if (!HeapTupleIsValid(tuple))		/* shouldn't happen */
		elog(ERROR, "cache lookup failed for attribute %d of relation %u",
			 myattnum, myrelid);

	((Form_pg_attribute) GETSTRUCT(tuple))->atthasdef = false;

	simple_heap_update(attr_rel, &tuple->t_self, tuple);

	/* keep the system catalog indexes current */
	CatalogUpdateIndexes(attr_rel, tuple);

	/*
	 * Our update of the pg_attribute row will force a relcache rebuild, so
	 * there's nothing else to do here.
	 */
	heap_close(attr_rel, RowExclusiveLock);

	/* Keep lock on attribute's rel until end of xact */
	relation_close(myrel, NoLock);
}

void
remove_gp_relation_node_and_schedule_drop(Relation rel)
{
	PersistentFileSysRelStorageMgr relStorageMgr;
	
	if (Debug_persistent_print)
		elog(Persistent_DebugPrintLevel(), 
			 "remove_gp_relation_node_and_schedule_drop: dropping relation '%s', relation id %u '%s', relfilenode %u",
			 rel->rd_rel->relname.data,
			 rel->rd_id,
			 relpath(rel->rd_node),
			 rel->rd_rel->relfilenode);

	relStorageMgr = ((RelationIsAoRows(rel) || RelationIsAoCols(rel)) ?
													PersistentFileSysRelStorageMgr_AppendOnly:
													PersistentFileSysRelStorageMgr_BufferPool);

	if (relStorageMgr == PersistentFileSysRelStorageMgr_BufferPool)
	{
		MirroredFileSysObj_ScheduleDropBufferPoolRel(rel);

		DeleteGpRelationNodeTuple(
								rel,
								/* segmentFileNum */ 0);
		
		if (Debug_persistent_print)
			elog(Persistent_DebugPrintLevel(), 
				 "remove_gp_relation_node_and_schedule_drop: For Buffer Pool managed relation '%s' persistent TID %s and serial number " INT64_FORMAT " for DROP",
				 relpath(rel->rd_node),
				 ItemPointerToString(&rel->rd_segfile0_relationnodeinfo.persistentTid),
				 rel->rd_segfile0_relationnodeinfo.persistentSerialNum);
	}
	else
	{
		Relation relNodeRelation;

		GpRelationNodeScan	gpRelationNodeScan;
		
		HeapTuple tuple;
		
		int32 segmentFileNum;
		
		ItemPointerData persistentTid;
		int64 persistentSerialNum;
		
		relNodeRelation = heap_open(GpRelationNodeRelationId, RowExclusiveLock);

		GpRelationNodeBeginScan(
						SnapshotNow,
						relNodeRelation,
						rel->rd_id,
						rel->rd_rel->reltablespace,
						rel->rd_rel->relfilenode,
						&gpRelationNodeScan);
		
		while ((tuple = GpRelationNodeGetNext(
								&gpRelationNodeScan,
								&segmentFileNum,
								&persistentTid,
								&persistentSerialNum)))
		{
			if (Debug_persistent_print)
				elog(Persistent_DebugPrintLevel(), 
					 "remove_gp_relation_node_and_schedule_drop: For Append-Only relation %u relfilenode %u scanned segment file #%d, serial number " INT64_FORMAT " at TID %s for DROP",
					 rel->rd_id,
					 rel->rd_rel->relfilenode,
					 segmentFileNum,
					 persistentSerialNum,
					 ItemPointerToString(&persistentTid));
			
			simple_heap_delete(relNodeRelation, &tuple->t_self);
			
			MirroredFileSysObj_ScheduleDropAppendOnlyFile(
											&rel->rd_node,
											segmentFileNum,
											rel->rd_rel->relname.data,
											&persistentTid,
											persistentSerialNum);
		}
		
		GpRelationNodeEndScan(&gpRelationNodeScan);
		
		heap_close(relNodeRelation, RowExclusiveLock);
	}
}

/*
 * heap_drop_with_catalog	- removes specified relation from catalogs
 *
 * Note that this routine is not responsible for dropping objects that are
 * linked to the pg_class entry via dependencies (for example, indexes and
 * constraints).  Those are deleted by the dependency-tracing logic in
 * dependency.c before control gets here.  In general, therefore, this routine
 * should never be called directly; go through performDeletion() instead.
 */
void
heap_drop_with_catalog(Oid relid)
{
	Relation	rel;
	bool		is_part_child = false;
	bool		is_appendonly_rel;
	bool		is_external_rel;
	char		relkind;

	/*
	 * Open and lock the relation.
	 */
	rel = relation_open(relid, AccessExclusiveLock);

	relkind = rel->rd_rel->relkind;

	is_appendonly_rel = (RelationIsAoRows(rel) || RelationIsAoCols(rel));
	is_external_rel = RelationIsExternal(rel);

	/*
	 * There can no longer be anyone *else* touching the relation, but we
	 * might still have open queries or cursors, or pending trigger events,
	 * in our own session.
	 */
	CheckTableNotInUse(rel, "DROP TABLE");

	/*
	 * Schedule unlinking of the relation's physical file at commit.
	 */
	if (relkind != RELKIND_VIEW &&
		relkind != RELKIND_COMPOSITE_TYPE &&
		!RelationIsExternal(rel))
	{
		remove_gp_relation_node_and_schedule_drop(rel);
	}

	/*
	 * Close relcache entry, but *keep* AccessExclusiveLock (unless this is
	 * a child partition) on the relation until transaction commit.  This
	 * ensures no one else will try to do something with the doomed relation.
	 */
	is_part_child = !rel_needs_long_lock(RelationGetRelid(rel));
	if (is_part_child)
		relation_close(rel, AccessExclusiveLock);
	else
		relation_close(rel, NoLock);

	/*
	 * Forget any ON COMMIT action for the rel
	 */
	remove_on_commit_action(relid);

	/*
	 * Flush the relation from the relcache.  We want to do this before
	 * starting to remove catalog entries, just to be certain that no relcache
	 * entry rebuild will happen partway through.  (That should not really
	 * matter, since we don't do CommandCounterIncrement here, but let's be
	 * safe.)
	 */
	RelationForgetRelation(relid);

	/*
	 * remove inheritance information
	 */
	RelationRemoveInheritance(relid);

	/*
	 * remove partitioning configuration
	 */
	RemovePartitioning(relid);

	/*
	 * delete statistics
	 */
	RemoveStatistics(relid, 0);

	/*
	 * delete attribute tuples
	 */
	DeleteAttributeTuples(relid);

	/*
	 * delete relation tuple
	 */
	DeleteRelationTuple(relid);

	/*
	 * delete error log file
	 */
	ErrorLogDelete(MyDatabaseId, relid);

	/*
	 * append-only table? delete the corresponding pg_appendonly tuple
	 */
	if(is_appendonly_rel)
		RemoveAppendonlyEntry(relid);

	/*
	 * External table? If so, delete the pg_exttable tuple.
	 */
	if (is_external_rel)
		RemoveExtTableEntry(relid);

	/*
	 * Remove distribution policy, if any.
 	 */
	if (relkind == RELKIND_RELATION)
		GpPolicyRemove(relid);

	/*
	 * Attribute encoding
	 */
	if (relkind == RELKIND_RELATION)
		RemoveAttributeEncodingsByRelid(relid);

	/* MPP-6929: metadata tracking */
	MetaTrackDropObject(RelationRelationId,
						relid);

}


/*
 * Store a default expression for column attnum of relation rel.
 */
void
StoreAttrDefault(Relation rel, AttrNumber attnum, Node *expr)
{
	char	   *adbin;
	char	   *adsrc;
	Relation	adrel;
	HeapTuple	tuple;
	Datum		values[4];
	static bool nulls[4] = {false, false, false, false};
	Relation	attrrel;
	HeapTuple	atttup;
	Form_pg_attribute attStruct;
	Oid			attrdefOid;
	ObjectAddress colobject,
				defobject;

	/*
	 * Flatten expression to string form for storage.
	 */
	adbin = nodeToString(expr);

	/*
	 * Also deparse it to form the mostly-obsolete adsrc field.
	 */
	adsrc = deparse_expression(expr,
							deparse_context_for(RelationGetRelationName(rel),
												RelationGetRelid(rel)),
							   false, false);

	/*
	 * Make the pg_attrdef entry.
	 */
	values[Anum_pg_attrdef_adrelid - 1] = RelationGetRelid(rel);
	values[Anum_pg_attrdef_adnum - 1] = attnum;
	values[Anum_pg_attrdef_adbin - 1] = CStringGetTextDatum(adbin);
	values[Anum_pg_attrdef_adsrc - 1] = CStringGetTextDatum(adsrc);

	adrel = heap_open(AttrDefaultRelationId, RowExclusiveLock);

	// Fetch gp_persistent_relation_node information that will be added to XLOG record.
	RelationFetchGpRelationNodeForXLog(adrel);

	tuple = heap_form_tuple(adrel->rd_att, values, nulls);

	attrdefOid = simple_heap_insert(adrel, tuple);

	CatalogUpdateIndexes(adrel, tuple);

	defobject.classId = AttrDefaultRelationId;
	defobject.objectId = attrdefOid;
	defobject.objectSubId = 0;

	heap_close(adrel, RowExclusiveLock);

	/* now can free some of the stuff allocated above */
	pfree(DatumGetPointer(values[Anum_pg_attrdef_adbin - 1]));
	pfree(DatumGetPointer(values[Anum_pg_attrdef_adsrc - 1]));
	heap_freetuple(tuple);
	pfree(adbin);
	pfree(adsrc);

	/*
	 * Update the pg_attribute entry for the column to show that a default
	 * exists.
	 */
	attrrel = heap_open(AttributeRelationId, RowExclusiveLock);
	atttup = SearchSysCacheCopy(ATTNUM,
								ObjectIdGetDatum(RelationGetRelid(rel)),
								Int16GetDatum(attnum),
								0, 0);
	if (!HeapTupleIsValid(atttup))
		elog(ERROR, "cache lookup failed for attribute %d of relation %u",
			 attnum, RelationGetRelid(rel));
	attStruct = (Form_pg_attribute) GETSTRUCT(atttup);
	if (!attStruct->atthasdef)
	{
		attStruct->atthasdef = true;
		simple_heap_update(attrrel, &atttup->t_self, atttup);
		/* keep catalog indexes current */
		CatalogUpdateIndexes(attrrel, atttup);
	}
	heap_close(attrrel, RowExclusiveLock);
	heap_freetuple(atttup);

	/*
	 * Make a dependency so that the pg_attrdef entry goes away if the column
	 * (or whole table) is deleted.
	 */
	colobject.classId = RelationRelationId;
	colobject.objectId = RelationGetRelid(rel);
	colobject.objectSubId = attnum;

	recordDependencyOn(&defobject, &colobject, DEPENDENCY_AUTO);

	/*
	 * Record dependencies on objects used in the expression, too.
	 */
	recordDependencyOnExpr(&defobject, expr, NIL, DEPENDENCY_NORMAL);
}

/*
 * Store a check-constraint expression for the given relation.
 * The expression must be presented as a nodeToString() string.
 *
 * Caller is responsible for updating the count of constraints
 * in the pg_class entry for the relation.
 */
static void
StoreRelCheck(Relation rel, char *ccname, char *ccbin)
{
	Node	   *expr;
	char	   *ccsrc;
	List	   *varList;
	int			keycount;
	int16	   *attNos;

	/*
	 * Convert condition to an expression tree.
	 */
	expr = stringToNode(ccbin);

	/*
	 * deparse it
	 */
	ccsrc = deparse_expression(expr,
							deparse_context_for(RelationGetRelationName(rel),
												RelationGetRelid(rel)),
							   false, false);

	/*
	 * Find columns of rel that are used in ccbin
	 *
	 * NB: pull_var_clause is okay here only because we don't allow subselects
	 * in check constraints; it would fail to examine the contents of
	 * subselects.
	 */
	varList = pull_var_clause(expr, false);
	keycount = list_length(varList);

	if (keycount > 0)
	{
		ListCell   *vl;
		int			i = 0;

		attNos = (int16 *) palloc(keycount * sizeof(int16));
		foreach(vl, varList)
		{
			Var		   *var = (Var *) lfirst(vl);
			int			j;

			for (j = 0; j < i; j++)
				if (attNos[j] == var->varattno)
					break;
			if (j == i)
				attNos[i++] = var->varattno;
		}
		keycount = i;
	}
	else
		attNos = NULL;

	/*
	 * Create the Check Constraint
	 */
	CreateConstraintEntry(ccname,		/* Constraint Name */
						  RelationGetNamespace(rel),	/* namespace */
						  CONSTRAINT_CHECK,		/* Constraint Type */
						  false,	/* Is Deferrable */
						  false,	/* Is Deferred */
						  RelationGetRelid(rel),		/* relation */
						  attNos,		/* attrs in the constraint */
						  keycount,		/* # attrs in the constraint */
						  InvalidOid,	/* not a domain constraint */
						  InvalidOid,	/* Foreign key fields */
						  NULL,
						  NULL,
						  NULL,
						  NULL,
						  0,
						  ' ',
						  ' ',
						  ' ',
						  InvalidOid,	/* no associated index */
						  expr, /* Tree form check constraint */
						  ccbin,	/* Binary form check constraint */
						  ccsrc);		/* Source form check constraint */

	pfree(ccsrc);
}

/*
 * AddRelationConstraints
 *
 * Add both raw (not-yet-transformed) and cooked column default expressions and/or
 * constraint check expressions to an existing relation. This is defined to do both
 * for efficiency in DefineRelation, but of course you can do just one or
 * the other by passing empty lists.
 *
 * rel: relation to be modified
 * colDefaults: list of ColumnDef nodes
 * constraints: list of Constraint nodes
 *
 * All entries in colDefaults will be processed.  Entries in constraints
 * will be processed only if they are CONSTR_CHECK type.
 *
 * Returns a list of CookedConstraint nodes that shows the cooked form of
 * the default and constraint expressions added to the relation.
 *
 * NB: caller should have opened rel with AccessExclusiveLock, and should
 * hold that lock till end of transaction.	Also, we assume the caller has
 * done a CommandCounterIncrement if necessary to make the relation's catalog
 * tuples visible.
 */
List *
AddRelationConstraints(Relation rel,
						  List *colDefaults,
						  List *constraints)
{
	List	   *cookedConstraints = NIL;
	TupleDesc	tupleDesc;
	TupleConstr *oldconstr;
	int			numoldchecks;
	ParseState *pstate;
	RangeTblEntry *rte;
	int			numchecks;
	List	   *checknames;
	ListCell   *cell;
	Node	   *expr;
	CookedConstraint *cooked;

	/*
	 * Get info about existing constraints.
	 */
	tupleDesc = RelationGetDescr(rel);
	oldconstr = tupleDesc->constr;
	if (oldconstr)
		numoldchecks = oldconstr->num_check;
	else
		numoldchecks = 0;

	/*
	 * Create a dummy ParseState and insert the target relation as its sole
	 * rangetable entry.  We need a ParseState for transformExpr.
	 */
	pstate = make_parsestate(NULL);
	rte = addRangeTableEntryForRelation(pstate,
										rel,
										NULL,
										false,
										true);
	addRTEtoQuery(pstate, rte, true, true, true);

	/*
	 * Process column default expressions.
	 */
	foreach(cell, colDefaults)
	{
		ColumnDef  *colDef = (ColumnDef *) lfirst(cell);

		Form_pg_attribute atp = rel->rd_att->attrs[colDef->attnum - 1];

		if (colDef->raw_default != NULL)
		{
			Insist (colDef->cooked_default == NULL);
			expr = cookDefault(pstate, colDef->raw_default,
							   atp->atttypid, atp->atttypmod,
							   NameStr(atp->attname));
		}
		else
		{
			Insist (colDef->cooked_default != NULL);
			expr = stringToNode(colDef->cooked_default);
		}

		/*
		 * If the expression is just a NULL constant, we do not bother to make
		 * an explicit pg_attrdef entry, since the default behavior is
		 * equivalent.
		 *
		 * Note a nonobvious property of this test: if the column is of a
		 * domain type, what we'll get is not a bare null Const but a
		 * CoerceToDomain expr, so we will not discard the default.  This is
		 * critical because the column default needs to be retained to
		 * override any default that the domain might have.
		 */
		if (expr == NULL ||
			(IsA(expr, Const) &&((Const *) expr)->constisnull))
			continue;

		StoreAttrDefault(rel, colDef->attnum, expr);

		cooked = (CookedConstraint *) palloc(sizeof(CookedConstraint));
		cooked->contype = CONSTR_DEFAULT;
		cooked->name = NULL;
		cooked->attnum = colDef->attnum;
		cooked->expr = expr;
		cookedConstraints = lappend(cookedConstraints, cooked);
	}

	/*
	 * Process constraint expressions.
	 */
	numchecks = numoldchecks;
	checknames = NIL;
	foreach(cell, constraints)
	{
		Constraint *cdef = (Constraint *) lfirst(cell);
		char	   *ccname;

		if (cdef->contype != CONSTR_CHECK)
			continue;

		if (cdef->raw_expr != NULL)
		{
			Assert(cdef->cooked_expr == NULL);

			/*
			 * Transform raw parsetree to executable expression, and verify
			 * it's valid as a CHECK constraint.
			 */
			expr = cookConstraint(pstate, cdef->raw_expr,
								  RelationGetRelationName(rel));
		}
		else
		{
			Assert(cdef->cooked_expr != NULL);

			/*
			 * Here, we assume the parser will only pass us valid CHECK
			 * expressions, so we do no particular checking.
			 */
			expr = stringToNode(cdef->cooked_expr);
		}

		/*
		 * Check name uniqueness, or generate a name if none was given.
		 */
		if (cdef->name != NULL)
		{
			ListCell   *cell2;

			ccname = cdef->name;
			/* Check against pre-existing constraints */
			if (ConstraintNameIsUsed(CONSTRAINT_RELATION,
									 RelationGetRelid(rel),
									 RelationGetNamespace(rel),
									 ccname))
				ereport(ERROR,
						(errcode(ERRCODE_DUPLICATE_OBJECT),
				errmsg("constraint \"%s\" for relation \"%s\" already exists",
					   ccname, RelationGetRelationName(rel))));
			/* Check against other new constraints */
			/* Needed because we don't do CommandCounterIncrement in loop */
			foreach(cell2, checknames)
			{
				if (strcmp((char *) lfirst(cell2), ccname) == 0)
					ereport(ERROR,
							(errcode(ERRCODE_DUPLICATE_OBJECT),
							 errmsg("check constraint \"%s\" already exists",
									ccname)));
			}
		}
		else
		{
			/*
			 * When generating a name, we want to create "tab_col_check" for a
			 * column constraint and "tab_check" for a table constraint.  We
			 * no longer have any info about the syntactic positioning of the
			 * constraint phrase, so we approximate this by seeing whether the
			 * expression references more than one column.	(If the user
			 * played by the rules, the result is the same...)
			 *
			 * Note: pull_var_clause() doesn't descend into sublinks, but we
			 * eliminated those above; and anyway this only needs to be an
			 * approximate answer.
			 */
			List	   *vars;
			char	   *colname;

			vars = pull_var_clause(expr, false);

			/* eliminate duplicates */
			vars = list_union(NIL, vars);

			if (list_length(vars) == 1)
				colname = get_attname(RelationGetRelid(rel),
									  ((Var *) linitial(vars))->varattno);
			else
				colname = NULL;

			ccname = ChooseConstraintName(RelationGetRelationName(rel),
										  colname,
										  "check",
										  RelationGetNamespace(rel),
										  checknames);
		}

		/* save name for future checks */
		checknames = lappend(checknames, ccname);

		/*
		 * OK, store it.
		 */
		StoreRelCheck(rel, ccname, nodeToString(expr));

		numchecks++;

		cooked = (CookedConstraint *) palloc(sizeof(CookedConstraint));
		cooked->contype = CONSTR_CHECK;
		cooked->name = ccname;
		cooked->attnum = 0;
		cooked->expr = expr;
		cookedConstraints = lappend(cookedConstraints, cooked);
	}

	/* Cleanup the parse state */
	free_parsestate(pstate);

	/*
	 * Update the count of constraints in the relation's pg_class tuple. We do
	 * this even if there was no change, in order to ensure that an SI update
	 * message is sent out for the pg_class tuple, which will force other
	 * backends to rebuild their relcache entries for the rel. (This is
	 * critical if we added defaults but not constraints.)
	 */
	SetRelationNumChecks(rel, numchecks);

	return cookedConstraints;
}

/*
 * Transform raw parsetree to executable expression.
 */
static Node*
cookConstraint (ParseState 	*pstate,
				Node 		*raw_constraint,
				char		*relname)
{
	Node	*expr;

	/* Transform raw parsetree to executable expression. */
	expr = transformExpr(pstate, raw_constraint);

	/* Make sure it yields a boolean result. */
	expr = coerce_to_boolean(pstate, expr, "CHECK");

	/* Make sure no outside relations are referred to. */
	if (list_length(pstate->p_rtable) != 1)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_COLUMN_REFERENCE),
		errmsg("only table \"%s\" can be referenced in check constraint",
			   relname)));
	/*
	 * No subplans or aggregates, either...
	 */
	if (pstate->p_hasSubLinks)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("cannot use subquery in check constraint")));
	if (pstate->p_hasAggs)
		ereport(ERROR,
				(errcode(ERRCODE_GROUPING_ERROR),
		   errmsg("cannot use aggregate function in check constraint")));
	if (pstate->p_hasWindFuncs)
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
		   errmsg("cannot use window function in check constraint")));

	return expr;
}


/*
 * Update the count of constraints in the relation's pg_class tuple.
 *
 * Caller had better hold exclusive lock on the relation.
 *
 * An important side effect is that a SI update message will be sent out for
 * the pg_class tuple, which will force other backends to rebuild their
 * relcache entries for the rel.  Also, this backend will rebuild its
 * own relcache entry at the next CommandCounterIncrement.
 */
void
SetRelationNumChecks(Relation rel, int numchecks)
{
	Relation	relrel;
	HeapTuple	reltup;
	Form_pg_class relStruct;

	relrel = heap_open(RelationRelationId, RowExclusiveLock);
	reltup = SearchSysCacheCopy(RELOID,
								ObjectIdGetDatum(RelationGetRelid(rel)),
								0, 0, 0);
	if (!HeapTupleIsValid(reltup))
		elog(ERROR, "cache lookup failed for relation %u",
			 RelationGetRelid(rel));
	relStruct = (Form_pg_class) GETSTRUCT(reltup);

	if (relStruct->relchecks != numchecks)
	{
		relStruct->relchecks = numchecks;

		simple_heap_update(relrel, &reltup->t_self, reltup);

		/* keep catalog indexes current */
		CatalogUpdateIndexes(relrel, reltup);
	}
	else
	{
		/* Skip the disk update, but force relcache inval anyway */
		CacheInvalidateRelcache(rel);
	}

	heap_freetuple(reltup);
	heap_close(relrel, RowExclusiveLock);
}

/*
 * Take a raw default and convert it to a cooked format ready for
 * storage.
 *
 * Parse state should be set up to recognize any vars that might appear
 * in the expression.  (Even though we plan to reject vars, it's more
 * user-friendly to give the correct error message than "unknown var".)
 *
 * If atttypid is not InvalidOid, coerce the expression to the specified
 * type (and typmod atttypmod).   attname is only needed in this case:
 * it is used in the error message, if any.
 */
Node *
cookDefault(ParseState *pstate,
			Node *raw_default,
			Oid atttypid,
			int32 atttypmod,
			char *attname)
{
	Node	   *expr;

	Assert(raw_default != NULL);

	/*
	 * Transform raw parsetree to executable expression.
	 */
	expr = transformExpr(pstate, raw_default);

	/*
	 * Make sure default expr does not refer to any vars.
	 */
	if (contain_var_clause(expr))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_COLUMN_REFERENCE),
			  errmsg("cannot use column references in default expression")));

	/*
	 * It can't return a set either.
	 */
	if (expression_returns_set(expr))
		ereport(ERROR,
				(errcode(ERRCODE_DATATYPE_MISMATCH),
				 errmsg("default expression must not return a set")));

	/*
	 * No subplans or aggregates, either...
	 */
	if (pstate->p_hasSubLinks)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("cannot use subquery in default expression")));
	if (pstate->p_hasAggs)
		ereport(ERROR,
				(errcode(ERRCODE_GROUPING_ERROR),
			 errmsg("cannot use aggregate function in default expression")));
	if (pstate->p_hasWindFuncs)
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
			 errmsg("cannot use window function in default expression")));

	/*
	 * Coerce the expression to the correct type and typmod, if given. This
	 * should match the parser's processing of non-defaulted expressions ---
	 * see transformAssignedExpr().
	 */
	if (OidIsValid(atttypid))
	{
		Oid			type_id = exprType(expr);

		expr = coerce_to_target_type(pstate, expr, type_id,
									 atttypid, atttypmod,
									 COERCION_ASSIGNMENT,
									 COERCE_IMPLICIT_CAST,
									 -1);
		if (expr == NULL)
			ereport(ERROR,
					(errcode(ERRCODE_DATATYPE_MISMATCH),
					 errmsg("column \"%s\" is of type %s"
							" but default expression is of type %s",
							attname,
							format_type_be(atttypid),
							format_type_be(type_id)),
			   errhint("You will need to rewrite or cast the expression.")));
	}

	return expr;
}


/*
 * Removes all constraints on a relation that match the given name.
 *
 * It is the responsibility of the calling function to acquire a suitable
 * lock on the relation.
 *
 * Returns: The number of constraints removed.
 */
int
RemoveRelConstraints(Relation rel, const char *constrName,
					 DropBehavior behavior)
{
	int			ndeleted = 0;
	Relation	conrel;
	SysScanDesc conscan;
	ScanKeyData key[1];
	HeapTuple	contup;

	/* Grab an appropriate lock on the pg_constraint relation */
	conrel = heap_open(ConstraintRelationId, RowExclusiveLock);

	/* Use the index to scan only constraints of the target relation */
	ScanKeyInit(&key[0],
				Anum_pg_constraint_conrelid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(RelationGetRelid(rel)));

	conscan = systable_beginscan(conrel, ConstraintRelidIndexId, true,
								 SnapshotNow, 1, key);

	/*
	 * Scan over the result set, removing any matching entries.
	 */
	while ((contup = systable_getnext(conscan)) != NULL)
	{
		Form_pg_constraint con = (Form_pg_constraint) GETSTRUCT(contup);

		if (strcmp(NameStr(con->conname), constrName) == 0)
		{
			ObjectAddress conobj;

			conobj.classId = ConstraintRelationId;
			conobj.objectId = HeapTupleGetOid(contup);
			conobj.objectSubId = 0;

			performDeletion(&conobj, behavior);

			ndeleted++;
		}
	}

	/* Clean up after the scan */
	systable_endscan(conscan);
	heap_close(conrel, RowExclusiveLock);

	return ndeleted;
}


/*
 * RemoveStatistics --- remove entries in pg_statistic for a rel or column
 *
 * If attnum is zero, remove all entries for rel; else remove only the one
 * for that column.
 */
void
RemoveStatistics(Oid relid, AttrNumber attnum)
{
	Relation	pgstatistic;
	SysScanDesc scan;
	ScanKeyData key[2];
	int			nkeys;
	HeapTuple	tuple;

	pgstatistic = heap_open(StatisticRelationId, RowExclusiveLock);

	ScanKeyInit(&key[0],
				Anum_pg_statistic_starelid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(relid));

	if (attnum == 0)
		nkeys = 1;
	else
	{
		ScanKeyInit(&key[1],
					Anum_pg_statistic_staattnum,
					BTEqualStrategyNumber, F_INT2EQ,
					Int16GetDatum(attnum));
		nkeys = 2;
	}

	scan = systable_beginscan(pgstatistic, StatisticRelidAttnumIndexId, true,
							  SnapshotNow, nkeys, key);

	while (HeapTupleIsValid(tuple = systable_getnext(scan)))
		simple_heap_delete(pgstatistic, &tuple->t_self);

	systable_endscan(scan);

	heap_close(pgstatistic, RowExclusiveLock);
}


/*
 * RelationTruncateIndexes - truncate all indexes associated
 * with the heap relation to zero tuples.
 *
 * The routine will truncate and then reconstruct the indexes on
 * the specified relation.	Caller must hold exclusive lock on rel.
 */
static void
RelationTruncateIndexes(Relation heapRelation)
{
	ListCell   *indlist;

	/* Ask the relcache to produce a list of the indexes of the rel */
	foreach(indlist, RelationGetIndexList(heapRelation))
	{
		Oid			indexId = lfirst_oid(indlist);
		Relation	currentIndex;
		IndexInfo  *indexInfo;

		/* Open the index relation; use exclusive lock, just to be sure */
		currentIndex = index_open(indexId, AccessExclusiveLock);

		/* Fetch info needed for index_build */
		indexInfo = BuildIndexInfo(currentIndex);

		/* Now truncate the actual file (and discard buffers) */
		RelationTruncate(
					currentIndex, 
					0,
					/* markPersistentAsPhysicallyTruncated */ true);

		/* Initialize the index and rebuild */
		/* Note: we do not need to re-establish pkey setting */
		index_build(heapRelation, currentIndex, indexInfo, false, true);

		/* We're done with this index */
		index_close(currentIndex, NoLock);
	}
}

/*
 *	 heap_truncate
 *
 *	 This routine deletes all data within all the specified relations.
 *
 * This is not transaction-safe!  There is another, transaction-safe
 * implementation in commands/tablecmds.c.	We now use this only for
 * ON COMMIT truncation of temporary tables, where it doesn't matter.
 */
void
heap_truncate(List *relids)
{
	List	   *relations = NIL;
	ListCell   *cell;

	/* Open relations for processing, and grab exclusive access on each */
	foreach(cell, relids)
	{
		Oid			rid = lfirst_oid(cell);
		Relation	rel;
		Oid			toastrelid;

		rel = heap_open(rid, AccessExclusiveLock);
		relations = lappend(relations, rel);

		/* If there is a toast table, add it to the list too */
		toastrelid = rel->rd_rel->reltoastrelid;
		if (OidIsValid(toastrelid))
		{
			Relation trel;

			trel = heap_open(toastrelid, AccessExclusiveLock);
			relations = lappend(relations, trel);
		}
	}

	/* Don't allow truncate on tables that are referenced by foreign keys */
	heap_truncate_check_FKs(relations, true);

	/* OK to do it */
	foreach(cell, relations)
	{
		Relation	rel = lfirst(cell);

		/*
		 * Truncating AO and auxiliary tables' relfiles, like in case
		 * of heap tables, leaves the AO table in an inconsistent
		 * state at the end of commit:
		 *
		 *    - The aoseg table indicates no segfiles on disk.
		 *    - AO segment files are truncated, with EOF = 0.
		 *    - The EOF recorded in persistent tables for the AO
		 *      segment files, however, is greater than 0 if the
		 *      transaction inserted tuples in the AO table.
		 *
		 * One may think of resetting the EOF in persistent tables to
		 * 0.  Beware, EOF of AO segfiles can only increase.  Filerep
		 * incremental recovery relies on this assumption.
		 *
		 * Therefore, ON COMMIT DELETE ROWS action for AO tables is
		 * implemented by creating new segment file and scheduling
		 * existing one for drop at the end of commit.  TRUNCATE TABLE
		 * command works the same way.  At some point, all temporary
		 * tables should be exempt from persistent tables/lifecycle
		 * management.
		 */

		if (RelationIsAoRows(rel) || RelationIsAoCols(rel))
		{
			TruncateRelfiles(rel);
			reindex_relation(RelationGetRelid(rel), true);
		}
		else
		{
			/* Truncate the actual file (and discard buffers) */
			RelationTruncate(
					rel,
					0,
					/* markPersistentAsPhysicallyTruncated */ false);

			/* If this relation has indexes, truncate the indexes too */
			RelationTruncateIndexes(rel);
		}
		/*
		 * Close the relation, but keep exclusive lock on it until commit.
		 */
		heap_close(rel, NoLock);
	}
}

/*
 * heap_truncate_check_FKs
 *		Check for foreign keys referencing a list of relations that
 *		are to be truncated, and raise error if there are any
 *
 * We disallow such FKs (except self-referential ones) since the whole point
 * of TRUNCATE is to not scan the individual rows to be thrown away.
 *
 * This is split out so it can be shared by both implementations of truncate.
 * Caller should already hold a suitable lock on the relations.
 *
 * tempTables is only used to select an appropriate error message.
 */
void
heap_truncate_check_FKs(List *relations, bool tempTables)
{
	List	   *oids = NIL;
	List	   *dependents;
	ListCell   *cell;

	/*
	 * Build a list of OIDs of the interesting relations.
	 *
	 * If a relation has no triggers, then it can neither have FKs nor be
	 * referenced by a FK from another table, so we can ignore it.
	 */
	foreach(cell, relations)
	{
		Relation	rel = lfirst(cell);

		if (rel->rd_rel->reltriggers != 0)
			oids = lappend_oid(oids, RelationGetRelid(rel));
	}

	/*
	 * Fast path: if no relation has triggers, none has FKs either.
	 */
	if (oids == NIL)
		return;

	/*
	 * Otherwise, must scan pg_constraint.	We make one pass with all the
	 * relations considered; if this finds nothing, then all is well.
	 */
	dependents = heap_truncate_find_FKs(oids);
	if (dependents == NIL)
		return;

	/*
	 * Otherwise we repeat the scan once per relation to identify a particular
	 * pair of relations to complain about.  This is pretty slow, but
	 * performance shouldn't matter much in a failure path.  The reason for
	 * doing things this way is to ensure that the message produced is not
	 * dependent on chance row locations within pg_constraint.
	 */
	foreach(cell, oids)
	{
		Oid			relid = lfirst_oid(cell);
		ListCell   *cell2;

		dependents = heap_truncate_find_FKs(list_make1_oid(relid));

		foreach(cell2, dependents)
		{
			Oid			relid2 = lfirst_oid(cell2);

			if (!list_member_oid(oids, relid2))
			{
				char	   *relname = get_rel_name(relid);
				char	   *relname2 = get_rel_name(relid2);

				if (tempTables)
					ereport(ERROR,
							(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
							 errmsg("unsupported ON COMMIT and foreign key combination"),
							 errdetail("Table \"%s\" references \"%s\", but they do not have the same ON COMMIT setting.",
									   relname2, relname)));
				else
					ereport(ERROR,
							(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
							 errmsg("cannot truncate a table referenced in a foreign key constraint"),
							 errdetail("Table \"%s\" references \"%s\".",
									   relname2, relname),
						   errhint("Truncate table \"%s\" at the same time, "
								   "or use TRUNCATE ... CASCADE.",
								   relname2)));
			}
		}
	}
}

/*
 * heap_truncate_find_FKs
 *		Find relations having foreign keys referencing any of the given rels
 *
 * Input and result are both lists of relation OIDs.  The result contains
 * no duplicates, does *not* include any rels that were already in the input
 * list, and is sorted in OID order.  (The last property is enforced mainly
 * to guarantee consistent behavior in the regression tests; we don't want
 * behavior to change depending on chance locations of rows in pg_constraint.)
 *
 * Note: caller should already have appropriate lock on all rels mentioned
 * in relationIds.	Since adding or dropping an FK requires exclusive lock
 * on both rels, this ensures that the answer will be stable.
 */
List *
heap_truncate_find_FKs(List *relationIds)
{
	List	   *result = NIL;
	Relation	fkeyRel;
	SysScanDesc fkeyScan;
	HeapTuple	tuple;

	/*
	 * Must scan pg_constraint.  Right now, it is a seqscan because there is
	 * no available index on confrelid.
	 */
	fkeyRel = heap_open(ConstraintRelationId, AccessShareLock);

	fkeyScan = systable_beginscan(fkeyRel, InvalidOid, false,
								  SnapshotNow, 0, NULL);

	while (HeapTupleIsValid(tuple = systable_getnext(fkeyScan)))
	{
		Form_pg_constraint con = (Form_pg_constraint) GETSTRUCT(tuple);

		/* Not a foreign key */
		if (con->contype != CONSTRAINT_FOREIGN)
			continue;

		/* Not referencing one of our list of tables */
		if (!list_member_oid(relationIds, con->confrelid))
			continue;

		/* Add referencer unless already in input or result list */
		if (!list_member_oid(relationIds, con->conrelid))
			result = insert_ordered_unique_oid(result, con->conrelid);
	}

	systable_endscan(fkeyScan);
	heap_close(fkeyRel, AccessShareLock);

	return result;
}

/*
 * insert_ordered_unique_oid
 *		Insert a new Oid into a sorted list of Oids, preserving ordering,
 *		and eliminating duplicates
 *
 * Building the ordered list this way is O(N^2), but with a pretty small
 * constant, so for the number of entries we expect it will probably be
 * faster than trying to apply qsort().  It seems unlikely someone would be
 * trying to truncate a table with thousands of dependent tables ...
 */
static List *
insert_ordered_unique_oid(List *list, Oid datum)
{
	ListCell   *prev;

	/* Does the datum belong at the front? */
	if (list == NIL || datum < linitial_oid(list))
		return lcons_oid(datum, list);
	/* Does it match the first entry? */
	if (datum == linitial_oid(list))
		return list;			/* duplicate, so don't insert */
	/* No, so find the entry it belongs after */
	prev = list_head(list);
	for (;;)
	{
		ListCell   *curr = lnext(prev);

		if (curr == NULL || datum < lfirst_oid(curr))
			break;				/* it belongs after 'prev', before 'curr' */

		if (datum == lfirst_oid(curr))
			return list;		/* duplicate, so don't insert */

		prev = curr;
	}
	/* Insert datum into list after 'prev' */
	lappend_cell_oid(list, prev, datum);
	return list;
}

bool
should_have_valid_relfrozenxid(Oid oid, char relkind, char relstorage)
{
	switch (relkind)
	{
		case RELKIND_RELATION:
			if (relstorage == RELSTORAGE_EXTERNAL ||
				relstorage == RELSTORAGE_FOREIGN  ||
				relstorage == RELSTORAGE_VIRTUAL ||
				relstorage == RELSTORAGE_AOROWS ||
				relstorage == RELSTORAGE_AOCOLS)
			{
				return false;
			}

			/* Persistent tables' always store tuples with forzenXid. */
			if (GpPersistent_IsPersistentRelation(oid))
				return false;

			return true;

		case RELKIND_TOASTVALUE:
		case RELKIND_AOSEGMENTS:
		case RELKIND_AOBLOCKDIR:
		case RELKIND_AOVISIMAP:
			return true;
	}

	return false;
}
