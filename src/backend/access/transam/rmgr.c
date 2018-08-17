/*
 * rmgr.c
 *
 * Resource managers definition
 *
 * src/backend/access/transam/rmgr.c
 */
#include "postgres.h"

#include "access/clog.h"
#include "access/distributedlog.h"
#include "access/gin.h"
#include "access/gist_private.h"
#include "access/hash.h"
#include "access/bitmap.h"
#include "access/heapam.h"
#include "access/multixact.h"
#include "access/nbtree.h"
#include "access/spgist.h"
#include "access/xact.h"
#include "access/xlog_internal.h"
#include "catalog/storage.h"
#include "commands/dbcommands.h"
#include "commands/sequence.h"
#include "commands/tablespace.h"
#include "storage/standby.h"
#include "utils/relmapper.h"

#include "cdb/cdbappendonlyam.h"


const RmgrData RmgrTable[RM_MAX_ID + 1] = {
	{"XLOG", xlog_redo, xlog_desc, NULL, NULL, NULL, NULL},
	{"Transaction", xact_redo, xact_desc, NULL, NULL, NULL, NULL},
	{"Storage", smgr_redo, smgr_desc, NULL, NULL, NULL, NULL},
	{"CLOG", clog_redo, clog_desc, NULL, NULL, NULL, NULL},
	{"Database", dbase_redo, dbase_desc, NULL, NULL, NULL, NULL},
	{"Tablespace", tblspc_redo, tblspc_desc, NULL, NULL, NULL, NULL},
	{"MultiXact", multixact_redo, multixact_desc, NULL, NULL, NULL, NULL},
	{"RelMap", relmap_redo, relmap_desc, NULL, NULL, NULL},
	{"Standby", standby_redo, standby_desc, NULL, NULL, NULL},
	{"Heap2", heap2_redo, heap2_desc, NULL, NULL, NULL, heap_mask},
	{"Heap", heap_redo, heap_desc, NULL, NULL, NULL, heap_mask},
	{"Btree", btree_redo, btree_desc, btree_xlog_startup, btree_xlog_cleanup, btree_safe_restartpoint, btree_mask},
	{"Hash", hash_redo, hash_desc, NULL, NULL, NULL, NULL},
	{"Gin", gin_redo, gin_desc, gin_xlog_startup, gin_xlog_cleanup, gin_safe_restartpoint, gin_mask},
	{"Gist", gist_redo, gist_desc, gist_xlog_startup, gist_xlog_cleanup, NULL, gist_mask},
	{"Sequence", seq_redo, seq_desc, NULL, NULL, NULL, seq_mask},
	{"SPGist", spg_redo, spg_desc, spg_xlog_startup, spg_xlog_cleanup, NULL},
	{"Bitmap", bitmap_redo, bitmap_desc, bitmap_xlog_startup, bitmap_xlog_cleanup, bitmap_safe_restartpoint, NULL},
	{"DistributedLog", DistributedLog_redo, DistributedLog_desc, NULL, NULL, NULL, NULL},
	{"Appendonly Table Log Records", appendonly_redo, appendonly_desc, NULL, NULL, NULL, NULL}
};
