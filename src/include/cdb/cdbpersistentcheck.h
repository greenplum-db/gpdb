/*-------------------------------------------------------------------------
 *
 * cdbpersistentcheck.h
 *
 * Portions Copyright (c) 2009-2010, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/cdbpersistentcheck.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef CDBPERSISTENTCHECK_H
#define CDBPERSISTENTCHECK_H

#include "miscadmin.h"
#include "utils/guc.h"

#include "cdb/cdbpersistentstore.h"

/*
 * This struct provides accounting information to help Post DTM recovery
 * PersistentTables-Catalog verification.
 */
typedef struct PT_PostDTMRecv_Data
{
	/* Indicates if PT-Catalog verification is indeed needed after DTM recovery */
	bool postDTMRecv_PTCatVerificationNeeded;

	/*
	 * Hash table that maintains a pair of {dbOid, Tablespace Oid} as the value
	 * and dbOid as the key. It maintains all the databases who haven't undergone
	 * PTCatalog verification.
	 */
	HTAB *postDTMRecv_dbTblSpc_Hash;
}PT_PostDTMRecv_Data;

typedef struct postDTMRecv_dbTblSpc_Hash_Entry
{
	Oid database;
	Oid tablespace;
}postDTMRecv_dbTblSpc_Hash_Entry;

extern void PTCheck_BeforeAddingEntry( PersistentStoreData *storeData, Datum *values);

extern ScanKey Persistent_RelationScanKeyInit(Datum *values, int *nKeys);
extern bool Persistent_RelationAllowDuplicateEntry(Datum *exist_values, Datum *new_values);
extern ScanKey Persistent_DatabaseScanKeyInit(Datum *values, int *nKeys);
extern bool Persistent_DatabaseAllowDuplicateEntry(Datum *exist_values, Datum *new_values);
extern ScanKey Persistent_TablespaceScanKeyInit(Datum *values, int *nKeys);
extern bool Persistent_TablespaceAllowDuplicateEntry(Datum *exist_values, Datum *new_values);
extern ScanKey Persistent_FilespaceScanKeyInit(Datum *values, int *nKeys);
extern bool Persistent_FilespaceAllowDuplicateEntry(Datum *exist_values, Datum *new_values);
extern bool Persistent_VerifyCrossCatalogConsistency(void);
extern void Persistent_Pre_ExecuteQuery(void);
extern int Persistent_ExecuteQuery(char const *query, bool readOnly);
extern void Persistent_Post_ExecuteQuery(void);
extern void Persistent_ExecuteQuery_Cleanup(void);
extern bool Persistent_NonDBSpecificPTCatVerification(void);
extern bool Persistent_DBSpecificPTCatVerification(void);
extern void Persistent_PostDTMRecv_NonDBSpecificPTCatVerification(void);

extern Size Persistent_PostDTMRecv_ShmemSize(void);
void Persistent_PostDTMRecv_ShmemInit(void);

extern postDTMRecv_dbTblSpc_Hash_Entry *Persistent_PostDTMRecv_InsertHashEntry(Oid dbId, postDTMRecv_dbTblSpc_Hash_Entry *values, bool *exists);
extern void Persistent_PostDTMRecv_RemoveHashEntry(Oid dbId);
extern postDTMRecv_dbTblSpc_Hash_Entry *Persistent_PostDTMRecv_LookupHashEntry(Oid dbId, bool *exists);
extern void Persistent_PrintHash(void);
extern void Persistent_Set_PostDTMRecv_PTCatVerificationNeeded(void);
extern bool Persistent_PostDTMRecv_PTCatVerificationNeeded(void);

extern void Persistent_PostDTMRecv_DBSpecificPTCatVerification(void);
extern bool Persistent_PostDTMRecv_IsHashFull(void);

#endif
