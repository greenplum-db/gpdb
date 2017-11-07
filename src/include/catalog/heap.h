/*-------------------------------------------------------------------------
 *
 * heap.h
 *	  prototypes for functions in backend/catalog/heap.c
 *
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/catalog/heap.h,v 1.88 2008/05/09 23:32:04 tgl Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef HEAP_H
#define HEAP_H

#include "parser/parse_node.h"
#include "catalog/gp_persistent.h"

/*
 * GPDB_84_MERGE_FIXME: the new constraints work in tablecmds use RawColumnDefault
 * which has been removed from GPDB in the past. Re-added for now but perhaps
 * there is a bigger rewrite looming in tablecmds.c
 */
typedef struct RawColumnDefault
{
	AttrNumber	attnum;			/* attribute to attach default to */
	Node	   *raw_default;	/* default value (untransformed parse tree) */
} RawColumnDefault;

typedef struct CookedConstraint
{
	/*
	 * In PostgreSQL, this struct is only during CREATE TABLE processing, but
	 * in GPDB, we create these in the QD and dispatch pre-built
	 * CookedConstraints to the QE nodes, in the CreateStmt. That's why we
	 * need to have a node tag and copy/out/read function support for this
	 * in GPDB.
	 */
	NodeTag		type;
	ConstrType	contype;		/* CONSTR_DEFAULT or CONSTR_CHECK */
	char	   *name;			/* name, or NULL if none */
	AttrNumber	attnum;			/* which attr (only for DEFAULT) */
	Node	   *expr;			/* transformed default or check expr */
	bool		is_local;		/* constraint has local (non-inherited) def */
	int			inhcount;		/* number of times constraint is inherited */
	/*
	 * Remember to update copy/out/read functions if new fields are added
	 * here!
	 */
} CookedConstraint;

extern Relation heap_create(const char *relname,
			Oid relnamespace,
			Oid reltablespace,
			Oid relid,
			TupleDesc tupDesc,
			Oid relam,
			char relkind,
			char relstorage,
			bool shared_relation,
			bool allow_system_table_mods,
			bool bufferPoolBulkLoad);

extern Oid heap_create_with_catalog(const char *relname,
						 Oid relnamespace,
						 Oid reltablespace,
						 Oid relid,
						 Oid ownerid,
						 TupleDesc tupdesc,
						 List *cooked_constraints,
						 Oid relam,
						 char relkind,
						 char relstorage,
						 bool shared_relation,
						 bool oidislocal,
						 bool bufferPoolBulkLoad,
						 int oidinhcount,
						 OnCommitAction oncommit,
                         const struct GpPolicy *policy,    /* MPP */
						 Datum reloptions,
						 bool allow_system_table_mods,
						 bool valid_opts,
						 ItemPointer persistentTid,
						 int64 *persistentSerialNum);

extern void heap_drop_with_catalog(Oid relid);

extern void heap_truncate(List *relids);

extern void heap_truncate_check_FKs(List *relations, bool tempTables);

extern List *heap_truncate_find_FKs(List *relationIds);

extern void InsertPgClassTuple(Relation pg_class_desc,
				   Relation new_rel_desc,
				   Oid new_rel_oid,
				   Datum reloptions);

extern void InsertGpRelationNodeTuple(
	Relation 		gp_relation_node,
	Oid				relationId,
	char			*relname,
	Oid				tablespaceOid,
	Oid				relation,
	int32			segmentFileNum,
	bool			updateIndex,
	ItemPointer		persistentTid,
	int64			persistentSerialNum);
extern void UpdateGpRelationNodeTuple(
		Relation	gp_relation_node,
		HeapTuple	tuple,
		Oid 		tablespaceOid,
		Oid 		relation,
		int32		segmentFileNum,
		ItemPointer persistentTid,
		int64		persistentSerialNum);

extern List *AddRelationNewConstraints(Relation rel,
						  List *newColDefaults,
						  List *newConstraints,
						  bool allow_merge,
						  bool is_local);
extern List *AddRelationConstraints(Relation rel,
						  List *rawColDefaults,
						  List *constraints);

extern void StoreAttrDefault(Relation rel, AttrNumber attnum, Node *expr);

extern Node *cookDefault(ParseState *pstate,
			Node *raw_default,
			Oid atttypid,
			int32 atttypmod,
			char *attname);

extern void DeleteRelationTuple(Oid relid);
extern void DeleteAttributeTuples(Oid relid);
extern void RemoveAttributeById(Oid relid, AttrNumber attnum);
extern void RemoveAttrDefault(Oid relid, AttrNumber attnum,
				  DropBehavior behavior, bool complain);
extern void RemoveAttrDefaultById(Oid attrdefId);
extern void RemoveStatistics(Oid relid, AttrNumber attnum);

extern Form_pg_attribute SystemAttributeDefinition(AttrNumber attno,
						  bool relhasoids);

extern Form_pg_attribute SystemAttributeByName(const char *attname,
					  bool relhasoids);

extern void CheckAttributeNamesTypes(TupleDesc tupdesc, char relkind);

extern void CheckAttributeType(const char *attname, Oid atttypid,
							   List *containing_rowtypes);
extern void SetRelationNumChecks(Relation rel, int numchecks);

/* MPP-6929: metadata tracking */
extern void MetaTrackAddObject(Oid		classid, 
							   Oid		objoid, 
							   Oid		relowner,
							   char*	actionname,
							   char*	subtype);
extern void MetaTrackUpdObject(Oid		classid, 
							   Oid		objoid, 
							   Oid		relowner,
							   char*	actionname,
							   char*	subtype);
extern void MetaTrackDropObject(Oid		classid, 
								Oid		objoid);

#define MetaTrackValidRelkind(relkind) \
		(((relkind) == RELKIND_RELATION) \
		|| ((relkind) == RELKIND_INDEX) \
		|| ((relkind) == RELKIND_SEQUENCE) \
		|| ((relkind) == RELKIND_VIEW)) 

extern void remove_gp_relation_node_and_schedule_drop(Relation rel);
extern bool should_have_valid_relfrozenxid(Oid oid, char relkind, char relstorage);
#endif   /* HEAP_H */
