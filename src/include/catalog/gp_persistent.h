/*-------------------------------------------------------------------------
 *
 * gp_persistent.h
 *
 * Portions Copyright (c) 2009-2010, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/catalog/gp_persistent.h
 *
 * Global tables:
 *     gp_persistent_relation_node
 *     gp_persistent_database_node
 *     gp_persistent_tablespace_node
 *     gp_persistent_filespace_node
 *
 * Per database table:
 *     gp_relation_node
 *
 *-------------------------------------------------------------------------
 */
#ifndef GP_PERSISTENT_H
#define GP_PERSISTENT_H

#include "access/htup.h"
#include "access/tupdesc.h"
#include "storage/itemptr.h"
#include "access/persistentfilesysobjname.h"
#include "access/xlogdefs.h"
#include "catalog/indexing.h"
#include "catalog/gp_global_sequence.h"
#include "catalog/gp_id.h"
#include "catalog/pg_shdepend.h"

#define int8 int64
#define tid ItemPointerData
#define gpxlogloc XLogRecPtr

/*
 * Defines for gp_persistent_relation_node table
 */
#define GpPersistentRelationNodeRelationName	"gp_persistent_relation_node"

#define GpPersistentRelationNodeRelationId 5090

// UNDONE: Change reserved column to relation_bufpool_kind in CATALOG
// definition below.  The Anum_* DEFINE below has already been renamed.
CATALOG(gp_persistent_relation_node,5090) BKI_SHARED_RELATION BKI_WITHOUT_OIDS
{
	Oid			tablespace_oid;
	Oid			database_oid;
	Oid			relfilenode_oid;
	int4		segment_file_num;
	int2		relation_storage_manager;
	int2		persistent_state;
	int8		create_mirror_data_loss_tracking_session_num;
	int2		mirror_existence_state;
	int2		mirror_data_synchronization_state;
	bool		mirror_bufpool_marked_for_scan_incremental_resync;
	int8		mirror_bufpool_resync_changed_page_count;
	gpxlogloc	mirror_bufpool_resync_ckpt_loc;
	int4		mirror_bufpool_resync_ckpt_block_num;
	int8		mirror_append_only_loss_eof;
	int8		mirror_append_only_new_eof;
	int4		relation_bufpool_kind;
	int4		parent_xid;
	int8		persistent_serial_num;
} FormData_gp_persistent_relation_node;

/* FIXME: no foreign keys defined */

#define Natts_gp_persistent_relation_node				    				  					18
#define Anum_gp_persistent_relation_node_tablespace_oid  					  					1
#define Anum_gp_persistent_relation_node_database_oid   					  					2
#define Anum_gp_persistent_relation_node_relfilenode_oid					  					3
#define Anum_gp_persistent_relation_node_segment_file_num					  					4
#define Anum_gp_persistent_relation_node_relation_storage_manager			  					5
#define Anum_gp_persistent_relation_node_persistent_state					  					6
#define Anum_gp_persistent_relation_node_create_mirror_data_loss_tracking_session_num	  		7
#define Anum_gp_persistent_relation_node_mirror_existence_state				  					8
#define Anum_gp_persistent_relation_node_mirror_data_synchronization_state	  					9
#define Anum_gp_persistent_relation_node_mirror_bufpool_marked_for_scan_incremental_resync 		10
#define Anum_gp_persistent_relation_node_mirror_bufpool_resync_changed_page_count				11
#define Anum_gp_persistent_relation_node_mirror_bufpool_resync_ckpt_loc		  					12
#define Anum_gp_persistent_relation_node_mirror_bufpool_resync_ckpt_block_num 					13
#define Anum_gp_persistent_relation_node_mirror_append_only_loss_eof		  					14
#define Anum_gp_persistent_relation_node_mirror_append_only_new_eof			  					15
#define Anum_gp_persistent_relation_node_relation_bufpool_kind									16
#define Anum_gp_persistent_relation_node_parent_xid  						  					17
#define Anum_gp_persistent_relation_node_persistent_serial_num     			  					18
 
typedef FormData_gp_persistent_relation_node *Form_gp_persistent_relation_node;


/*
 * gp_persistent_relation_node table values for FormData_pg_attribute.
 *
 * [Similar examples are Schema_pg_type, Schema_pg_proc, Schema_pg_attribute, etc, in
 *  pg_attribute.h]
 */
#define Schema_gp_persistent_relation_node \
{ GpPersistentRelationNodeRelationId, {"tablespace_oid"}, 										26, -1,	4, 1, 0, -1, -1, true, 'p', 'i', true, false, false, true, 0 }, \
{ GpPersistentRelationNodeRelationId, {"database_oid"}, 										26, -1,	4, 2, 0, -1, -1, true, 'p', 'i', true, false, false, true, 0 }, \
{ GpPersistentRelationNodeRelationId, {"relfilenode_oid"}, 										26, -1,	4, 3, 0, -1, -1, true, 'p', 'i', true, false, false, true, 0 }, \
{ GpPersistentRelationNodeRelationId, {"segment_file_num"}, 									23, -1, 4, 4, 0, -1, -1, true, 'p', 'i', true, false, false, true, 0 }, \
{ GpPersistentRelationNodeRelationId, {"relation_storage_manager"},								21, -1, 2, 5, 0, -1, -1, true, 'p', 's', true, false, false, true, 0 }, \
{ GpPersistentRelationNodeRelationId, {"persistent_state"}, 									21, -1, 2, 6, 0, -1, -1, true, 'p', 's', true, false, false, true, 0 }, \
{ GpPersistentRelationNodeRelationId, {"create_mirror_data_loss_tracking_session_num"}, 		20, -1, 8, 7, 0, -1, -1, true, 'p', 'd', true, false, false, true, 0 }, \
{ GpPersistentRelationNodeRelationId, {"mirror_existence_state"}, 								21, -1, 2, 8, 0, -1, -1, true, 'p', 's', true, false, false, true, 0 }, \
{ GpPersistentRelationNodeRelationId, {"mirror_data_synchronization_state"},					21, -1, 2, 9, 0, -1, -1, true, 'p', 's', true, false, false, true, 0 }, \
{ GpPersistentRelationNodeRelationId, {"mirror_bufpool_marked_for_scan_incremental_resync"},	20, -1, 8, 18, 0, -1, -1, true, 'p', 'd', true, false, false, true, 0 }, \
{ GpPersistentRelationNodeRelationId, {"mirror_bufpool_resync_changed_page_count"}, 			23, -1, 4, 11, 0, -1, -1, true, 'p', 'i', true, false, false, true, 0 }, \
{ GpPersistentRelationNodeRelationId, {"mirror_bufpool_resync_ckpt_loc"},						3310, -1, 8, 12, 0, -1, -1, false, 'p', 'i', true, false, false, true, 0 }, \
{ GpPersistentRelationNodeRelationId, {"mirror_bufpool_resync_ckpt_block_num"},					23, -1, 4, 13, 0, -1, -1, true, 'p', 'i', true, false, false, true, 0 }, \
{ GpPersistentRelationNodeRelationId, {"mirror_append_only_loss_eof"},							20, -1, 8, 14, 0, -1, -1, true, 'p', 'd', true, false, false, true, 0 }, \
{ GpPersistentRelationNodeRelationId, {"mirror_append_only_new_eof"},							20, -1, 8, 15, 0, -1, -1, true, 'p', 'd', true, false, false, true, 0 }, \
{ GpPersistentRelationNodeRelationId, {"relation_bufpool_kind"},								23, -1, 4, 16, 0, -1, -1, true, 'p', 'i', true, false, false, true, 0 }, \
{ GpPersistentRelationNodeRelationId, {"parent_xid"},											23, -1, 4, 17, 0, -1, -1, true, 'p', 'i', true, false, false, true, 0 }, \
{ GpPersistentRelationNodeRelationId, {"persistent_serial_num"},								20, -1, 8, 18, 0, -1, -1, true, 'p', 'd', true, false, false, true, 0 }, \

/*
 * gp_persistent_relation_node table values for FormData_pg_class.
 */
#define Class_gp_persistent_relation_node \
  {"gp_persistent_relation_node"}, PG_CATALOG_NAMESPACE, GP_PERSISTENT_RELATION_NODE_OID, BOOTSTRAP_SUPERUSERID, 0, \
               GpPersistentRelationNodeRelationId, GLOBALTABLESPACE_OID, \
               25, 10000, 0, 0, false, true, false, RELKIND_RELATION, RELSTORAGE_HEAP, Natts_gp_persistent_relation_node, \
               0, false, false, false, false, false, FirstNormalTransactionId, {0}, {{{'\0','\0','\0','\0'},{'\0'}}}



/*
 * Defines for gp_relation_node table
 */
#define GpRelationNodeRelationName		"gp_relation_node"

#define GpRelationNodeRelationId 5094

CATALOG(gp_relation_node,5094) BKI_WITHOUT_OIDS
{
	Oid         tablespace_oid;
	Oid			relfilenode_oid;
	int4		segment_file_num;
	int8		create_mirror_data_loss_tracking_session_num;
	tid			persistent_tid;
	int8		persistent_serial_num;
} FormData_gp_relation_node;

#define Natts_gp_relation_node							6
#define Anum_gp_relation_node_tablespace_oid					1
#define Anum_gp_relation_node_relfilenode_oid					2
#define Anum_gp_relation_node_segment_file_num					3
#define Anum_gp_relation_node_create_mirror_data_loss_tracking_session_num	4
#define Anum_gp_relation_node_persistent_tid					5
#define Anum_gp_relation_node_persistent_serial_num				6

typedef FormData_gp_relation_node *Form_gp_relation_node;

/*
 * gp_relation_node table values for FormData_pg_attribute.
 *
 * [Similar examples are Schema_pg_type, Schema_pg_proc, Schema_pg_attribute, etc, in
 *  pg_attribute.h]
 */
#define Schema_gp_relation_node \
{ GpRelationNodeRelationId, {"tablespace_oid"}, 									26, -1,	4, 1, 0, -1, -1, true, 'p', 'i', true, false, false, true, 0 }, \
{ GpRelationNodeRelationId, {"relfilenode_oid"}, 									26, -1,	4, 2, 0, -1, -1, true, 'p', 'i', true, false, false, true, 0 }, \
{ GpRelationNodeRelationId, {"segment_file_num"}, 									23, -1, 4, 3, 0, -1, -1, true, 'p', 'i', true, false, false, true, 0 }, \
{ GpRelationNodeRelationId, {"create_mirror_data_loss_tracking_session_num"}, 		20, -1, 8, 4, 0, -1, -1, true, 'p', 'd', true, false, false, true, 0 }, \
{ GpRelationNodeRelationId, {"persistent_tid"},										27, -1, 6, 5, 0, -1, -1, false, 'p', 's', true, false, false, true, 0 }, \
{ GpRelationNodeRelationId, {"persistent_serial_num"},								20, -1, 8, 6, 0, -1, -1, true, 'p', 'd', true, false, false, true, 0 }

/*
 * gp_relation_node table values for FormData_pg_class.
 */
#define Class_gp_relation_node \
  {"gp_relation_node"}, PG_CATALOG_NAMESPACE, GP_RELATION_NODE_OID, BOOTSTRAP_SUPERUSERID, 0, \
               GpRelationNodeRelationId, DEFAULTTABLESPACE_OID, \
               25, 10000, 0, 0, true, false, false, RELKIND_RELATION, RELSTORAGE_HEAP, Natts_gp_relation_node, \
               0, false, false, false, false, false, FirstNormalTransactionId, {0}, {{{'\0','\0','\0','\0'},{'\0'}}}

/*
 * gp_relation_node's index.
 */
#define Natts_gp_relation_node_index				    					3
 
/*
 * gp_relation_node_index table values for FormData_pg_attribute.
 *
 * [Similar examples are Schema_pg_type, Schema_pg_proc, Schema_pg_attribute, etc, in
 *  pg_attribute.h]
 */
#define Schema_gp_relation_node_index \
{ GpRelationNodeRelationId, {"tablespace_oid"}, 	26, -1,	4, 1, 0, -1, -1, true, 'p', 'i', false, false, false, true, 0 }, \
{ GpRelationNodeRelationId, {"relfilenode_oid"}, 	26, -1,	4, 2, 0, -1, -1, true, 'p', 'i', false, false, false, true, 0 }, \
{ GpRelationNodeRelationId, {"segment_file_num"}, 	23, -1, 4, 3, 0, -1, -1, true, 'p', 'i', false, false, false, true, 0 },

/*
 * gp_relation_node_index index values for FormData_pg_class.
 */
#define Class_gp_relation_node_index \
  {"gp_relation_node_index"}, PG_CATALOG_NAMESPACE, 0, BOOTSTRAP_SUPERUSERID, BTREE_AM_OID, \
               GpRelationNodeOidIndexId, DEFAULTTABLESPACE_OID, \
               25, 10000, 0, 0, false, false, false, RELKIND_INDEX, RELSTORAGE_HEAP, Natts_gp_relation_node_index, \
               0, false, false, false, false, false, FirstNormalTransactionId, {0}, {{{'\0','\0','\0','\0'},{'\0'}}}

/*
 * gp_relation_node_index AM values for FormData_pg_am.
 */
#define Am_gp_relation_node_index \
   Am_btree

/*
 * Init fields of int2vector with 0 or other values.  Needs to be populated for real later.
 * vl_len_, ndim, dataoffset, elemtype, dim1, lbound1, values.
 */
#define Init_int2vector 0, 1, 0, InvalidOid, 0, 0, {0}

#define Init_oidvector 0, 1, 0, InvalidOid, 0, 0, {InvalidOid}

#define Init_text {'\0','\0','\0','\0'},{'\0'}
/*
 * gp_relation_node_index Index values for FormData_pg_index.
 */
#define Index_gp_relation_node_index \
	GpRelationNodeOidIndexId, GpRelationNodeRelationId, Natts_gp_relation_node_index, true, false, false, true, false, true, {Init_int2vector}, {Init_oidvector}, {Init_oidvector}, {Init_text}, {Init_text}

#define IndKey_gp_relation_node_index \
    1, 2, 3

#define IndClass_gp_relation_node_index \
    OID_BTREE_OPS_OID, OID_BTREE_OPS_OID, INT4_BTREE_OPS_OID

/*
 * Defines for gp_persistent_database_node table
 */
#define GpPersistentDatabaseNodeRelationName		"gp_persistent_database_node"

#define GpPersistentDatabaseNodeRelationId 5091

CATALOG(gp_persistent_database_node,5091) BKI_SHARED_RELATION BKI_WITHOUT_OIDS
{
	Oid			tablespace_oid;
	Oid			database_oid;
	int2		persistent_state;
	int8		create_mirror_data_loss_tracking_session_num;
	int2		mirror_existence_state;
	int4		reserved;
	int4		parent_xid;
	int8		persistent_serial_num;
} FormData_gp_persistent_database_node;

/* FIXME: no foreign keys defined */

#define Natts_gp_persistent_database_node				    							8
#define Anum_gp_persistent_database_node_tablespace_oid  								1
#define Anum_gp_persistent_database_node_database_oid   								2
#define Anum_gp_persistent_database_node_persistent_state      							3
#define Anum_gp_persistent_database_node_create_mirror_data_loss_tracking_session_num	4
#define Anum_gp_persistent_database_node_mirror_existence_state 						5
#define Anum_gp_persistent_database_node_reserved  										6
#define Anum_gp_persistent_database_node_parent_xid  									7
#define Anum_gp_persistent_database_node_persistent_serial_num  						8
 
typedef FormData_gp_persistent_database_node *Form_gp_persistent_database_node;


/*
 * gp_persistent_database_node table values for FormData_pg_attribute.
 *
 * [Similar examples are Schema_pg_type, Schema_pg_proc, Schema_pg_attribute, etc, in
 *  pg_attribute.h]
 */
#define Schema_gp_persistent_database_node \
{ GpPersistentDatabaseNodeRelationId, {"tablespace_oid"}, 									26, -1,	4, 1, 0, -1, -1, true, 'p', 'i', true, false, false, true, 0 }, \
{ GpPersistentDatabaseNodeRelationId, {"database_oid"}, 									26, -1,	4, 2, 0, -1, -1, true, 'p', 'i', true, false, false, true, 0 }, \
{ GpPersistentDatabaseNodeRelationId, {"persistent_state"}, 								21, -1, 2, 3, 0, -1, -1, true, 'p', 's', true, false, false, true, 0 }, \
{ GpPersistentDatabaseNodeRelationId, {"create_mirror_data_loss_tracking_session_num"},		20, -1, 8, 4, 0, -1, -1, true, 'p', 'd', true, false, false, true, 0 }, \
{ GpPersistentDatabaseNodeRelationId, {"mirror_existence_state"}, 							21, -1, 2, 5, 0, -1, -1, true, 'p', 's', true, false, false, true, 0 }, \
{ GpPersistentDatabaseNodeRelationId, {"reserved"},											23, -1, 4, 6, 0, -1, -1, true, 'p', 'i', true, false, false, true, 0 }, \
{ GpPersistentDatabaseNodeRelationId, {"parent_xid"},										23, -1, 4, 7, 0, -1, -1, true, 'p', 'i', true, false, false, true, 0 }, \
{ GpPersistentDatabaseNodeRelationId, {"persistent_serial_num"},							20, -1, 8, 8, 0, -1, -1, true, 'p', 'd', true, false, false, true, 0 }, \

/*
 * gp_persistent_database_node table values for FormData_pg_class.
 */
#define Class_gp_persistent_database_node \
  {"gp_persistent_database_node"}, PG_CATALOG_NAMESPACE, GP_PERSISTENT_DATABASE_NODE_OID, BOOTSTRAP_SUPERUSERID, 0, \
               GpPersistentDatabaseNodeRelationId, GLOBALTABLESPACE_OID, \
               2, 100, 0, 0, false, true, false, RELKIND_RELATION, RELSTORAGE_HEAP, Natts_gp_persistent_database_node, \
               0, false, false, false, false, false, FirstNormalTransactionId, {0}, {{{'\0','\0','\0','\0'},{'\0'}}}


/*
 * Defines for gp_persistent_tablespace_node table
 */
#define GpPersistentTablespaceNodeRelationName		"gp_persistent_tablespace_node"

#define GpPersistentTablespaceNodeRelationId 5092

CATALOG(gp_persistent_tablespace_node,5092) BKI_SHARED_RELATION BKI_WITHOUT_OIDS
{
	Oid			filespace_oid;
	Oid			tablespace_oid;
	int2		persistent_state;
	int8		create_mirror_data_loss_tracking_session_num;
	int2		mirror_existence_state;
	int4		reserved;
	int4		parent_xid;
	int8		persistent_serial_num;
} FormData_gp_persistent_tablespace_node;

/* FIXME: no foreign keys defined */

#define Natts_gp_persistent_tablespace_node				    								8
#define Anum_gp_persistent_tablespace_node_filespace_oid  									1
#define Anum_gp_persistent_tablespace_node_tablespace_oid  									2
#define Anum_gp_persistent_tablespace_node_persistent_state      							3
#define Anum_gp_persistent_tablespace_node_create_mirror_data_loss_tracking_session_num		4
#define Anum_gp_persistent_tablespace_node_mirror_existence_state   						5
#define Anum_gp_persistent_tablespace_node_reserved				    						6
#define Anum_gp_persistent_tablespace_node_parent_xid  										7
#define Anum_gp_persistent_tablespace_node_persistent_serial_num    						8
 
typedef FormData_gp_persistent_tablespace_node *Form_gp_persistent_tablespace_node;


/*
 * gp_persistent_tablespace_node table values for FormData_pg_attribute.
 *
 * [Similar examples are Schema_pg_type, Schema_pg_proc, Schema_pg_attribute, etc, in
 *  pg_attribute.h]
 */
#define Schema_gp_persistent_tablespace_node \
{ GpPersistentTablespaceNodeRelationId, {"filespace_oid"},									26, -1, 4, 1, 0, -1, -1, true, 'p', 'i', true, false, false, true, 0 }, \
{ GpPersistentTablespaceNodeRelationId, {"tablespace_oid"}, 								26, -1,	4, 2, 0, -1, -1, true, 'p', 'i', true, false, false, true, 0 }, \
{ GpPersistentTablespaceNodeRelationId, {"persistent_state"},								21, -1, 2, 3, 0, -1, -1, true, 'p', 's', true, false, false, true, 0 }, \
{ GpPersistentTablespaceNodeRelationId, {"create_mirror_data_loss_tracking_session_num"}, 	20, -1, 8, 4, 0, -1, -1, true, 'p', 'd', true, false, false, true, 0 }, \
{ GpPersistentTablespaceNodeRelationId, {"mirror_existence_state"},							21, -1, 2, 5, 0, -1, -1, true, 'p', 's', true, false, false, true, 0 }, \
{ GpPersistentTablespaceNodeRelationId, {"reserved"}, 										23, -1, 4, 6, 0, -1, -1, true, 'p', 'i', true, false, false, true, 0 }, \
{ GpPersistentTablespaceNodeRelationId, {"parent_xid"},										23, -1, 4, 7, 0, -1, -1, true, 'p', 'i', true, false, false, true, 0 }, \
{ GpPersistentTablespaceNodeRelationId, {"persistent_serial_num"},							20, -1, 8, 8, 0, -1, -1, true, 'p', 'd', true, false, false, true, 0 }, \

/*
 * gp_persistent_tablespace_node table values for FormData_pg_class.
 */
#define Class_gp_persistent_tablespace_node \
  {"gp_persistent_tablespace_node"}, PG_CATALOG_NAMESPACE, GP_PERSISTENT_TABLESPACE_NODE_OID, BOOTSTRAP_SUPERUSERID, 0, \
               GpPersistentTablespaceNodeRelationId, GLOBALTABLESPACE_OID, \
               2, 100, 0, 0, false, true, false, RELKIND_RELATION, RELSTORAGE_HEAP, Natts_gp_persistent_tablespace_node, \
               0, false, false, false, false, false, FirstNormalTransactionId, {0}, {{{'\0','\0','\0','\0'},{'\0'}}}


/*
 * Defines for gp_persistent_filespace_node table
 */
#define GpPersistentFilespaceNodeRelationName		"gp_persistent_filespace_node"

#define GpPersistentFilespaceNodeRelationId 5093

CATALOG(gp_persistent_filespace_node,5093) BKI_SHARED_RELATION BKI_WITHOUT_OIDS
{
	Oid			filespace_oid;
	int2		db_id_1;
	text		location_1;
	int2		db_id_2;
	text		location_2;
	int2		persistent_state;
	int8		create_mirror_data_loss_tracking_session_num;
	int2		mirror_existence_state;
	int4		reserved;
	int4		parent_xid;
	int8		persistent_serial_num;
} FormData_gp_persistent_filespace_node;

/* FIXME: no foreign keys defined */

#define Natts_gp_persistent_filespace_node				    	 						11
#define Anum_gp_persistent_filespace_node_filespace_oid  		 						1
#define Anum_gp_persistent_filespace_node_db_id_1  				 						2
#define Anum_gp_persistent_filespace_node_location_1			 						3
#define Anum_gp_persistent_filespace_node_db_id_2  				 						4
#define Anum_gp_persistent_filespace_node_location_2			 						5
#define Anum_gp_persistent_filespace_node_persistent_state       						6
#define Anum_gp_persistent_filespace_node_create_mirror_data_loss_tracking_session_num  7
#define Anum_gp_persistent_filespace_node_mirror_existence_state 						8
#define Anum_gp_persistent_filespace_node_reserved				 						9
#define Anum_gp_persistent_filespace_node_parent_xid  			 						10
#define Anum_gp_persistent_filespace_node_persistent_serial_num  						11
 
typedef FormData_gp_persistent_filespace_node *Form_gp_persistent_filespace_node;


/*
 * gp_persistent_filespace_node table values for FormData_pg_attribute.
 *
 * [Similar examples are Schema_pg_type, Schema_pg_proc, Schema_pg_attribute, etc, in
 *  pg_attribute.h]
 *
 * NOTE: The use of type text triggers all columns afterward to be NOT NULL....
 */
#define Schema_gp_persistent_filespace_node \
{ GpPersistentFilespaceNodeRelationId, {"filespace_oid"}, 									26, -1,	4, 1, 0, -1, -1, true, 'p', 'i', true, false, false, true, 0 }, \
{ GpPersistentFilespaceNodeRelationId, {"db_id_1"},											21, -1, 2, 2, 0, -1, -1, true, 'p', 's', true, false, false, true, 0 }, \
{ GpPersistentFilespaceNodeRelationId, {"location_1"},										25, -1, -1, 3, 0, -1, -1, false, 'x', 'i', false, false, false, true, 0 }, \
{ GpPersistentFilespaceNodeRelationId, {"db_id_2"}, 										21, -1, 2, 4, 0, -1, -1, true, 'p', 's', false, false, false, true, 0 }, \
{ GpPersistentFilespaceNodeRelationId, {"location_2"},										25, -1, -1, 5, 0, -1, -1, false, 'x', 'i', false, false, false, true, 0 }, \
{ GpPersistentFilespaceNodeRelationId, {"persistent_state"},								21, -1, 2, 6, 0, -1, -1, true, 'p', 's', false, false, false, true, 0 }, \
{ GpPersistentFilespaceNodeRelationId, {"create_mirror_data_loss_tracking_session_num"},	20, -1, 8, 7, 0, -1, -1, true, 'p', 'd', true, false, false, true, 0 }, \
{ GpPersistentFilespaceNodeRelationId, {"mirror_existence_state"},							21, -1, 2, 8, 0, -1, -1, true, 'p', 's', false, false, false, true, 0 }, \
{ GpPersistentFilespaceNodeRelationId, {"reserved"},										23, -1, 4, 9, 0, -1, -1, true, 'p', 'i', false, false, false, true, 0 }, \
{ GpPersistentFilespaceNodeRelationId, {"parent_xid"},										23, -1, 4, 10, 0, -1, -1, true, 'p', 'i', false, false, false, true, 0 }, \
{ GpPersistentFilespaceNodeRelationId, {"persistent_serial_num"},							20, -1, 8, 11, 0, -1, -1, true, 'p', 'd', false, false, false, true, 0 }, \

/*
 * gp_persistent_filespace_node table values for FormData_pg_class.
 */
#define Class_gp_persistent_filespace_node \
  {"gp_persistent_filespace_node"}, PG_CATALOG_NAMESPACE, GP_PERSISTENT_FILESPACE_NODE_OID, BOOTSTRAP_SUPERUSERID, 0, \
               GpPersistentFilespaceNodeRelationId, GLOBALTABLESPACE_OID, \
               2, 100, 0, 0, false, true, false, RELKIND_RELATION, RELSTORAGE_HEAP, Natts_gp_persistent_filespace_node, \
               0, false, false, false, false, false, FirstNormalTransactionId, {0}, {{{'\0','\0','\0','\0'},{'\0'}}}



#undef tid
#undef int8
#undef gpxlogloc


// Cannot change this length with upgrade work to convert the file's tuples...
#define FilespaceLocationBlankPaddedWithNullTermLen MAXPGPATH

inline static bool GpPersistent_IsPersistentRelation(
	Oid		testOid)
{
	/*
	 * Persistent related global tables.
	 */
	if (testOid == GpPersistentRelationNodeRelationId ||	// 5090
		testOid == GpPersistentDatabaseNodeRelationId ||	// 5091
		testOid == GpPersistentTablespaceNodeRelationId ||	// 5092
		testOid == GpPersistentFilespaceNodeRelationId ||	// 5093
		testOid == GpGlobalSequenceRelationId)				// 5096
		return true;

	return false;
}
inline static bool GpPersistent_SkipXLogInfo(
	Oid		testOid)
{
	/*
	 * NOTE: Only shared (i.e. global) relations can be skipped.
	 */
//	Assert(IsSharedRelation(testOid));

	if (GpPersistent_IsPersistentRelation(testOid))
		return true;
	
	/*
	 * gp_id and pg_shdepend 
	 */
	if (testOid == GpIdRelationId ||						// 5001
		testOid == SharedDependRelationId)					// 1214
		return true;

	return false;
}

extern void GpPersistentRelationNode_GetRelationInfo(
	char		relkind,			/* see RELKIND_xxx constants */
	char		relstorage,			/* see RELSTORAGE_xxx constants */
	Oid			relam,				/* index access method; 0 if not an index */

	PersistentFileSysRelStorageMgr			*relationStorageManager,
	PersistentFileSysRelBufpoolKind			*relationBufpoolKind);

extern void GpPersistentRelationNode_GetValues(
	Datum									*values,

	Oid 									*tablespaceOid,
	Oid 									*databaseOid,
	Oid 									*relfilenodeOid,
	int32									*segmentFileNum,
	PersistentFileSysRelStorageMgr			*relationStorageManager,
	PersistentFileSysState					*persistentState,
	int64									*createMirrorDataLossTrackingSessionNum,
	MirroredObjectExistenceState			*mirrorExistenceState,
	MirroredRelDataSynchronizationState 	*mirrorDataSynchronizationState,
	bool									*mirrorBufpoolMarkedForScanIncrementalResync,
	int64									*mirrorBufpoolResyncChangedPageCount,
	XLogRecPtr								*mirrorBufpoolResyncCkptLoc,
	BlockNumber								*mirrorBufpoolResyncCkptBlockNum,
	int64									*mirrorAppendOnlyLossEof,
	int64									*mirrorAppendOnlyNewEof,
	PersistentFileSysRelBufpoolKind 		*relBufpoolKind,
	TransactionId							*parentXid,
	int64									*persistentSerialNum);

extern void GpPersistentRelationNode_SetDatumValues(
	Datum									*values,

	Oid 									tablespaceOid,
	Oid 									databaseOid,
	Oid 									relfilenodeOid,
	int32									segmentFileNum,
	PersistentFileSysRelStorageMgr			relationStorageManager,
	PersistentFileSysState					persistentState,
	int64									createMirrorDataLossTrackingSessionNum,
	MirroredObjectExistenceState			mirrorExistenceState,
	MirroredRelDataSynchronizationState 	mirrorDataSynchronizationState,
	bool									mirrorBufpoolMarkedForScanIncrementalResync,
	int64									mirrorBufpoolResyncChangedPageCount,
	XLogRecPtr								*mirrorBufpoolResyncCkptLoc,
	BlockNumber								mirrorBufpoolResyncCkptBlockNum,
	int64									mirrorAppendOnlyLossEof,
	int64									mirrorAppendOnlyNewEof,
	PersistentFileSysRelBufpoolKind 		relBufpoolKind,
	TransactionId							parentXid,
	int64									persistentSerialNum);

extern void GpPersistentDatabaseNode_GetValues(
	Datum							*values,

	Oid 							*tablespaceOid,
	Oid 							*databaseOid,
	PersistentFileSysState			*persistentState,
	int64							*createMirrorDataLossTrackingSessionNum,
	MirroredObjectExistenceState 	*mirrorExistenceState,
	int32							*reserved,
	TransactionId					*parentXid,
	int64							*persistentSerialNum);

extern void GpPersistentDatabaseNode_SetDatumValues(
	Datum							*values,

	Oid 							tablespaceOid,
	Oid 							databaseOid,
	PersistentFileSysState			persistentState,
	int64							createMirrorDataLossTrackingSessionNum,
	MirroredObjectExistenceState	mirrorExistenceState,
	int32							reserved,
	TransactionId					parentXid,
	int64							persistentSerialNum);

extern void GpPersistentTablespaceNode_GetValues(
	Datum							*values,

	Oid 							*filespaceOid,
	Oid 							*tablespaceOid,
	PersistentFileSysState			*persistentState,
	int64							*createMirrorDataLossTrackingSessionNum,
	MirroredObjectExistenceState	*mirrorExistenceState,
	int32							*reserved,
	TransactionId					*parentXid,
	int64							*persistentSerialNum);

extern void GpPersistentTablespaceNode_SetDatumValues(
	Datum							*values,

	Oid 							filespaceOid,
	Oid 							tablespaceOid,
	PersistentFileSysState			persistentState,
	int64							createMirrorDataLossTrackingSessionNum,
	MirroredObjectExistenceState	mirrorExistenceState,
	int32							reserved,
	TransactionId					parentXid,
	int64							persistentSerialNum);

extern void GpPersistentFilespaceNode_GetValues(
	Datum							*values,

	Oid 							*filespaceOid,
	int16							*dbId1,
	char							locationBlankPadded1[FilespaceLocationBlankPaddedWithNullTermLen],
	int16							*dbId2,
	char							locationBlankPadded2[FilespaceLocationBlankPaddedWithNullTermLen],
	PersistentFileSysState			*persistentState,
	int64							*createMirrorDataLossTrackingSessionNum,
	MirroredObjectExistenceState	*mirrorExistenceState,
	int32							*reserved,
	TransactionId					*parentXid,
	int64							*persistentSerialNum);

extern void GpPersistentFilespaceNode_SetDatumValues(
	Datum							*values,

	Oid 							filespaceOid,
	int16							dbId1,
	char							locationBlankPadded1[FilespaceLocationBlankPaddedWithNullTermLen],
	int16							dbId2,
	char							locationBlankPadded2[FilespaceLocationBlankPaddedWithNullTermLen],
	PersistentFileSysState			persistentState,
	int64							createMirrorDataLossTrackingSessionNum,
	MirroredObjectExistenceState	mirrorExistenceState,
	int32							reserved,
	TransactionId					parentXid,
	int64							persistentSerialNum);

extern void GpPersistent_GetCommonValues(
	PersistentFsObjType 			fsObjType,
	Datum							*values,

	PersistentFileSysObjName		*fsObjName,
	PersistentFileSysState			*persistentState,
	MirroredObjectExistenceState	*mirrorExistenceState,
	TransactionId					*parentXid,
	int64							*persistentSerialNum);

extern void GpRelationNode_GetValues(
	Datum							*values,
	Oid 							*tablespaceOid,
	Oid 							*relfilenodeOid,
	int32							*segmentFileNum,
	int64							*createMirrorDataLossTrackingSessionNum,
	ItemPointer						persistentTid,
	int64							*persistentSerialNum);

extern void GpRelationNode_SetDatumValues(
	Datum							*values,
	Oid 							tablespaceOid,
	Oid 							relfilenodeOid,
	int32							segmentFileNum,
	int64							createMirrorDataLossTrackingSessionNum,
	ItemPointer		 				persistentTid,
	int64							persistentSerialNum);


#endif   /* GP_PERSISTENT_H */

