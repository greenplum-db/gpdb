/*-------------------------------------------------------------------------
 *
 * pg_extprotocol.h
 *    an external table custom protocol table
 *
 * Portions Copyright (c) 2011, Greenplum Inc.
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
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
#include "catalog/pg_extprotocol_d.h"
#include "nodes/pg_list.h"
#include "utils/acl.h"

/* ----------------
 *		pg_extprotocol definition.  cpp turns this into
 *		typedef struct FormData_pg_extprotocol
 * ----------------
 */
CATALOG(pg_extprotocol,7175,ExtprotocolRelationId)
{
	Oid			oid;			/* oid */
	NameData	ptcname;
	Oid			ptcreadfn;
	Oid			ptcwritefn;
	Oid			ptcvalidatorfn;
	Oid			ptcowner;
	bool		ptctrusted;
#ifdef CATALOG_VARLEN			/* variable-length fields start here */
	aclitem		ptcacl[1];
#endif
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

extern Oid
LookupExtProtocolFunction(const char *prot_name, 
						  ExtPtcFuncType prot_type,
						  bool error);

extern Oid get_extprotocol_oid(const char *prot_name, bool error_if_missing);

extern char *
ExtProtocolGetNameByOid(Oid	protOid);

#endif /* PG_EXTPROTOCOL_H */
