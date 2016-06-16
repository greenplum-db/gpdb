/*-------------------------------------------------------------------------
 *
 * heap.h
 *	  prototypes for functions in backend/catalog/heap.c
 *
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/catalog/heap.h,v 1.87 2008/01/01 19:45:56 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef HEAP_H
#define HEAP_H

#include "parser/parse_node.h"
#include "catalog/gp_persistent.h"

typedef struct CookedConstraint
{
	ConstrType	contype;		/* CONSTR_DEFAULT or CONSTR_CHECK */
	char	   *name;			/* name, or NULL if none */
	AttrNumber	attnum;			/* which attr (only for DEFAULT) */
	Node	   *expr;			/* transformed default or check expr */
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
						 Oid *comptypeOid, /* MPP */
						 Oid *comptypeArrayOid, /* MPP */
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
	Oid				relation,
	int32			segmentFileNum,
	bool			updateIndex,
	ItemPointer		persistentTid,
	int64			persistentSerialNum);
extern void UpdateGpRelationNodeTuple(
		Relation	gp_relation_node,
		HeapTuple	tuple,
		Oid 		relation,
		int32		segmentFileNum,
		ItemPointer persistentTid,
		int64		persistentSerialNum);

extern List *AddRelationRawConstraints(Relation rel,
						  List *rawColDefaults,
						  List *rawConstraints);
extern List *AddRelationConstraints(Relation rel,
						  List *rawColDefaults,
						  List *constraints);

extern Oid StoreAttrDefault(Relation rel, AttrNumber attnum, Node *expr,
							Oid defoid);

extern Node *cookDefault(ParseState *pstate,
			Node *raw_default,
			Oid atttypid,
			int32 atttypmod,
			char *attname);

extern int RemoveRelConstraints(Relation rel, const char *constrName,
					 DropBehavior behavior);

extern void DeleteRelationTuple(Oid relid);
extern void DeleteAttributeTuples(Oid relid);
extern void RemoveAttributeById(Oid relid, AttrNumber attnum);
extern Oid RemoveAttrDefault(Oid relid, AttrNumber attnum,
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

#endif   /* HEAP_H */
