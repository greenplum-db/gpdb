/*-------------------------------------------------------------------------
 *
 * relationnode.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef RELATIONNODE_H
#define RELATIONNODE_H

#include "access/htup.h"
#include "access/skey.h"
#include "access/tupdesc.h"
#include "nodes/bitmapset.h"
#include "nodes/pg_list.h"
#include "catalog/pg_class.h"
#include "utils/relcache.h"
#include "utils/tqual.h"

typedef struct GpRelationNodeScan
{
	Relation	gp_relation_node;
	Oid		relationId;
	Oid 		tablespaceOid;
	Oid		relfilenode;
	ScanKeyData     scankey[2];
	struct SysScanDescData  *scan;
} GpRelationNodeScan;

extern void
GpRelationNodeBeginScan(
	Snapshot	snapshot,
	Relation 	gp_relation_node,
	Oid		relationId,
	Oid 		tablespaceOid,
	Oid 		relfilenode,
	GpRelationNodeScan 	*gpRelationNodeScan);

extern HeapTuple
GpRelationNodeGetNext(
	GpRelationNodeScan 	*gpRelationNodeScan,
	int32				*segmentFileNum,
	ItemPointer			persistentTid,
	int64				*persistentSerialNum);

extern void
GpRelationNodeEndScan(
	GpRelationNodeScan 	*gpRelationNodeScan);

extern HeapTuple FetchGpRelationNodeTuple(
	Relation 	gp_relation_node,
	Oid 		tablespaceOid,
	Oid 		relationNode,
	int32		segmentFileNum,
	ItemPointer	persistentTid,
	int64		*persistentSerialNum);
extern bool ReadGpRelationNode(
	Oid tablespaceOid,
	Oid 			relfilenode,
	int32			segmentFileNum,
	ItemPointer		persistentTid,
	int64			*persistentSerialNum);
extern void DeleteGpRelationNodeTuple(
	Relation	rel,
	int32           segmentFileNum);
extern void RelationFetchSegFile0GpRelationNode(Relation relation);
extern void RelationFetchGpRelationNodeForXLog_Index(Relation relation);

#endif
