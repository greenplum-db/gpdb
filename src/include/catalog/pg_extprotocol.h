/*-------------------------------------------------------------------------
 *
 * pg_extprotocol.h
 *    an external table custom protocol table
 *
 * Portions Copyright (c) 2011, Greenplum Inc.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/catalog/pg_extprotocol.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef PG_EXTPROTOCOL_H
#define PG_EXTPROTOCOL_H

#include "catalog/genbki.h"
#include "nodes/pg_list.h"
#include "utils/acl.h"

/* ----------------
 *		pg_extprotocol definition.  cpp turns this into
 *		typedef struct FormData_pg_extprotocol
 * ----------------
 */
#define ExtprotocolRelationId	7175

CATALOG(pg_extprotocol,7175)
{
	NameData	ptcname;		
	Oid			ptcreadfn;		
	Oid			ptcwritefn;		
	Oid			ptcvalidatorfn;	
	Oid			ptcowner;		
	bool		ptctrusted;		
	aclitem		ptcacl[1];		
} FormData_pg_extprotocol;

/* GPDB added foreign key definitions for gpcheckcat. */
FOREIGN_KEY(ptcreadfn REFERENCES pg_proc(oid));
FOREIGN_KEY(ptcwritefn REFERENCES pg_proc(oid));
FOREIGN_KEY(ptcvalidatorfn REFERENCES pg_proc(oid));

/* ----------------
 *		Form_pg_extprotocol corresponds to a pointer to a tuple with
 *		the format of pg_extprotocol relation.
 * ----------------
 */
typedef FormData_pg_extprotocol *Form_pg_extprotocol;


/* ----------------
 *		compiler constants for pg_extprotocol
 * ----------------
 */
#define Natts_pg_extprotocol				7
#define Anum_pg_extprotocol_ptcname			1
#define Anum_pg_extprotocol_ptcreadfn		2
#define Anum_pg_extprotocol_ptcwritefn		3
#define Anum_pg_extprotocol_ptcvalidatorfn	4
#define Anum_pg_extprotocol_ptcowner		5
#define Anum_pg_extprotocol_ptctrusted		6
#define Anum_pg_extprotocol_ptcacl			7

/*
 * Different type of functions that can be 
 * specified for a given external protocol.
 */
typedef enum ExtPtcFuncType
{
	EXTPTC_FUNC_READER,
	EXTPTC_FUNC_WRITER,
	EXTPTC_FUNC_VALIDATOR
	
} ExtPtcFuncType;

extern Oid
ExtProtocolCreate(const char *protocolName,
				  List *readfuncName,
				  List *writefuncName,
				  List *validatorfuncName,
				  bool trusted);

extern void
ExtProtocolDeleteByOid(Oid	protOid);

extern Oid
LookupExtProtocolFunction(const char *prot_name, 
						  ExtPtcFuncType prot_type,
						  bool error);

extern Oid
LookupExtProtocolOid(const char *prot_name, bool error_if_missing);

extern char *
ExtProtocolGetNameByOid(Oid	protOid);

#endif /* PG_EXTPROTOCOL_H */
