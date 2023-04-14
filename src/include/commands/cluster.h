/*-------------------------------------------------------------------------
 *
 * cluster.h
 *	  header file for postgres cluster command stuff
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994-5, Regents of the University of California
 *
 * src/include/commands/cluster.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef CLUSTER_H
#define CLUSTER_H

#include "nodes/parsenodes.h"
#include "storage/lock.h"
#include "utils/relcache.h"


extern void cluster(ClusterStmt *stmt, bool isTopLevel);
extern bool cluster_rel(Oid tableOid, Oid indexOid, int options,
						bool printError);
extern void check_index_is_clusterable(Relation OldHeap, Oid indexOid,
									   bool recheck, LOCKMODE lockmode);
extern void mark_index_clustered(Relation rel, Oid indexOid, bool is_internal);

extern Oid make_new_heap(Oid OIDOldHeap, Oid NewTableSpace, Oid NewAccessMethod,
						 List *NewEncodings,
						 Datum reloptions,
						 char relpersistence,
						 LOCKMODE lockmode,
						 bool createAoBlockDirectory,
						 bool makeCdbPolicy);
extern Oid make_new_heap_with_colname(Oid OIDOldHeap, Oid NewTableSpace, Oid NewAccessMethod,
									  List *NewEncodings,
									  Datum newoptions,
									  char relpersistence,
									  LOCKMODE lockmode,
									  bool createAoBlockDirectory,
									  bool makeCdbPolicy,
									  char *colprefix);
extern void finish_heap_swap(Oid OIDOldHeap, Oid OIDNewHeap,
							 bool is_system_catalog,
							 bool swap_toast_by_content,
							 bool swap_stats,
							 bool check_constraints,
							 bool is_internal,
							 TransactionId frozenXid,
							 MultiXactId minMulti,
							 char newrelpersistence);

extern void swap_relation_files(Oid r1, Oid r2, bool target_is_pg_class,
					bool swap_toast_by_content,
					bool swap_stats,
					bool is_internal,
					TransactionId frozenXid,
					MultiXactId frozenMulti,
					Oid *mapped_tables);

#endif							/* CLUSTER_H */
