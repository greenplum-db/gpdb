/*-------------------------------------------------------------------------
 *
 * pg_extprotocol.h
 *    an external table custom protocol table
 *
 * Copyright (c) 2011, Greenplum Inc.
 *
 * $Id: $
 * $Change: $
 * $DateTime: $
 * $Author: $
 *-------------------------------------------------------------------------
 */
#ifndef PG_EXTPROTOCOL_H
#define PG_EXTPROTOCOL_H

#include "catalog/genbki.h"
#include "nodes/pg_list.h"
#include "utils/acl.h"

/* TIDYCAT_BEGINDEF

  CREATE TABLE pg_extprotocol
  with (shared=false, oid=true, relid=7175)
  (
   ptcname        name,
   ptcreadfn      Oid,
   ptcwritefn     Oid,
   ptcvalidatorfn Oid, 
   ptcowner       Oid,
   ptctrusted	  bool,
   ptcacl         aclitem[]   
   );
   
   create unique index on pg_extprotocol(oid) with (indexid=7156);
   create unique index on pg_extprotocol(ptcname) with (indexid=7177);
   alter table pg_extprotocol add fk ptcreadfn on pg_proc(oid);
   alter table pg_extprotocol add fk ptcwritefn on pg_proc(oid);
   alter table pg_extprotocol add fk ptcvalidatorfn on pg_proc(oid);

   TIDYCAT_ENDDEF
*/
/* TIDYCAT_BEGIN_CODEGEN 

   WARNING: DO NOT MODIFY THE FOLLOWING SECTION: 
   Generated by ./tidycat.pl version 29
   on Sun Aug  7 16:12:43 2011
*/


/*
 TidyCat Comments for pg_extprotocol:
  Table has an Oid column.
  Table has static type (see pg_types.h).

*/

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


/* TIDYCAT_END_CODEGEN */

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
ExtProtocolCreateWithOid(const char			*protocolName,
						 List				*readfuncName,
						 List				*writefuncName,
						 List				*validatorfuncName,						 
						 Oid				 protOid,
						 bool				 trusted);

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
