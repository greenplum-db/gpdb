/*-------------------------------------------------------------------------
 *
 * pg_proc.h
 *	  definition of the system "procedure" relation (pg_proc)
 *	  along with the relation's initial contents.
 *
 * Portions Copyright (c) 2006-2010, Greenplum inc
 * Portions Copyright (c) 1996-2010, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/catalog/pg_proc.h,v 1.429 2006/11/28 19:18:44 tgl Exp $
 *
 * NOTES
 *	  The script catalog/genbki.sh reads this file and generates .bki
 *	  information from the DATA() statements.  utils/Gen_fmgrtab.sh
 *	  generates fmgroids.h and fmgrtab.c the same way.
 *
 *	  XXX do NOT break up DATA() statements into multiple lines!
 *		  the scripts are not as smart as you might think...
 *	  XXX (eg. #if 0 #endif won't do what you think)
 *
 *-------------------------------------------------------------------------
 */
#ifndef PG_PROC_H
#define PG_PROC_H

#include "catalog/genbki.h"

/* TIDYCAT_BEGINFAKEDEF

   CREATE TABLE pg_proc
   with (camelcase=Procedure, bootstrap=true, relid=1255, toast_oid=2836, toast_index=2837)
   (
   proname         name, 
   pronamespace    oid, 
   proowner        oid, 
   prolang         oid, 
   proisagg        boolean, 
   prosecdef       boolean, 
   proisstrict     boolean, 
   proretset       boolean, 
   provolatile     "char", 
   pronargs        smallint, 
   prorettype      oid, 
   proiswin        boolean, 
   proargtypes     oidvector, 
   proallargtypes  oid[], 
   proargmodes     "char"[], 
   proargnames     text[], 
   prosrc          text, 
   probin          bytea, 
   proacl          aclitem[],
   prodataaccess   "char",
   provariadic     oid
   );


   create unique index on pg_proc(oid) with (indexid=2690, CamelCase=ProcedureOid, syscacheid=PROCOID, syscache_nbuckets=2048);

   create unique index on pg_proc(proname, proargtypes, pronamespace) with (indexid=2691, CamelCase=ProcedureNameArgsNsp, syscacheid=PROCNAMEARGSNSP, syscache_nbuckets=2048);

   alter table pg_proc add fk pronamespace on pg_namespace(oid);
   alter table pg_proc add fk proowner on pg_authid(oid);
   alter table pg_proc add fk prolang on pg_language(oid);
   alter table pg_proc add fk prorettype on pg_type(oid);
   alter table pg_proc add vector_fk proargtypes on pg_type(oid);
   alter table pg_proc add vector_fk proallargtypes on pg_type(oid);

   TIDYCAT_ENDFAKEDEF
*/

/* ----------------
 *		pg_proc definition.  cpp turns this into
 *		typedef struct FormData_pg_proc
 * ----------------
 */
#define ProcedureRelationId  1255

CATALOG(pg_proc,1255) BKI_BOOTSTRAP
{
	NameData	proname;		/* procedure name */
	Oid			pronamespace;	/* OID of namespace containing this proc */
	Oid			proowner;		/* procedure owner */
	Oid			prolang;		/* OID of pg_language entry */
	bool		proisagg;		/* is it an aggregate? */
	bool		prosecdef;		/* security definer */
	bool		proisstrict;	/* strict with respect to NULLs? */
	bool		proretset;		/* returns a set? */
	char		provolatile;	/* see PROVOLATILE_ categories below */
	int2		pronargs;		/* number of arguments */
	Oid			prorettype;		/* OID of result type */
	bool		proiswin;		/* is it a window function? */

	/* VARIABLE LENGTH FIELDS: */
	oidvector	proargtypes;	/* parameter types (excludes OUT params) */
	Oid			proallargtypes[1];		/* all param types (NULL if IN only) */
	char		proargmodes[1]; /* parameter modes (NULL if IN only) */
	text		proargnames[1]; /* parameter names (NULL if no names) */
	text		prosrc;			/* procedure source text */
	bytea		probin;			/* secondary procedure definition field */
	aclitem		proacl[1];		/* access permissions */
	char		prodataaccess;	/* data access indicator */
	Oid			provariadic;	/* pg_type.oid of variadic argument */
} FormData_pg_proc;

/* ----------------
 *		Form_pg_proc corresponds to a pointer to a tuple with
 *		the format of pg_proc relation.
 * ----------------
 */
typedef FormData_pg_proc *Form_pg_proc;

/* ----------------
 *		compiler constants for pg_proc
 * ----------------
 */
#define Natts_pg_proc					21
#define Anum_pg_proc_proname			1
#define Anum_pg_proc_pronamespace		2
#define Anum_pg_proc_proowner			3
#define Anum_pg_proc_prolang			4
#define Anum_pg_proc_proisagg			5
#define Anum_pg_proc_prosecdef			6
#define Anum_pg_proc_proisstrict		7
#define Anum_pg_proc_proretset			8
#define Anum_pg_proc_provolatile		9
#define Anum_pg_proc_pronargs			10
#define Anum_pg_proc_prorettype			11
#define Anum_pg_proc_proiswin			12
#define Anum_pg_proc_proargtypes		13
#define Anum_pg_proc_proallargtypes		14
#define Anum_pg_proc_proargmodes		15
#define Anum_pg_proc_proargnames		16
#define Anum_pg_proc_prosrc				17
#define Anum_pg_proc_probin				18
#define Anum_pg_proc_proacl				19
#define Anum_pg_proc_prodataaccess		20
#define Anum_pg_proc_provariadic		21

/* ----------------
 *		initial contents of pg_proc
 * ----------------
 */

/* keep the following ordered by OID so that later changes can be made easier */

/* TIDYCAT_BEGIN_PG_PROC_GEN 


   WARNING: DO NOT MODIFY THE FOLLOWING SECTION: 
   Generated by ./catullus.pl version 8
   on Thu Nov 12 12:56:48 2015

   Please make your changes in pg_proc.sql
*/

/* OIDS 1 - 99  */
/* boolin(cstring) => bool */ 
DATA(insert OID = 1242 ( boolin  PGNSP PGUID 12 f f t f i 1 16 f "2275" _null_ _null_ _null_ boolin _null_ _null_ n 0));
DESCR("I/O");

/* boolout(bool) => cstring */ 
DATA(insert OID = 1243 ( boolout  PGNSP PGUID 12 f f t f i 1 2275 f "16" _null_ _null_ _null_ boolout _null_ _null_ n 0));
DESCR("I/O");

/* byteain(cstring) => bytea */ 
DATA(insert OID = 1244 ( byteain  PGNSP PGUID 12 f f t f i 1 17 f "2275" _null_ _null_ _null_ byteain _null_ _null_ n 0));
DESCR("I/O");

/* byteaout(bytea) => cstring */ 
DATA(insert OID = 31 ( byteaout  PGNSP PGUID 12 f f t f i 1 2275 f "17" _null_ _null_ _null_ byteaout _null_ _null_ n 0));
DESCR("I/O");

/* charin(cstring) => "char" */ 
DATA(insert OID = 1245 ( charin  PGNSP PGUID 12 f f t f i 1 18 f "2275" _null_ _null_ _null_ charin _null_ _null_ n 0));
DESCR("I/O");

/* charout("char") => cstring */ 
DATA(insert OID = 33 ( charout  PGNSP PGUID 12 f f t f i 1 2275 f "18" _null_ _null_ _null_ charout _null_ _null_ n 0));
DESCR("I/O");

/* namein(cstring) => name */ 
DATA(insert OID = 34 ( namein  PGNSP PGUID 12 f f t f i 1 19 f "2275" _null_ _null_ _null_ namein _null_ _null_ n 0));
DESCR("I/O");

/* nameout(name) => cstring */ 
DATA(insert OID = 35 ( nameout  PGNSP PGUID 12 f f t f i 1 2275 f "19" _null_ _null_ _null_ nameout _null_ _null_ n 0));
DESCR("I/O");

/* int2in(cstring) => int2 */ 
DATA(insert OID = 38 ( int2in  PGNSP PGUID 12 f f t f i 1 21 f "2275" _null_ _null_ _null_ int2in _null_ _null_ n 0));
DESCR("I/O");

/* int2out(int2) => cstring */ 
DATA(insert OID = 39 ( int2out  PGNSP PGUID 12 f f t f i 1 2275 f "21" _null_ _null_ _null_ int2out _null_ _null_ n 0));
DESCR("I/O");

/* int2vectorin(cstring) => int2vector */ 
DATA(insert OID = 40 ( int2vectorin  PGNSP PGUID 12 f f t f i 1 22 f "2275" _null_ _null_ _null_ int2vectorin _null_ _null_ n 0));
DESCR("I/O");

/* int2vectorout(int2vector) => cstring */ 
DATA(insert OID = 41 ( int2vectorout  PGNSP PGUID 12 f f t f i 1 2275 f "22" _null_ _null_ _null_ int2vectorout _null_ _null_ n 0));
DESCR("I/O");

/* int4in(cstring) => int4 */ 
DATA(insert OID = 42 ( int4in  PGNSP PGUID 12 f f t f i 1 23 f "2275" _null_ _null_ _null_ int4in _null_ _null_ n 0));
DESCR("I/O");

/* int4out(int4) => cstring */ 
DATA(insert OID = 43 ( int4out  PGNSP PGUID 12 f f t f i 1 2275 f "23" _null_ _null_ _null_ int4out _null_ _null_ n 0));
DESCR("I/O");

/* regprocin(cstring) => regproc */ 
DATA(insert OID = 44 ( regprocin  PGNSP PGUID 12 f f t f s 1 24 f "2275" _null_ _null_ _null_ regprocin _null_ _null_ n 0));
DESCR("I/O");

/* regprocout(regproc) => cstring */ 
DATA(insert OID = 45 ( regprocout  PGNSP PGUID 12 f f t f s 1 2275 f "24" _null_ _null_ _null_ regprocout _null_ _null_ n 0));
DESCR("I/O");

/* textin(cstring) => text */ 
DATA(insert OID = 46 ( textin  PGNSP PGUID 12 f f t f i 1 25 f "2275" _null_ _null_ _null_ textin _null_ _null_ n 0));
DESCR("I/O");

/* textout(text) => cstring */ 
DATA(insert OID = 47 ( textout  PGNSP PGUID 12 f f t f i 1 2275 f "25" _null_ _null_ _null_ textout _null_ _null_ n 0));
DESCR("I/O");

/* tidin(cstring) => tid */ 
DATA(insert OID = 48 ( tidin  PGNSP PGUID 12 f f t f i 1 27 f "2275" _null_ _null_ _null_ tidin _null_ _null_ n 0));
DESCR("I/O");

/* tidout(tid) => cstring */ 
DATA(insert OID = 49 ( tidout  PGNSP PGUID 12 f f t f i 1 2275 f "27" _null_ _null_ _null_ tidout _null_ _null_ n 0));
DESCR("I/O");

/* xidin(cstring) => xid */ 
DATA(insert OID = 50 ( xidin  PGNSP PGUID 12 f f t f i 1 28 f "2275" _null_ _null_ _null_ xidin _null_ _null_ n 0));
DESCR("I/O");

/* xidout(xid) => cstring */ 
DATA(insert OID = 51 ( xidout  PGNSP PGUID 12 f f t f i 1 2275 f "28" _null_ _null_ _null_ xidout _null_ _null_ n 0));
DESCR("I/O");

/* cidin(cstring) => cid */ 
DATA(insert OID = 52 ( cidin  PGNSP PGUID 12 f f t f i 1 29 f "2275" _null_ _null_ _null_ cidin _null_ _null_ n 0));
DESCR("I/O");

/* cidout(cid) => cstring */ 
DATA(insert OID = 53 ( cidout  PGNSP PGUID 12 f f t f i 1 2275 f "29" _null_ _null_ _null_ cidout _null_ _null_ n 0));
DESCR("I/O");

/* oidvectorin(cstring) => oidvector */ 
DATA(insert OID = 54 ( oidvectorin  PGNSP PGUID 12 f f t f i 1 30 f "2275" _null_ _null_ _null_ oidvectorin _null_ _null_ n 0));
DESCR("I/O");

/* oidvectorout(oidvector) => cstring */ 
DATA(insert OID = 55 ( oidvectorout  PGNSP PGUID 12 f f t f i 1 2275 f "30" _null_ _null_ _null_ oidvectorout _null_ _null_ n 0));
DESCR("I/O");

/* boollt(bool, bool) => bool */ 
DATA(insert OID = 56 ( boollt  PGNSP PGUID 12 f f t f i 2 16 f "16 16" _null_ _null_ _null_ boollt _null_ _null_ n 0));
DESCR("less-than");

/* boolgt(bool, bool) => bool */ 
DATA(insert OID = 57 ( boolgt  PGNSP PGUID 12 f f t f i 2 16 f "16 16" _null_ _null_ _null_ boolgt _null_ _null_ n 0));
DESCR("greater-than");

/* booleq(bool, bool) => bool */ 
DATA(insert OID = 60 ( booleq  PGNSP PGUID 12 f f t f i 2 16 f "16 16" _null_ _null_ _null_ booleq _null_ _null_ n 0));
DESCR("equal");

/* chareq("char", "char") => bool */ 
DATA(insert OID = 61 ( chareq  PGNSP PGUID 12 f f t f i 2 16 f "18 18" _null_ _null_ _null_ chareq _null_ _null_ n 0));
DESCR("equal");

/* nameeq(name, name) => bool */ 
DATA(insert OID = 62 ( nameeq  PGNSP PGUID 12 f f t f i 2 16 f "19 19" _null_ _null_ _null_ nameeq _null_ _null_ n 0));
DESCR("equal");

/* int2eq(int2, int2) => bool */ 
DATA(insert OID = 63 ( int2eq  PGNSP PGUID 12 f f t f i 2 16 f "21 21" _null_ _null_ _null_ int2eq _null_ _null_ n 0));
DESCR("equal");

/* int2lt(int2, int2) => bool */ 
DATA(insert OID = 64 ( int2lt  PGNSP PGUID 12 f f t f i 2 16 f "21 21" _null_ _null_ _null_ int2lt _null_ _null_ n 0));
DESCR("less-than");

/* int4eq(int4, int4) => bool */ 
DATA(insert OID = 65 ( int4eq  PGNSP PGUID 12 f f t f i 2 16 f "23 23" _null_ _null_ _null_ int4eq _null_ _null_ n 0));
DESCR("equal");

/* int4lt(int4, int4) => bool */ 
DATA(insert OID = 66 ( int4lt  PGNSP PGUID 12 f f t f i 2 16 f "23 23" _null_ _null_ _null_ int4lt _null_ _null_ n 0));
DESCR("less-than");

/* texteq(text, text) => bool */ 
DATA(insert OID = 67 ( texteq  PGNSP PGUID 12 f f t f i 2 16 f "25 25" _null_ _null_ _null_ texteq _null_ _null_ n 0));
DESCR("equal");

/* xideq(xid, xid) => bool */ 
DATA(insert OID = 68 ( xideq  PGNSP PGUID 12 f f t f i 2 16 f "28 28" _null_ _null_ _null_ xideq _null_ _null_ n 0));
DESCR("equal");

/* cideq(cid, cid) => bool */ 
DATA(insert OID = 69 ( cideq  PGNSP PGUID 12 f f t f i 2 16 f "29 29" _null_ _null_ _null_ cideq _null_ _null_ n 0));
DESCR("equal");

/* charne("char", "char") => bool */ 
DATA(insert OID = 70 ( charne  PGNSP PGUID 12 f f t f i 2 16 f "18 18" _null_ _null_ _null_ charne _null_ _null_ n 0));
DESCR("not equal");

/* charlt("char", "char") => bool */ 
DATA(insert OID = 1246 ( charlt  PGNSP PGUID 12 f f t f i 2 16 f "18 18" _null_ _null_ _null_ charlt _null_ _null_ n 0));
DESCR("less-than");

/* charle("char", "char") => bool */ 
DATA(insert OID = 72 ( charle  PGNSP PGUID 12 f f t f i 2 16 f "18 18" _null_ _null_ _null_ charle _null_ _null_ n 0));
DESCR("less-than-or-equal");

/* chargt("char", "char") => bool */ 
DATA(insert OID = 73 ( chargt  PGNSP PGUID 12 f f t f i 2 16 f "18 18" _null_ _null_ _null_ chargt _null_ _null_ n 0));
DESCR("greater-than");

/* charge("char", "char") => bool */ 
DATA(insert OID = 74 ( charge  PGNSP PGUID 12 f f t f i 2 16 f "18 18" _null_ _null_ _null_ charge _null_ _null_ n 0));
DESCR("greater-than-or-equal");

/* int4("char") => int4 */ 
DATA(insert OID = 77 ( int4  PGNSP PGUID 12 f f t f i 1 23 f "18" _null_ _null_ _null_ chartoi4 _null_ _null_ n 0));
DESCR("convert char to int4");

/* "char"(int4) => "char" */ 
DATA(insert OID = 78 ( char  PGNSP PGUID 12 f f t f i 1 18 f "23" _null_ _null_ _null_ i4tochar _null_ _null_ n 0));
DESCR("convert int4 to char");

/* nameregexeq(name, text) => bool */ 
DATA(insert OID = 79 ( nameregexeq  PGNSP PGUID 12 f f t f i 2 16 f "19 25" _null_ _null_ _null_ nameregexeq _null_ _null_ n 0));
DESCR("matches regex., case-sensitive");

/* nameregexne(name, text) => bool */ 
DATA(insert OID = 1252 ( nameregexne  PGNSP PGUID 12 f f t f i 2 16 f "19 25" _null_ _null_ _null_ nameregexne _null_ _null_ n 0));
DESCR("does not match regex., case-sensitive");

/* textregexeq(text, text) => bool */ 
DATA(insert OID = 1254 ( textregexeq  PGNSP PGUID 12 f f t f i 2 16 f "25 25" _null_ _null_ _null_ textregexeq _null_ _null_ n 0));
DESCR("matches regex., case-sensitive");

/* textregexne(text, text) => bool */ 
DATA(insert OID = 1256 ( textregexne  PGNSP PGUID 12 f f t f i 2 16 f "25 25" _null_ _null_ _null_ textregexne _null_ _null_ n 0));
DESCR("does not match regex., case-sensitive");

/* textlen(text) => int4 */ 
DATA(insert OID = 1257 ( textlen  PGNSP PGUID 12 f f t f i 1 23 f "25" _null_ _null_ _null_ textlen _null_ _null_ n 0));
DESCR("length");

/* textcat(text, text) => text */ 
DATA(insert OID = 1258 ( textcat  PGNSP PGUID 12 f f t f i 2 25 f "25 25" _null_ _null_ _null_ textcat _null_ _null_ n 0));
DESCR("concatenate");

/* boolne(bool, bool) => bool */ 
DATA(insert OID = 84 ( boolne  PGNSP PGUID 12 f f t f i 2 16 f "16 16" _null_ _null_ _null_ boolne _null_ _null_ n 0));
DESCR("not equal");

/* version() => text */ 
DATA(insert OID = 89 ( version  PGNSP PGUID 12 f f t f s 0 25 f "" _null_ _null_ _null_ pgsql_version _null_ _null_ n 0));
DESCR("PostgreSQL version string");


/* OIDS 100 - 199 */
/* eqsel(internal, oid, internal, int4) => float8 */ 
DATA(insert OID = 101 ( eqsel  PGNSP PGUID 12 f f t f s 4 701 f "2281 26 2281 23" _null_ _null_ _null_ eqsel _null_ _null_ n 0));
DESCR("restriction selectivity of = and related operators");

/* neqsel(internal, oid, internal, int4) => float8 */ 
DATA(insert OID = 102 ( neqsel  PGNSP PGUID 12 f f t f s 4 701 f "2281 26 2281 23" _null_ _null_ _null_ neqsel _null_ _null_ n 0));
DESCR("restriction selectivity of <> and related operators");

/* scalarltsel(internal, oid, internal, int4) => float8 */ 
DATA(insert OID = 103 ( scalarltsel  PGNSP PGUID 12 f f t f s 4 701 f "2281 26 2281 23" _null_ _null_ _null_ scalarltsel _null_ _null_ n 0));
DESCR("restriction selectivity of < and related operators on scalar datatypes");

/* scalargtsel(internal, oid, internal, int4) => float8 */ 
DATA(insert OID = 104 ( scalargtsel  PGNSP PGUID 12 f f t f s 4 701 f "2281 26 2281 23" _null_ _null_ _null_ scalargtsel _null_ _null_ n 0));
DESCR("restriction selectivity of > and related operators on scalar datatypes");

/* eqjoinsel(internal, oid, internal, int2) => float8 */ 
DATA(insert OID = 105 ( eqjoinsel  PGNSP PGUID 12 f f t f s 4 701 f "2281 26 2281 21" _null_ _null_ _null_ eqjoinsel _null_ _null_ n 0));
DESCR("join selectivity of = and related operators");

/* neqjoinsel(internal, oid, internal, int2) => float8 */ 
DATA(insert OID = 106 ( neqjoinsel  PGNSP PGUID 12 f f t f s 4 701 f "2281 26 2281 21" _null_ _null_ _null_ neqjoinsel _null_ _null_ n 0));
DESCR("join selectivity of <> and related operators");

/* scalarltjoinsel(internal, oid, internal, int2) => float8 */ 
DATA(insert OID = 107 ( scalarltjoinsel  PGNSP PGUID 12 f f t f s 4 701 f "2281 26 2281 21" _null_ _null_ _null_ scalarltjoinsel _null_ _null_ n 0));
DESCR("join selectivity of < and related operators on scalar datatypes");

/* scalargtjoinsel(internal, oid, internal, int2) => float8 */ 
DATA(insert OID = 108 ( scalargtjoinsel  PGNSP PGUID 12 f f t f s 4 701 f "2281 26 2281 21" _null_ _null_ _null_ scalargtjoinsel _null_ _null_ n 0));
DESCR("join selectivity of > and related operators on scalar datatypes");

/* unknownin(cstring) => unknown */ 
DATA(insert OID = 109 ( unknownin  PGNSP PGUID 12 f f t f i 1 705 f "2275" _null_ _null_ _null_ unknownin _null_ _null_ n 0));
DESCR("I/O");

/* unknownout(unknown) => cstring */ 
DATA(insert OID = 110 ( unknownout  PGNSP PGUID 12 f f t f i 1 2275 f "705" _null_ _null_ _null_ unknownout _null_ _null_ n 0));
DESCR("I/O");

/* numeric_fac(int8) => "numeric" */ 
DATA(insert OID = 111 ( numeric_fac  PGNSP PGUID 12 f f t f i 1 1700 f "20" _null_ _null_ _null_ numeric_fac _null_ _null_ n 0));

/* text(int4) => text */ 
DATA(insert OID = 112 ( text  PGNSP PGUID 12 f f t f i 1 25 f "23" _null_ _null_ _null_ int4_text _null_ _null_ n 0));
DESCR("convert int4 to text");

/* text(int2) => text */ 
DATA(insert OID = 113 ( text  PGNSP PGUID 12 f f t f i 1 25 f "21" _null_ _null_ _null_ int2_text _null_ _null_ n 0));
DESCR("convert int2 to text");

/* text(oid) => text */ 
DATA(insert OID = 114 ( text  PGNSP PGUID 12 f f t f i 1 25 f "26" _null_ _null_ _null_ oid_text _null_ _null_ n 0));
DESCR("convert oid to text");

/* box_above_eq(box, box) => bool */ 
DATA(insert OID = 115 ( box_above_eq  PGNSP PGUID 12 f f t f i 2 16 f "603 603" _null_ _null_ _null_ box_above_eq _null_ _null_ n 0));
DESCR("is above (allows touching)");

/* box_below_eq(box, box) => bool */ 
DATA(insert OID = 116 ( box_below_eq  PGNSP PGUID 12 f f t f i 2 16 f "603 603" _null_ _null_ _null_ box_below_eq _null_ _null_ n 0));
DESCR("is below (allows touching)");

/* point_in(cstring) => point */ 
DATA(insert OID = 117 ( point_in  PGNSP PGUID 12 f f t f i 1 600 f "2275" _null_ _null_ _null_ point_in _null_ _null_ n 0));
DESCR("I/O");

/* point_out(point) => cstring */ 
DATA(insert OID = 118 ( point_out  PGNSP PGUID 12 f f t f i 1 2275 f "600" _null_ _null_ _null_ point_out _null_ _null_ n 0));
DESCR("I/O");

/* lseg_in(cstring) => lseg */ 
DATA(insert OID = 119 ( lseg_in  PGNSP PGUID 12 f f t f i 1 601 f "2275" _null_ _null_ _null_ lseg_in _null_ _null_ n 0));
DESCR("I/O");

/* lseg_out(lseg) => cstring */ 
DATA(insert OID = 120 ( lseg_out  PGNSP PGUID 12 f f t f i 1 2275 f "601" _null_ _null_ _null_ lseg_out _null_ _null_ n 0));
DESCR("I/O");

/* path_in(cstring) => path */ 
DATA(insert OID = 121 ( path_in  PGNSP PGUID 12 f f t f i 1 602 f "2275" _null_ _null_ _null_ path_in _null_ _null_ n 0));
DESCR("I/O");

/* path_out(path) => cstring */ 
DATA(insert OID = 122 ( path_out  PGNSP PGUID 12 f f t f i 1 2275 f "602" _null_ _null_ _null_ path_out _null_ _null_ n 0));
DESCR("I/O");

/* box_in(cstring) => box */ 
DATA(insert OID = 123 ( box_in  PGNSP PGUID 12 f f t f i 1 603 f "2275" _null_ _null_ _null_ box_in _null_ _null_ n 0));
DESCR("I/O");

/* box_out(box) => cstring */ 
DATA(insert OID = 124 ( box_out  PGNSP PGUID 12 f f t f i 1 2275 f "603" _null_ _null_ _null_ box_out _null_ _null_ n 0));
DESCR("I/O");

/* box_overlap(box, box) => bool */ 
DATA(insert OID = 125 ( box_overlap  PGNSP PGUID 12 f f t f i 2 16 f "603 603" _null_ _null_ _null_ box_overlap _null_ _null_ n 0));
DESCR("overlaps");

/* box_ge(box, box) => bool */ 
DATA(insert OID = 126 ( box_ge  PGNSP PGUID 12 f f t f i 2 16 f "603 603" _null_ _null_ _null_ box_ge _null_ _null_ n 0));
DESCR("greater-than-or-equal by area");

/* box_gt(box, box) => bool */ 
DATA(insert OID = 127 ( box_gt  PGNSP PGUID 12 f f t f i 2 16 f "603 603" _null_ _null_ _null_ box_gt _null_ _null_ n 0));
DESCR("greater-than by area");

/* box_eq(box, box) => bool */ 
DATA(insert OID = 128 ( box_eq  PGNSP PGUID 12 f f t f i 2 16 f "603 603" _null_ _null_ _null_ box_eq _null_ _null_ n 0));
DESCR("equal by area");

/* box_lt(box, box) => bool */ 
DATA(insert OID = 129 ( box_lt  PGNSP PGUID 12 f f t f i 2 16 f "603 603" _null_ _null_ _null_ box_lt _null_ _null_ n 0));
DESCR("less-than by area");

/* box_le(box, box) => bool */ 
DATA(insert OID = 130 ( box_le  PGNSP PGUID 12 f f t f i 2 16 f "603 603" _null_ _null_ _null_ box_le _null_ _null_ n 0));
DESCR("less-than-or-equal by area");

/* point_above(point, point) => bool */ 
DATA(insert OID = 131 ( point_above  PGNSP PGUID 12 f f t f i 2 16 f "600 600" _null_ _null_ _null_ point_above _null_ _null_ n 0));
DESCR("is above");

/* point_left(point, point) => bool */ 
DATA(insert OID = 132 ( point_left  PGNSP PGUID 12 f f t f i 2 16 f "600 600" _null_ _null_ _null_ point_left _null_ _null_ n 0));
DESCR("is left of");

/* point_right(point, point) => bool */ 
DATA(insert OID = 133 ( point_right  PGNSP PGUID 12 f f t f i 2 16 f "600 600" _null_ _null_ _null_ point_right _null_ _null_ n 0));
DESCR("is right of");

/* point_below(point, point) => bool */ 
DATA(insert OID = 134 ( point_below  PGNSP PGUID 12 f f t f i 2 16 f "600 600" _null_ _null_ _null_ point_below _null_ _null_ n 0));
DESCR("is below");

/* point_eq(point, point) => bool */ 
DATA(insert OID = 135 ( point_eq  PGNSP PGUID 12 f f t f i 2 16 f "600 600" _null_ _null_ _null_ point_eq _null_ _null_ n 0));
DESCR("same as?");

/* on_pb(point, box) => bool */ 
DATA(insert OID = 136 ( on_pb  PGNSP PGUID 12 f f t f i 2 16 f "600 603" _null_ _null_ _null_ on_pb _null_ _null_ n 0));
DESCR("point inside box?");

/* on_ppath(point, path) => bool */ 
DATA(insert OID = 137 ( on_ppath  PGNSP PGUID 12 f f t f i 2 16 f "600 602" _null_ _null_ _null_ on_ppath _null_ _null_ n 0));
DESCR("point within closed path, or point on open path");

/* box_center(box) => point */ 
DATA(insert OID = 138 ( box_center  PGNSP PGUID 12 f f t f i 1 600 f "603" _null_ _null_ _null_ box_center _null_ _null_ n 0));
DESCR("center of");

/* areasel(internal, oid, internal, int4) => float8 */ 
DATA(insert OID = 139 ( areasel  PGNSP PGUID 12 f f t f s 4 701 f "2281 26 2281 23" _null_ _null_ _null_ areasel _null_ _null_ n 0));
DESCR("restriction selectivity for area-comparison operators");

/* areajoinsel(internal, oid, internal, int2) => float8 */ 
DATA(insert OID = 140 ( areajoinsel  PGNSP PGUID 12 f f t f s 4 701 f "2281 26 2281 21" _null_ _null_ _null_ areajoinsel _null_ _null_ n 0));
DESCR("join selectivity for area-comparison operators");

/* int4mul(int4, int4) => int4 */ 
DATA(insert OID = 141 ( int4mul  PGNSP PGUID 12 f f t f i 2 23 f "23 23" _null_ _null_ _null_ int4mul _null_ _null_ n 0));
DESCR("multiply");

/* int4ne(int4, int4) => bool */ 
DATA(insert OID = 144 ( int4ne  PGNSP PGUID 12 f f t f i 2 16 f "23 23" _null_ _null_ _null_ int4ne _null_ _null_ n 0));
DESCR("not equal");

/* int2ne(int2, int2) => bool */ 
DATA(insert OID = 145 ( int2ne  PGNSP PGUID 12 f f t f i 2 16 f "21 21" _null_ _null_ _null_ int2ne _null_ _null_ n 0));
DESCR("not equal");

/* int2gt(int2, int2) => bool */ 
DATA(insert OID = 146 ( int2gt  PGNSP PGUID 12 f f t f i 2 16 f "21 21" _null_ _null_ _null_ int2gt _null_ _null_ n 0));
DESCR("greater-than");

/* int4gt(int4, int4) => bool */ 
DATA(insert OID = 147 ( int4gt  PGNSP PGUID 12 f f t f i 2 16 f "23 23" _null_ _null_ _null_ int4gt _null_ _null_ n 0));
DESCR("greater-than");

/* int2le(int2, int2) => bool */ 
DATA(insert OID = 148 ( int2le  PGNSP PGUID 12 f f t f i 2 16 f "21 21" _null_ _null_ _null_ int2le _null_ _null_ n 0));
DESCR("less-than-or-equal");

/* int4le(int4, int4) => bool */ 
DATA(insert OID = 149 ( int4le  PGNSP PGUID 12 f f t f i 2 16 f "23 23" _null_ _null_ _null_ int4le _null_ _null_ n 0));
DESCR("less-than-or-equal");

/* int4ge(int4, int4) => bool */ 
DATA(insert OID = 150 ( int4ge  PGNSP PGUID 12 f f t f i 2 16 f "23 23" _null_ _null_ _null_ int4ge _null_ _null_ n 0));
DESCR("greater-than-or-equal");

/* int2ge(int2, int2) => bool */ 
DATA(insert OID = 151 ( int2ge  PGNSP PGUID 12 f f t f i 2 16 f "21 21" _null_ _null_ _null_ int2ge _null_ _null_ n 0));
DESCR("greater-than-or-equal");

/* int2mul(int2, int2) => int2 */ 
DATA(insert OID = 152 ( int2mul  PGNSP PGUID 12 f f t f i 2 21 f "21 21" _null_ _null_ _null_ int2mul _null_ _null_ n 0));
DESCR("multiply");

/* int2div(int2, int2) => int2 */ 
DATA(insert OID = 153 ( int2div  PGNSP PGUID 12 f f t f i 2 21 f "21 21" _null_ _null_ _null_ int2div _null_ _null_ n 0));
DESCR("divide");

/* int4div(int4, int4) => int4 */ 
DATA(insert OID = 154 ( int4div  PGNSP PGUID 12 f f t f i 2 23 f "23 23" _null_ _null_ _null_ int4div _null_ _null_ n 0));
DESCR("divide");

/* int2mod(int2, int2) => int2 */ 
DATA(insert OID = 155 ( int2mod  PGNSP PGUID 12 f f t f i 2 21 f "21 21" _null_ _null_ _null_ int2mod _null_ _null_ n 0));
DESCR("modulus");

/* int4mod(int4, int4) => int4 */ 
DATA(insert OID = 156 ( int4mod  PGNSP PGUID 12 f f t f i 2 23 f "23 23" _null_ _null_ _null_ int4mod _null_ _null_ n 0));
DESCR("modulus");

/* textne(text, text) => bool */ 
DATA(insert OID = 157 ( textne  PGNSP PGUID 12 f f t f i 2 16 f "25 25" _null_ _null_ _null_ textne _null_ _null_ n 0));
DESCR("not equal");

/* int24eq(int2, int4) => bool */ 
DATA(insert OID = 158 ( int24eq  PGNSP PGUID 12 f f t f i 2 16 f "21 23" _null_ _null_ _null_ int24eq _null_ _null_ n 0));
DESCR("equal");

/* int42eq(int4, int2) => bool */ 
DATA(insert OID = 159 ( int42eq  PGNSP PGUID 12 f f t f i 2 16 f "23 21" _null_ _null_ _null_ int42eq _null_ _null_ n 0));
DESCR("equal");

/* int24lt(int2, int4) => bool */ 
DATA(insert OID = 160 ( int24lt  PGNSP PGUID 12 f f t f i 2 16 f "21 23" _null_ _null_ _null_ int24lt _null_ _null_ n 0));
DESCR("less-than");

/* int42lt(int4, int2) => bool */ 
DATA(insert OID = 161 ( int42lt  PGNSP PGUID 12 f f t f i 2 16 f "23 21" _null_ _null_ _null_ int42lt _null_ _null_ n 0));
DESCR("less-than");

/* int24gt(int2, int4) => bool */ 
DATA(insert OID = 162 ( int24gt  PGNSP PGUID 12 f f t f i 2 16 f "21 23" _null_ _null_ _null_ int24gt _null_ _null_ n 0));
DESCR("greater-than");

/* int42gt(int4, int2) => bool */ 
DATA(insert OID = 163 ( int42gt  PGNSP PGUID 12 f f t f i 2 16 f "23 21" _null_ _null_ _null_ int42gt _null_ _null_ n 0));
DESCR("greater-than");

/* int24ne(int2, int4) => bool */ 
DATA(insert OID = 164 ( int24ne  PGNSP PGUID 12 f f t f i 2 16 f "21 23" _null_ _null_ _null_ int24ne _null_ _null_ n 0));
DESCR("not equal");

/* int42ne(int4, int2) => bool */ 
DATA(insert OID = 165 ( int42ne  PGNSP PGUID 12 f f t f i 2 16 f "23 21" _null_ _null_ _null_ int42ne _null_ _null_ n 0));
DESCR("not equal");

/* int24le(int2, int4) => bool */ 
DATA(insert OID = 166 ( int24le  PGNSP PGUID 12 f f t f i 2 16 f "21 23" _null_ _null_ _null_ int24le _null_ _null_ n 0));
DESCR("less-than-or-equal");

/* int42le(int4, int2) => bool */ 
DATA(insert OID = 167 ( int42le  PGNSP PGUID 12 f f t f i 2 16 f "23 21" _null_ _null_ _null_ int42le _null_ _null_ n 0));
DESCR("less-than-or-equal");

/* int24ge(int2, int4) => bool */ 
DATA(insert OID = 168 ( int24ge  PGNSP PGUID 12 f f t f i 2 16 f "21 23" _null_ _null_ _null_ int24ge _null_ _null_ n 0));
DESCR("greater-than-or-equal");

/* int42ge(int4, int2) => bool */ 
DATA(insert OID = 169 ( int42ge  PGNSP PGUID 12 f f t f i 2 16 f "23 21" _null_ _null_ _null_ int42ge _null_ _null_ n 0));
DESCR("greater-than-or-equal");

/* int24mul(int2, int4) => int4 */ 
DATA(insert OID = 170 ( int24mul  PGNSP PGUID 12 f f t f i 2 23 f "21 23" _null_ _null_ _null_ int24mul _null_ _null_ n 0));
DESCR("multiply");

/* int42mul(int4, int2) => int4 */ 
DATA(insert OID = 171 ( int42mul  PGNSP PGUID 12 f f t f i 2 23 f "23 21" _null_ _null_ _null_ int42mul _null_ _null_ n 0));
DESCR("multiply");

/* int24div(int2, int4) => int4 */ 
DATA(insert OID = 172 ( int24div  PGNSP PGUID 12 f f t f i 2 23 f "21 23" _null_ _null_ _null_ int24div _null_ _null_ n 0));
DESCR("divide");

/* int42div(int4, int2) => int4 */ 
DATA(insert OID = 173 ( int42div  PGNSP PGUID 12 f f t f i 2 23 f "23 21" _null_ _null_ _null_ int42div _null_ _null_ n 0));
DESCR("divide");

/* int24mod(int2, int4) => int4 */ 
DATA(insert OID = 174 ( int24mod  PGNSP PGUID 12 f f t f i 2 23 f "21 23" _null_ _null_ _null_ int24mod _null_ _null_ n 0));
DESCR("modulus");

/* int42mod(int4, int2) => int4 */ 
DATA(insert OID = 175 ( int42mod  PGNSP PGUID 12 f f t f i 2 23 f "23 21" _null_ _null_ _null_ int42mod _null_ _null_ n 0));
DESCR("modulus");

/* int2pl(int2, int2) => int2 */ 
DATA(insert OID = 176 ( int2pl  PGNSP PGUID 12 f f t f i 2 21 f "21 21" _null_ _null_ _null_ int2pl _null_ _null_ n 0));
DESCR("add");

/* int4pl(int4, int4) => int4 */ 
DATA(insert OID = 177 ( int4pl  PGNSP PGUID 12 f f t f i 2 23 f "23 23" _null_ _null_ _null_ int4pl _null_ _null_ n 0));
DESCR("add");

/* int24pl(int2, int4) => int4 */ 
DATA(insert OID = 178 ( int24pl  PGNSP PGUID 12 f f t f i 2 23 f "21 23" _null_ _null_ _null_ int24pl _null_ _null_ n 0));
DESCR("add");

/* int42pl(int4, int2) => int4 */ 
DATA(insert OID = 179 ( int42pl  PGNSP PGUID 12 f f t f i 2 23 f "23 21" _null_ _null_ _null_ int42pl _null_ _null_ n 0));
DESCR("add");

/* int2mi(int2, int2) => int2 */ 
DATA(insert OID = 180 ( int2mi  PGNSP PGUID 12 f f t f i 2 21 f "21 21" _null_ _null_ _null_ int2mi _null_ _null_ n 0));
DESCR("subtract");

/* int4mi(int4, int4) => int4 */ 
DATA(insert OID = 181 ( int4mi  PGNSP PGUID 12 f f t f i 2 23 f "23 23" _null_ _null_ _null_ int4mi _null_ _null_ n 0));
DESCR("subtract");

/* int24mi(int2, int4) => int4 */ 
DATA(insert OID = 182 ( int24mi  PGNSP PGUID 12 f f t f i 2 23 f "21 23" _null_ _null_ _null_ int24mi _null_ _null_ n 0));
DESCR("subtract");

/* int42mi(int4, int2) => int4 */ 
DATA(insert OID = 183 ( int42mi  PGNSP PGUID 12 f f t f i 2 23 f "23 21" _null_ _null_ _null_ int42mi _null_ _null_ n 0));
DESCR("subtract");

/* oideq(oid, oid) => bool */ 
DATA(insert OID = 184 ( oideq  PGNSP PGUID 12 f f t f i 2 16 f "26 26" _null_ _null_ _null_ oideq _null_ _null_ n 0));
DESCR("equal");

/* oidne(oid, oid) => bool */ 
DATA(insert OID = 185 ( oidne  PGNSP PGUID 12 f f t f i 2 16 f "26 26" _null_ _null_ _null_ oidne _null_ _null_ n 0));
DESCR("not equal");

/* box_same(box, box) => bool */ 
DATA(insert OID = 186 ( box_same  PGNSP PGUID 12 f f t f i 2 16 f "603 603" _null_ _null_ _null_ box_same _null_ _null_ n 0));
DESCR("same as?");

/* box_contain(box, box) => bool */ 
DATA(insert OID = 187 ( box_contain  PGNSP PGUID 12 f f t f i 2 16 f "603 603" _null_ _null_ _null_ box_contain _null_ _null_ n 0));
DESCR("contains?");

/* box_left(box, box) => bool */ 
DATA(insert OID = 188 ( box_left  PGNSP PGUID 12 f f t f i 2 16 f "603 603" _null_ _null_ _null_ box_left _null_ _null_ n 0));
DESCR("is left of");

/* box_overleft(box, box) => bool */ 
DATA(insert OID = 189 ( box_overleft  PGNSP PGUID 12 f f t f i 2 16 f "603 603" _null_ _null_ _null_ box_overleft _null_ _null_ n 0));
DESCR("overlaps or is left of");

/* box_overright(box, box) => bool */ 
DATA(insert OID = 190 ( box_overright  PGNSP PGUID 12 f f t f i 2 16 f "603 603" _null_ _null_ _null_ box_overright _null_ _null_ n 0));
DESCR("overlaps or is right of");

/* box_right(box, box) => bool */ 
DATA(insert OID = 191 ( box_right  PGNSP PGUID 12 f f t f i 2 16 f "603 603" _null_ _null_ _null_ box_right _null_ _null_ n 0));
DESCR("is right of");

/* box_contained(box, box) => bool */ 
DATA(insert OID = 192 ( box_contained  PGNSP PGUID 12 f f t f i 2 16 f "603 603" _null_ _null_ _null_ box_contained _null_ _null_ n 0));
DESCR("is contained by?");


/* OIDS 200 - 299 */
/* float4in(cstring) => float4 */ 
DATA(insert OID = 200 ( float4in  PGNSP PGUID 12 f f t f i 1 700 f "2275" _null_ _null_ _null_ float4in _null_ _null_ n 0));
DESCR("I/O");

/* float4out(float4) => cstring */ 
DATA(insert OID = 201 ( float4out  PGNSP PGUID 12 f f t f i 1 2275 f "700" _null_ _null_ _null_ float4out _null_ _null_ n 0));
DESCR("I/O");

/* float4mul(float4, float4) => float4 */ 
DATA(insert OID = 202 ( float4mul  PGNSP PGUID 12 f f t f i 2 700 f "700 700" _null_ _null_ _null_ float4mul _null_ _null_ n 0));
DESCR("multiply");

/* float4div(float4, float4) => float4 */ 
DATA(insert OID = 203 ( float4div  PGNSP PGUID 12 f f t f i 2 700 f "700 700" _null_ _null_ _null_ float4div _null_ _null_ n 0));
DESCR("divide");

/* float4pl(float4, float4) => float4 */ 
DATA(insert OID = 204 ( float4pl  PGNSP PGUID 12 f f t f i 2 700 f "700 700" _null_ _null_ _null_ float4pl _null_ _null_ n 0));
DESCR("add");

/* float4mi(float4, float4) => float4 */ 
DATA(insert OID = 205 ( float4mi  PGNSP PGUID 12 f f t f i 2 700 f "700 700" _null_ _null_ _null_ float4mi _null_ _null_ n 0));
DESCR("subtract");

/* float4um(float4) => float4 */ 
DATA(insert OID = 206 ( float4um  PGNSP PGUID 12 f f t f i 1 700 f "700" _null_ _null_ _null_ float4um _null_ _null_ n 0));
DESCR("negate");

/* float4abs(float4) => float4 */ 
DATA(insert OID = 207 ( float4abs  PGNSP PGUID 12 f f t f i 1 700 f "700" _null_ _null_ _null_ float4abs _null_ _null_ n 0));
DESCR("absolute value");

/* float4_accum(_float8, float4) => _float8 */ 
DATA(insert OID = 208 ( float4_accum  PGNSP PGUID 12 f f t f i 2 1022 f "1022 700" _null_ _null_ _null_ float4_accum _null_ _null_ n 0));
DESCR("aggregate transition function");

/* float4_decum(_float8, float4) => _float8 */ 
DATA(insert OID = 6024 ( float4_decum  PGNSP PGUID 12 f f t f i 2 1022 f "1022 700" _null_ _null_ _null_ float4_decum _null_ _null_ n 0));
DESCR("aggregate inverse transition function");

/* float4_avg_accum(bytea, float4) => bytea */ 
DATA(insert OID = 3106 ( float4_avg_accum  PGNSP PGUID 12 f f t f i 2 17 f "17 700" _null_ _null_ _null_ float4_avg_accum _null_ _null_ n 0));
DESCR("aggregate transition function");

/* float4_avg_decum(bytea, float4) => bytea */ 
DATA(insert OID = 3107 ( float4_avg_decum  PGNSP PGUID 12 f f t f i 2 17 f "17 700" _null_ _null_ _null_ float4_avg_decum _null_ _null_ n 0));
DESCR("aggregate inverse transition function");

/* float4larger(float4, float4) => float4 */ 
DATA(insert OID = 209 ( float4larger  PGNSP PGUID 12 f f t f i 2 700 f "700 700" _null_ _null_ _null_ float4larger _null_ _null_ n 0));
DESCR("larger of two");

/* float4smaller(float4, float4) => float4 */ 
DATA(insert OID = 211 ( float4smaller  PGNSP PGUID 12 f f t f i 2 700 f "700 700" _null_ _null_ _null_ float4smaller _null_ _null_ n 0));
DESCR("smaller of two");

/* int4um(int4) => int4 */ 
DATA(insert OID = 212 ( int4um  PGNSP PGUID 12 f f t f i 1 23 f "23" _null_ _null_ _null_ int4um _null_ _null_ n 0));
DESCR("negate");

/* int2um(int2) => int2 */ 
DATA(insert OID = 213 ( int2um  PGNSP PGUID 12 f f t f i 1 21 f "21" _null_ _null_ _null_ int2um _null_ _null_ n 0));
DESCR("negate");

/* float8in(cstring) => float8 */ 
DATA(insert OID = 214 ( float8in  PGNSP PGUID 12 f f t f i 1 701 f "2275" _null_ _null_ _null_ float8in _null_ _null_ n 0));
DESCR("I/O");

/* float8out(float8) => cstring */ 
DATA(insert OID = 215 ( float8out  PGNSP PGUID 12 f f t f i 1 2275 f "701" _null_ _null_ _null_ float8out _null_ _null_ n 0));
DESCR("I/O");

/* float8mul(float8, float8) => float8 */ 
DATA(insert OID = 216 ( float8mul  PGNSP PGUID 12 f f t f i 2 701 f "701 701" _null_ _null_ _null_ float8mul _null_ _null_ n 0));
DESCR("multiply");

/* float8div(float8, float8) => float8 */ 
DATA(insert OID = 217 ( float8div  PGNSP PGUID 12 f f t f i 2 701 f "701 701" _null_ _null_ _null_ float8div _null_ _null_ n 0));
DESCR("divide");

/* float8pl(float8, float8) => float8 */ 
DATA(insert OID = 218 ( float8pl  PGNSP PGUID 12 f f t f i 2 701 f "701 701" _null_ _null_ _null_ float8pl _null_ _null_ n 0));
DESCR("add");

/* float8mi(float8, float8) => float8 */ 
DATA(insert OID = 219 ( float8mi  PGNSP PGUID 12 f f t f i 2 701 f "701 701" _null_ _null_ _null_ float8mi _null_ _null_ n 0));
DESCR("subtract");

/* float8um(float8) => float8 */ 
DATA(insert OID = 220 ( float8um  PGNSP PGUID 12 f f t f i 1 701 f "701" _null_ _null_ _null_ float8um _null_ _null_ n 0));
DESCR("negate");

/* float8abs(float8) => float8 */ 
DATA(insert OID = 221 ( float8abs  PGNSP PGUID 12 f f t f i 1 701 f "701" _null_ _null_ _null_ float8abs _null_ _null_ n 0));
DESCR("absolute value");

/* float8_accum(_float8, float8) => _float8 */ 
DATA(insert OID = 222 ( float8_accum  PGNSP PGUID 12 f f t f i 2 1022 f "1022 701" _null_ _null_ _null_ float8_accum _null_ _null_ n 0));
DESCR("aggregate transition function");

/* float8_decum(_float8, float8) => _float8 */ 
DATA(insert OID = 6025 ( float8_decum  PGNSP PGUID 12 f f t f i 2 1022 f "1022 701" _null_ _null_ _null_ float8_decum _null_ _null_ n 0));
DESCR("aggregate inverse transition function");

/* float8_avg_accum(bytea, float8) => bytea */ 
DATA(insert OID = 3108 ( float8_avg_accum  PGNSP PGUID 12 f f t f i 2 17 f "17 701" _null_ _null_ _null_ float8_avg_accum _null_ _null_ n 0));
DESCR("aggregate transition function");

/* float8_avg_decum(bytea, float8) => bytea */ 
DATA(insert OID = 3109 ( float8_avg_decum  PGNSP PGUID 12 f f t f i 2 17 f "17 701" _null_ _null_ _null_ float8_avg_decum _null_ _null_ n 0));
DESCR("aggregate inverse transition function");

/* float8larger(float8, float8) => float8 */ 
DATA(insert OID = 223 ( float8larger  PGNSP PGUID 12 f f t f i 2 701 f "701 701" _null_ _null_ _null_ float8larger _null_ _null_ n 0));
DESCR("larger of two");

/* float8smaller(float8, float8) => float8 */ 
DATA(insert OID = 224 ( float8smaller  PGNSP PGUID 12 f f t f i 2 701 f "701 701" _null_ _null_ _null_ float8smaller _null_ _null_ n 0));
DESCR("smaller of two");

/* lseg_center(lseg) => point */ 
DATA(insert OID = 225 ( lseg_center  PGNSP PGUID 12 f f t f i 1 600 f "601" _null_ _null_ _null_ lseg_center _null_ _null_ n 0));
DESCR("center of");

/* path_center(path) => point */ 
DATA(insert OID = 226 ( path_center  PGNSP PGUID 12 f f t f i 1 600 f "602" _null_ _null_ _null_ path_center _null_ _null_ n 0));
DESCR("center of");

/* poly_center(polygon) => point */ 
DATA(insert OID = 227 ( poly_center  PGNSP PGUID 12 f f t f i 1 600 f "604" _null_ _null_ _null_ poly_center _null_ _null_ n 0));
DESCR("center of");

/* dround(float8) => float8 */ 
DATA(insert OID = 228 ( dround  PGNSP PGUID 12 f f t f i 1 701 f "701" _null_ _null_ _null_ dround _null_ _null_ n 0));
DESCR("round to nearest integer");

/* dtrunc(float8) => float8 */ 
DATA(insert OID = 229 ( dtrunc  PGNSP PGUID 12 f f t f i 1 701 f "701" _null_ _null_ _null_ dtrunc _null_ _null_ n 0));
DESCR("truncate to integer");

/* ceil(float8) => float8 */ 
DATA(insert OID = 2308 ( ceil  PGNSP PGUID 12 f f t f i 1 701 f "701" _null_ _null_ _null_ dceil _null_ _null_ n 0));
DESCR("smallest integer >= value");

/* ceiling(float8) => float8 */ 
DATA(insert OID = 2320 ( ceiling  PGNSP PGUID 12 f f t f i 1 701 f "701" _null_ _null_ _null_ dceil _null_ _null_ n 0));
DESCR("smallest integer >= value");

/* floor(float8) => float8 */ 
DATA(insert OID = 2309 ( floor  PGNSP PGUID 12 f f t f i 1 701 f "701" _null_ _null_ _null_ dfloor _null_ _null_ n 0));
DESCR("largest integer <= value");

/* sign(float8) => float8 */ 
DATA(insert OID = 2310 ( sign  PGNSP PGUID 12 f f t f i 1 701 f "701" _null_ _null_ _null_ dsign _null_ _null_ n 0));
DESCR("sign of value");

/* dsqrt(float8) => float8 */ 
DATA(insert OID = 230 ( dsqrt  PGNSP PGUID 12 f f t f i 1 701 f "701" _null_ _null_ _null_ dsqrt _null_ _null_ n 0));
DESCR("square root");

/* dcbrt(float8) => float8 */ 
DATA(insert OID = 231 ( dcbrt  PGNSP PGUID 12 f f t f i 1 701 f "701" _null_ _null_ _null_ dcbrt _null_ _null_ n 0));
DESCR("cube root");

/* dpow(float8, float8) => float8 */ 
DATA(insert OID = 232 ( dpow  PGNSP PGUID 12 f f t f i 2 701 f "701 701" _null_ _null_ _null_ dpow _null_ _null_ n 0));
DESCR("exponentiation (x^y)");

/* dexp(float8) => float8 */ 
DATA(insert OID = 233 ( dexp  PGNSP PGUID 12 f f t f i 1 701 f "701" _null_ _null_ _null_ dexp _null_ _null_ n 0));
DESCR("natural exponential (e^x)");

/* dlog1(float8) => float8 */ 
DATA(insert OID = 234 ( dlog1  PGNSP PGUID 12 f f t f i 1 701 f "701" _null_ _null_ _null_ dlog1 _null_ _null_ n 0));
DESCR("natural logarithm");

/* float8(int2) => float8 */ 
DATA(insert OID = 235 ( float8  PGNSP PGUID 12 f f t f i 1 701 f "21" _null_ _null_ _null_ i2tod _null_ _null_ n 0));
DESCR("convert int2 to float8");

/* float4(int2) => float4 */ 
DATA(insert OID = 236 ( float4  PGNSP PGUID 12 f f t f i 1 700 f "21" _null_ _null_ _null_ i2tof _null_ _null_ n 0));
DESCR("convert int2 to float4");

/* int2(float8) => int2 */ 
DATA(insert OID = 237 ( int2  PGNSP PGUID 12 f f t f i 1 21 f "701" _null_ _null_ _null_ dtoi2 _null_ _null_ n 0));
DESCR("convert float8 to int2");

/* int2(float4) => int2 */ 
DATA(insert OID = 238 ( int2  PGNSP PGUID 12 f f t f i 1 21 f "700" _null_ _null_ _null_ ftoi2 _null_ _null_ n 0));
DESCR("convert float4 to int2");

/* line_distance(line, line) => float8 */ 
DATA(insert OID = 239 ( line_distance  PGNSP PGUID 12 f f t f i 2 701 f "628 628" _null_ _null_ _null_ line_distance _null_ _null_ n 0));
DESCR("distance between");

/* abstimein(cstring) => abstime */ 
DATA(insert OID = 240 ( abstimein  PGNSP PGUID 12 f f t f s 1 702 f "2275" _null_ _null_ _null_ abstimein _null_ _null_ n 0));
DESCR("I/O");

/* abstimeout(abstime) => cstring */ 
DATA(insert OID = 241 ( abstimeout  PGNSP PGUID 12 f f t f s 1 2275 f "702" _null_ _null_ _null_ abstimeout _null_ _null_ n 0));
DESCR("I/O");

/* reltimein(cstring) => reltime */ 
DATA(insert OID = 242 ( reltimein  PGNSP PGUID 12 f f t f s 1 703 f "2275" _null_ _null_ _null_ reltimein _null_ _null_ n 0));
DESCR("I/O");

/* reltimeout(reltime) => cstring */ 
DATA(insert OID = 243 ( reltimeout  PGNSP PGUID 12 f f t f s 1 2275 f "703" _null_ _null_ _null_ reltimeout _null_ _null_ n 0));
DESCR("I/O");

/* timepl(abstime, reltime) => abstime */ 
DATA(insert OID = 244 ( timepl  PGNSP PGUID 12 f f t f i 2 702 f "702 703" _null_ _null_ _null_ timepl _null_ _null_ n 0));
DESCR("add");

/* timemi(abstime, reltime) => abstime */ 
DATA(insert OID = 245 ( timemi  PGNSP PGUID 12 f f t f i 2 702 f "702 703" _null_ _null_ _null_ timemi _null_ _null_ n 0));
DESCR("subtract");

/* tintervalin(cstring) => tinterval */ 
DATA(insert OID = 246 ( tintervalin  PGNSP PGUID 12 f f t f s 1 704 f "2275" _null_ _null_ _null_ tintervalin _null_ _null_ n 0));
DESCR("I/O");

/* tintervalout(tinterval) => cstring */ 
DATA(insert OID = 247 ( tintervalout  PGNSP PGUID 12 f f t f s 1 2275 f "704" _null_ _null_ _null_ tintervalout _null_ _null_ n 0));
DESCR("I/O");

/* intinterval(abstime, tinterval) => bool */ 
DATA(insert OID = 248 ( intinterval  PGNSP PGUID 12 f f t f i 2 16 f "702 704" _null_ _null_ _null_ intinterval _null_ _null_ n 0));
DESCR("abstime in tinterval");

/* tintervalrel(tinterval) => reltime */ 
DATA(insert OID = 249 ( tintervalrel  PGNSP PGUID 12 f f t f i 1 703 f "704" _null_ _null_ _null_ tintervalrel _null_ _null_ n 0));
DESCR("tinterval to reltime");

/* timenow() => abstime */ 
DATA(insert OID = 250 ( timenow  PGNSP PGUID 12 f f t f s 0 702 f "" _null_ _null_ _null_ timenow _null_ _null_ n 0));
DESCR("Current date and time (abstime)");

/* abstimeeq(abstime, abstime) => bool */ 
DATA(insert OID = 251 ( abstimeeq  PGNSP PGUID 12 f f t f i 2 16 f "702 702" _null_ _null_ _null_ abstimeeq _null_ _null_ n 0));
DESCR("equal");

/* abstimene(abstime, abstime) => bool */ 
DATA(insert OID = 252 ( abstimene  PGNSP PGUID 12 f f t f i 2 16 f "702 702" _null_ _null_ _null_ abstimene _null_ _null_ n 0));
DESCR("not equal");

/* abstimelt(abstime, abstime) => bool */ 
DATA(insert OID = 253 ( abstimelt  PGNSP PGUID 12 f f t f i 2 16 f "702 702" _null_ _null_ _null_ abstimelt _null_ _null_ n 0));
DESCR("less-than");

/* abstimegt(abstime, abstime) => bool */ 
DATA(insert OID = 254 ( abstimegt  PGNSP PGUID 12 f f t f i 2 16 f "702 702" _null_ _null_ _null_ abstimegt _null_ _null_ n 0));
DESCR("greater-than");

/* abstimele(abstime, abstime) => bool */ 
DATA(insert OID = 255 ( abstimele  PGNSP PGUID 12 f f t f i 2 16 f "702 702" _null_ _null_ _null_ abstimele _null_ _null_ n 0));
DESCR("less-than-or-equal");

/* abstimege(abstime, abstime) => bool */ 
DATA(insert OID = 256 ( abstimege  PGNSP PGUID 12 f f t f i 2 16 f "702 702" _null_ _null_ _null_ abstimege _null_ _null_ n 0));
DESCR("greater-than-or-equal");

/* reltimeeq(reltime, reltime) => bool */ 
DATA(insert OID = 257 ( reltimeeq  PGNSP PGUID 12 f f t f i 2 16 f "703 703" _null_ _null_ _null_ reltimeeq _null_ _null_ n 0));
DESCR("equal");

/* reltimene(reltime, reltime) => bool */ 
DATA(insert OID = 258 ( reltimene  PGNSP PGUID 12 f f t f i 2 16 f "703 703" _null_ _null_ _null_ reltimene _null_ _null_ n 0));
DESCR("not equal");

/* reltimelt(reltime, reltime) => bool */ 
DATA(insert OID = 259 ( reltimelt  PGNSP PGUID 12 f f t f i 2 16 f "703 703" _null_ _null_ _null_ reltimelt _null_ _null_ n 0));
DESCR("less-than");

/* reltimegt(reltime, reltime) => bool */ 
DATA(insert OID = 260 ( reltimegt  PGNSP PGUID 12 f f t f i 2 16 f "703 703" _null_ _null_ _null_ reltimegt _null_ _null_ n 0));
DESCR("greater-than");

/* reltimele(reltime, reltime) => bool */ 
DATA(insert OID = 261 ( reltimele  PGNSP PGUID 12 f f t f i 2 16 f "703 703" _null_ _null_ _null_ reltimele _null_ _null_ n 0));
DESCR("less-than-or-equal");

/* reltimege(reltime, reltime) => bool */ 
DATA(insert OID = 262 ( reltimege  PGNSP PGUID 12 f f t f i 2 16 f "703 703" _null_ _null_ _null_ reltimege _null_ _null_ n 0));
DESCR("greater-than-or-equal");

/* tintervalsame(tinterval, tinterval) => bool */ 
DATA(insert OID = 263 ( tintervalsame  PGNSP PGUID 12 f f t f i 2 16 f "704 704" _null_ _null_ _null_ tintervalsame _null_ _null_ n 0));
DESCR("same as?");

/* tintervalct(tinterval, tinterval) => bool */ 
DATA(insert OID = 264 ( tintervalct  PGNSP PGUID 12 f f t f i 2 16 f "704 704" _null_ _null_ _null_ tintervalct _null_ _null_ n 0));
DESCR("contains?");

/* tintervalov(tinterval, tinterval) => bool */ 
DATA(insert OID = 265 ( tintervalov  PGNSP PGUID 12 f f t f i 2 16 f "704 704" _null_ _null_ _null_ tintervalov _null_ _null_ n 0));
DESCR("overlaps");

/* tintervalleneq(tinterval, reltime) => bool */ 
DATA(insert OID = 266 ( tintervalleneq  PGNSP PGUID 12 f f t f i 2 16 f "704 703" _null_ _null_ _null_ tintervalleneq _null_ _null_ n 0));
DESCR("length equal");

/* tintervallenne(tinterval, reltime) => bool */ 
DATA(insert OID = 267 ( tintervallenne  PGNSP PGUID 12 f f t f i 2 16 f "704 703" _null_ _null_ _null_ tintervallenne _null_ _null_ n 0));
DESCR("length not equal to");

/* tintervallenlt(tinterval, reltime) => bool */ 
DATA(insert OID = 268 ( tintervallenlt  PGNSP PGUID 12 f f t f i 2 16 f "704 703" _null_ _null_ _null_ tintervallenlt _null_ _null_ n 0));
DESCR("length less-than");

/* tintervallengt(tinterval, reltime) => bool */ 
DATA(insert OID = 269 ( tintervallengt  PGNSP PGUID 12 f f t f i 2 16 f "704 703" _null_ _null_ _null_ tintervallengt _null_ _null_ n 0));
DESCR("length greater-than");

/* tintervallenle(tinterval, reltime) => bool */ 
DATA(insert OID = 270 ( tintervallenle  PGNSP PGUID 12 f f t f i 2 16 f "704 703" _null_ _null_ _null_ tintervallenle _null_ _null_ n 0));
DESCR("length less-than-or-equal");

/* tintervallenge(tinterval, reltime) => bool */ 
DATA(insert OID = 271 ( tintervallenge  PGNSP PGUID 12 f f t f i 2 16 f "704 703" _null_ _null_ _null_ tintervallenge _null_ _null_ n 0));
DESCR("length greater-than-or-equal");

/* tintervalstart(tinterval) => abstime */ 
DATA(insert OID = 272 ( tintervalstart  PGNSP PGUID 12 f f t f i 1 702 f "704" _null_ _null_ _null_ tintervalstart _null_ _null_ n 0));
DESCR("start of interval");

/* tintervalend(tinterval) => abstime */ 
DATA(insert OID = 273 ( tintervalend  PGNSP PGUID 12 f f t f i 1 702 f "704" _null_ _null_ _null_ tintervalend _null_ _null_ n 0));
DESCR("end of interval");

/* timeofday() => text */ 
DATA(insert OID = 274 ( timeofday  PGNSP PGUID 12 f f t f v 0 25 f "" _null_ _null_ _null_ timeofday _null_ _null_ n 0));
DESCR("Current date and time - increments during transactions");

/* isfinite(abstime) => bool */ 
DATA(insert OID = 275 ( isfinite  PGNSP PGUID 12 f f t f i 1 16 f "702" _null_ _null_ _null_ abstime_finite _null_ _null_ n 0));
DESCR("finite abstime?");

/* inter_sl(lseg, line) => bool */ 
DATA(insert OID = 277 ( inter_sl  PGNSP PGUID 12 f f t f i 2 16 f "601 628" _null_ _null_ _null_ inter_sl _null_ _null_ n 0));
DESCR("intersect?");

/* inter_lb(line, box) => bool */ 
DATA(insert OID = 278 ( inter_lb  PGNSP PGUID 12 f f t f i 2 16 f "628 603" _null_ _null_ _null_ inter_lb _null_ _null_ n 0));
DESCR("intersect?");

/* float48mul(float4, float8) => float8 */ 
DATA(insert OID = 279 ( float48mul  PGNSP PGUID 12 f f t f i 2 701 f "700 701" _null_ _null_ _null_ float48mul _null_ _null_ n 0));
DESCR("multiply");

/* float48div(float4, float8) => float8 */ 
DATA(insert OID = 280 ( float48div  PGNSP PGUID 12 f f t f i 2 701 f "700 701" _null_ _null_ _null_ float48div _null_ _null_ n 0));
DESCR("divide");

/* float48pl(float4, float8) => float8 */ 
DATA(insert OID = 281 ( float48pl  PGNSP PGUID 12 f f t f i 2 701 f "700 701" _null_ _null_ _null_ float48pl _null_ _null_ n 0));
DESCR("add");

/* float48mi(float4, float8) => float8 */ 
DATA(insert OID = 282 ( float48mi  PGNSP PGUID 12 f f t f i 2 701 f "700 701" _null_ _null_ _null_ float48mi _null_ _null_ n 0));
DESCR("subtract");

/* float84mul(float8, float4) => float8 */ 
DATA(insert OID = 283 ( float84mul  PGNSP PGUID 12 f f t f i 2 701 f "701 700" _null_ _null_ _null_ float84mul _null_ _null_ n 0));
DESCR("multiply");

/* float84div(float8, float4) => float8 */ 
DATA(insert OID = 284 ( float84div  PGNSP PGUID 12 f f t f i 2 701 f "701 700" _null_ _null_ _null_ float84div _null_ _null_ n 0));
DESCR("divide");

/* float84pl(float8, float4) => float8 */ 
DATA(insert OID = 285 ( float84pl  PGNSP PGUID 12 f f t f i 2 701 f "701 700" _null_ _null_ _null_ float84pl _null_ _null_ n 0));
DESCR("add");

/* float84mi(float8, float4) => float8 */ 
DATA(insert OID = 286 ( float84mi  PGNSP PGUID 12 f f t f i 2 701 f "701 700" _null_ _null_ _null_ float84mi _null_ _null_ n 0));
DESCR("subtract");

/* float4eq(float4, float4) => bool */ 
DATA(insert OID = 287 ( float4eq  PGNSP PGUID 12 f f t f i 2 16 f "700 700" _null_ _null_ _null_ float4eq _null_ _null_ n 0));
DESCR("equal");

/* float4ne(float4, float4) => bool */ 
DATA(insert OID = 288 ( float4ne  PGNSP PGUID 12 f f t f i 2 16 f "700 700" _null_ _null_ _null_ float4ne _null_ _null_ n 0));
DESCR("not equal");

/* float4lt(float4, float4) => bool */ 
DATA(insert OID = 289 ( float4lt  PGNSP PGUID 12 f f t f i 2 16 f "700 700" _null_ _null_ _null_ float4lt _null_ _null_ n 0));
DESCR("less-than");

/* float4le(float4, float4) => bool */ 
DATA(insert OID = 290 ( float4le  PGNSP PGUID 12 f f t f i 2 16 f "700 700" _null_ _null_ _null_ float4le _null_ _null_ n 0));
DESCR("less-than-or-equal");

/* float4gt(float4, float4) => bool */ 
DATA(insert OID = 291 ( float4gt  PGNSP PGUID 12 f f t f i 2 16 f "700 700" _null_ _null_ _null_ float4gt _null_ _null_ n 0));
DESCR("greater-than");

/* float4ge(float4, float4) => bool */ 
DATA(insert OID = 292 ( float4ge  PGNSP PGUID 12 f f t f i 2 16 f "700 700" _null_ _null_ _null_ float4ge _null_ _null_ n 0));
DESCR("greater-than-or-equal");

/* float8eq(float8, float8) => bool */ 
DATA(insert OID = 293 ( float8eq  PGNSP PGUID 12 f f t f i 2 16 f "701 701" _null_ _null_ _null_ float8eq _null_ _null_ n 0));
DESCR("equal");

/* float8ne(float8, float8) => bool */ 
DATA(insert OID = 294 ( float8ne  PGNSP PGUID 12 f f t f i 2 16 f "701 701" _null_ _null_ _null_ float8ne _null_ _null_ n 0));
DESCR("not equal");

/* float8lt(float8, float8) => bool */ 
DATA(insert OID = 295 ( float8lt  PGNSP PGUID 12 f f t f i 2 16 f "701 701" _null_ _null_ _null_ float8lt _null_ _null_ n 0));
DESCR("less-than");

/* float8le(float8, float8) => bool */ 
DATA(insert OID = 296 ( float8le  PGNSP PGUID 12 f f t f i 2 16 f "701 701" _null_ _null_ _null_ float8le _null_ _null_ n 0));
DESCR("less-than-or-equal");

/* float8gt(float8, float8) => bool */ 
DATA(insert OID = 297 ( float8gt  PGNSP PGUID 12 f f t f i 2 16 f "701 701" _null_ _null_ _null_ float8gt _null_ _null_ n 0));
DESCR("greater-than");

/* float8ge(float8, float8) => bool */ 
DATA(insert OID = 298 ( float8ge  PGNSP PGUID 12 f f t f i 2 16 f "701 701" _null_ _null_ _null_ float8ge _null_ _null_ n 0));
DESCR("greater-than-or-equal");

/* float48eq(float4, float8) => bool */ 
DATA(insert OID = 299 ( float48eq  PGNSP PGUID 12 f f t f i 2 16 f "700 701" _null_ _null_ _null_ float48eq _null_ _null_ n 0));
DESCR("equal");


/* OIDS 300 - 399 */
/* float48ne(float4, float8) => bool */ 
DATA(insert OID = 300 ( float48ne  PGNSP PGUID 12 f f t f i 2 16 f "700 701" _null_ _null_ _null_ float48ne _null_ _null_ n 0));
DESCR("not equal");

/* float48lt(float4, float8) => bool */ 
DATA(insert OID = 301 ( float48lt  PGNSP PGUID 12 f f t f i 2 16 f "700 701" _null_ _null_ _null_ float48lt _null_ _null_ n 0));
DESCR("less-than");

/* float48le(float4, float8) => bool */ 
DATA(insert OID = 302 ( float48le  PGNSP PGUID 12 f f t f i 2 16 f "700 701" _null_ _null_ _null_ float48le _null_ _null_ n 0));
DESCR("less-than-or-equal");

/* float48gt(float4, float8) => bool */ 
DATA(insert OID = 303 ( float48gt  PGNSP PGUID 12 f f t f i 2 16 f "700 701" _null_ _null_ _null_ float48gt _null_ _null_ n 0));
DESCR("greater-than");

/* float48ge(float4, float8) => bool */ 
DATA(insert OID = 304 ( float48ge  PGNSP PGUID 12 f f t f i 2 16 f "700 701" _null_ _null_ _null_ float48ge _null_ _null_ n 0));
DESCR("greater-than-or-equal");

/* float84eq(float8, float4) => bool */ 
DATA(insert OID = 305 ( float84eq  PGNSP PGUID 12 f f t f i 2 16 f "701 700" _null_ _null_ _null_ float84eq _null_ _null_ n 0));
DESCR("equal");

/* float84ne(float8, float4) => bool */ 
DATA(insert OID = 306 ( float84ne  PGNSP PGUID 12 f f t f i 2 16 f "701 700" _null_ _null_ _null_ float84ne _null_ _null_ n 0));
DESCR("not equal");

/* float84lt(float8, float4) => bool */ 
DATA(insert OID = 307 ( float84lt  PGNSP PGUID 12 f f t f i 2 16 f "701 700" _null_ _null_ _null_ float84lt _null_ _null_ n 0));
DESCR("less-than");

/* float84le(float8, float4) => bool */ 
DATA(insert OID = 308 ( float84le  PGNSP PGUID 12 f f t f i 2 16 f "701 700" _null_ _null_ _null_ float84le _null_ _null_ n 0));
DESCR("less-than-or-equal");

/* float84gt(float8, float4) => bool */ 
DATA(insert OID = 309 ( float84gt  PGNSP PGUID 12 f f t f i 2 16 f "701 700" _null_ _null_ _null_ float84gt _null_ _null_ n 0));
DESCR("greater-than");

/* float84ge(float8, float4) => bool */ 
DATA(insert OID = 310 ( float84ge  PGNSP PGUID 12 f f t f i 2 16 f "701 700" _null_ _null_ _null_ float84ge _null_ _null_ n 0));
DESCR("greater-than-or-equal");

/* float8(float4) => float8 */ 
DATA(insert OID = 311 ( float8  PGNSP PGUID 12 f f t f i 1 701 f "700" _null_ _null_ _null_ ftod _null_ _null_ n 0));
DESCR("convert float4 to float8");

/* float4(float8) => float4 */ 
DATA(insert OID = 312 ( float4  PGNSP PGUID 12 f f t f i 1 700 f "701" _null_ _null_ _null_ dtof _null_ _null_ n 0));
DESCR("convert float8 to float4");

/* int4(int2) => int4 */ 
DATA(insert OID = 313 ( int4  PGNSP PGUID 12 f f t f i 1 23 f "21" _null_ _null_ _null_ i2toi4 _null_ _null_ n 0));
DESCR("convert int2 to int4");

/* int2(int4) => int2 */ 
DATA(insert OID = 314 ( int2  PGNSP PGUID 12 f f t f i 1 21 f "23" _null_ _null_ _null_ i4toi2 _null_ _null_ n 0));
DESCR("convert int4 to int2");

/* int2vectoreq(int2vector, int2vector) => bool */ 
DATA(insert OID = 315 ( int2vectoreq  PGNSP PGUID 12 f f t f i 2 16 f "22 22" _null_ _null_ _null_ int2vectoreq _null_ _null_ n 0));
DESCR("equal");

/* float8(int4) => float8 */ 
DATA(insert OID = 316 ( float8  PGNSP PGUID 12 f f t f i 1 701 f "23" _null_ _null_ _null_ i4tod _null_ _null_ n 0));
DESCR("convert int4 to float8");

/* int4(float8) => int4 */ 
DATA(insert OID = 317 ( int4  PGNSP PGUID 12 f f t f i 1 23 f "701" _null_ _null_ _null_ dtoi4 _null_ _null_ n 0));
DESCR("convert float8 to int4");

/* float4(int4) => float4 */ 
DATA(insert OID = 318 ( float4  PGNSP PGUID 12 f f t f i 1 700 f "23" _null_ _null_ _null_ i4tof _null_ _null_ n 0));
DESCR("convert int4 to float4");

/* int4(float4) => int4 */ 
DATA(insert OID = 319 ( int4  PGNSP PGUID 12 f f t f i 1 23 f "700" _null_ _null_ _null_ ftoi4 _null_ _null_ n 0));
DESCR("convert float4 to int4");

/* btgettuple(internal, internal) => bool */ 
DATA(insert OID = 330 ( btgettuple  PGNSP PGUID 12 f f t f v 2 16 f "2281 2281" _null_ _null_ _null_ btgettuple _null_ _null_ n 0));
DESCR("btree(internal)");


#define BTGETTUPLE_OID 330
/* btgetmulti(internal, internal) => internal */ 
DATA(insert OID = 636 ( btgetmulti  PGNSP PGUID 12 f f t f v 2 2281 f "2281 2281" _null_ _null_ _null_ btgetmulti _null_ _null_ n 0));
DESCR("btree(internal)");


#define BTGETMULTI_OID 636
/* btinsert(internal, internal, internal, internal, internal, internal) => bool */ 
DATA(insert OID = 331 ( btinsert  PGNSP PGUID 12 f f t f v 6 16 f "2281 2281 2281 2281 2281 2281" _null_ _null_ _null_ btinsert _null_ _null_ n 0));
DESCR("btree(internal)");


#define BTINSERT_OID 331
/* btbeginscan(internal, internal, internal) => internal */ 
DATA(insert OID = 333 ( btbeginscan  PGNSP PGUID 12 f f t f v 3 2281 f "2281 2281 2281" _null_ _null_ _null_ btbeginscan _null_ _null_ n 0));
DESCR("btree(internal)");


#define BTBEGINSCAN_OID 333
/* btrescan(internal, internal) => void */ 
DATA(insert OID = 334 ( btrescan  PGNSP PGUID 12 f f t f v 2 2278 f "2281 2281" _null_ _null_ _null_ btrescan _null_ _null_ n 0));
DESCR("btree(internal)");


#define BTRESCAN_OID 334
/* btendscan(internal) => void */ 
DATA(insert OID = 335 ( btendscan  PGNSP PGUID 12 f f t f v 1 2278 f "2281" _null_ _null_ _null_ btendscan _null_ _null_ n 0));
DESCR("btree(internal)");


#define BTENDSCAN_OID 335
/* btmarkpos(internal) => void */ 
DATA(insert OID = 336 ( btmarkpos  PGNSP PGUID 12 f f t f v 1 2278 f "2281" _null_ _null_ _null_ btmarkpos _null_ _null_ n 0));
DESCR("btree(internal)");


#define BTMARKPOS_OID 336
/* btrestrpos(internal) => void */ 
DATA(insert OID = 337 ( btrestrpos  PGNSP PGUID 12 f f t f v 1 2278 f "2281" _null_ _null_ _null_ btrestrpos _null_ _null_ n 0));
DESCR("btree(internal)");


#define BTRESTRPOS_OID 337
/* btbuild(internal, internal, internal) => internal */ 
DATA(insert OID = 338 ( btbuild  PGNSP PGUID 12 f f t f v 3 2281 f "2281 2281 2281" _null_ _null_ _null_ btbuild _null_ _null_ n 0));
DESCR("btree(internal)");


#define BTBUILD_OID 338
/* btbulkdelete(internal, internal, internal, internal) => internal */ 
DATA(insert OID = 332 ( btbulkdelete  PGNSP PGUID 12 f f t f v 4 2281 f "2281 2281 2281 2281" _null_ _null_ _null_ btbulkdelete _null_ _null_ n 0));
DESCR("btree(internal)");


#define BTBULKDELETE_OID 332
/* btvacuumcleanup(internal, internal) => internal */ 
DATA(insert OID = 972 ( btvacuumcleanup  PGNSP PGUID 12 f f t f v 2 2281 f "2281 2281" _null_ _null_ _null_ btvacuumcleanup _null_ _null_ n 0));
DESCR("btree(internal)");


#define BTVACUUMCLEANUP_OID 972
/* btcostestimate(internal, internal, internal, internal, internal, internal, internal, internal) => void */ 
DATA(insert OID = 1268 ( btcostestimate  PGNSP PGUID 12 f f t f v 8 2278 f "2281 2281 2281 2281 2281 2281 2281 2281" _null_ _null_ _null_ btcostestimate _null_ _null_ n 0));
DESCR("btree(internal)");


#define BTCOSTESTIMATE_OID 1268
/* btoptions(_text, bool) => bytea */ 
DATA(insert OID = 2785 ( btoptions  PGNSP PGUID 12 f f t f s 2 17 f "1009 16" _null_ _null_ _null_ btoptions _null_ _null_ n 0));
DESCR("btree(internal)");


#define BTOPTIONS_OID 2785
/* poly_same(polygon, polygon) => bool */ 
DATA(insert OID = 339 ( poly_same  PGNSP PGUID 12 f f t f i 2 16 f "604 604" _null_ _null_ _null_ poly_same _null_ _null_ n 0));
DESCR("same as?");

/* poly_contain(polygon, polygon) => bool */ 
DATA(insert OID = 340 ( poly_contain  PGNSP PGUID 12 f f t f i 2 16 f "604 604" _null_ _null_ _null_ poly_contain _null_ _null_ n 0));
DESCR("contains?");

/* poly_left(polygon, polygon) => bool */ 
DATA(insert OID = 341 ( poly_left  PGNSP PGUID 12 f f t f i 2 16 f "604 604" _null_ _null_ _null_ poly_left _null_ _null_ n 0));
DESCR("is left of");

/* poly_overleft(polygon, polygon) => bool */ 
DATA(insert OID = 342 ( poly_overleft  PGNSP PGUID 12 f f t f i 2 16 f "604 604" _null_ _null_ _null_ poly_overleft _null_ _null_ n 0));
DESCR("overlaps or is left of");

/* poly_overright(polygon, polygon) => bool */ 
DATA(insert OID = 343 ( poly_overright  PGNSP PGUID 12 f f t f i 2 16 f "604 604" _null_ _null_ _null_ poly_overright _null_ _null_ n 0));
DESCR("overlaps or is right of");

/* poly_right(polygon, polygon) => bool */ 
DATA(insert OID = 344 ( poly_right  PGNSP PGUID 12 f f t f i 2 16 f "604 604" _null_ _null_ _null_ poly_right _null_ _null_ n 0));
DESCR("is right of");

/* poly_contained(polygon, polygon) => bool */ 
DATA(insert OID = 345 ( poly_contained  PGNSP PGUID 12 f f t f i 2 16 f "604 604" _null_ _null_ _null_ poly_contained _null_ _null_ n 0));
DESCR("is contained by?");

/* poly_overlap(polygon, polygon) => bool */ 
DATA(insert OID = 346 ( poly_overlap  PGNSP PGUID 12 f f t f i 2 16 f "604 604" _null_ _null_ _null_ poly_overlap _null_ _null_ n 0));
DESCR("overlaps");

/* poly_in(cstring) => polygon */ 
DATA(insert OID = 347 ( poly_in  PGNSP PGUID 12 f f t f i 1 604 f "2275" _null_ _null_ _null_ poly_in _null_ _null_ n 0));
DESCR("I/O");

/* poly_out(polygon) => cstring */ 
DATA(insert OID = 348 ( poly_out  PGNSP PGUID 12 f f t f i 1 2275 f "604" _null_ _null_ _null_ poly_out _null_ _null_ n 0));
DESCR("I/O");

/* btint2cmp(int2, int2) => int4 */ 
DATA(insert OID = 350 ( btint2cmp  PGNSP PGUID 12 f f t f i 2 23 f "21 21" _null_ _null_ _null_ btint2cmp _null_ _null_ n 0));
DESCR("btree less-equal-greater");

/* btint4cmp(int4, int4) => int4 */ 
DATA(insert OID = 351 ( btint4cmp  PGNSP PGUID 12 f f t f i 2 23 f "23 23" _null_ _null_ _null_ btint4cmp _null_ _null_ n 0));
DESCR("btree less-equal-greater");

/* btint8cmp(int8, int8) => int4 */ 
DATA(insert OID = 842 ( btint8cmp  PGNSP PGUID 12 f f t f i 2 23 f "20 20" _null_ _null_ _null_ btint8cmp _null_ _null_ n 0));
DESCR("btree less-equal-greater");

/* btfloat4cmp(float4, float4) => int4 */ 
DATA(insert OID = 354 ( btfloat4cmp  PGNSP PGUID 12 f f t f i 2 23 f "700 700" _null_ _null_ _null_ btfloat4cmp _null_ _null_ n 0));
DESCR("btree less-equal-greater");

/* btfloat8cmp(float8, float8) => int4 */ 
DATA(insert OID = 355 ( btfloat8cmp  PGNSP PGUID 12 f f t f i 2 23 f "701 701" _null_ _null_ _null_ btfloat8cmp _null_ _null_ n 0));
DESCR("btree less-equal-greater");

/* btoidcmp(oid, oid) => int4 */ 
DATA(insert OID = 356 ( btoidcmp  PGNSP PGUID 12 f f t f i 2 23 f "26 26" _null_ _null_ _null_ btoidcmp _null_ _null_ n 0));
DESCR("btree less-equal-greater");

/* btoidvectorcmp(oidvector, oidvector) => int4 */ 
DATA(insert OID = 404 ( btoidvectorcmp  PGNSP PGUID 12 f f t f i 2 23 f "30 30" _null_ _null_ _null_ btoidvectorcmp _null_ _null_ n 0));
DESCR("btree less-equal-greater");

/* btabstimecmp(abstime, abstime) => int4 */ 
DATA(insert OID = 357 ( btabstimecmp  PGNSP PGUID 12 f f t f i 2 23 f "702 702" _null_ _null_ _null_ btabstimecmp _null_ _null_ n 0));
DESCR("btree less-equal-greater");

/* btcharcmp("char", "char") => int4 */ 
DATA(insert OID = 358 ( btcharcmp  PGNSP PGUID 12 f f t f i 2 23 f "18 18" _null_ _null_ _null_ btcharcmp _null_ _null_ n 0));
DESCR("btree less-equal-greater");

/* btnamecmp(name, name) => int4 */ 
DATA(insert OID = 359 ( btnamecmp  PGNSP PGUID 12 f f t f i 2 23 f "19 19" _null_ _null_ _null_ btnamecmp _null_ _null_ n 0));
DESCR("btree less-equal-greater");

/* bttextcmp(text, text) => int4 */ 
DATA(insert OID = 360 ( bttextcmp  PGNSP PGUID 12 f f t f i 2 23 f "25 25" _null_ _null_ _null_ bttextcmp _null_ _null_ n 0));
DESCR("btree less-equal-greater");

/* cash_cmp(money, money) => int4 */ 
DATA(insert OID = 377 ( cash_cmp  PGNSP PGUID 12 f f t f i 2 23 f "790 790" _null_ _null_ _null_ cash_cmp _null_ _null_ n 0));
DESCR("btree less-equal-greater");

/* btreltimecmp(reltime, reltime) => int4 */ 
DATA(insert OID = 380 ( btreltimecmp  PGNSP PGUID 12 f f t f i 2 23 f "703 703" _null_ _null_ _null_ btreltimecmp _null_ _null_ n 0));
DESCR("btree less-equal-greater");

/* bttintervalcmp(tinterval, tinterval) => int4 */ 
DATA(insert OID = 381 ( bttintervalcmp  PGNSP PGUID 12 f f t f i 2 23 f "704 704" _null_ _null_ _null_ bttintervalcmp _null_ _null_ n 0));
DESCR("btree less-equal-greater");

/* btarraycmp(anyarray, anyarray) => int4 */ 
DATA(insert OID = 382 ( btarraycmp  PGNSP PGUID 12 f f t f i 2 23 f "2277 2277" _null_ _null_ _null_ btarraycmp _null_ _null_ n 0));
DESCR("btree less-equal-greater");

/* btgpxlogloccmp(gpxlogloc, gpxlogloc) => int4 */ 
DATA(insert OID = 2905 ( btgpxlogloccmp  PGNSP PGUID 12 f f t f i 2 23 f "3310 3310" _null_ _null_ _null_ btgpxlogloccmp _null_ _null_ n 0));
DESCR("btree less-equal-greater");

/* lseg_distance(lseg, lseg) => float8 */ 
DATA(insert OID = 361 ( lseg_distance  PGNSP PGUID 12 f f t f i 2 701 f "601 601" _null_ _null_ _null_ lseg_distance _null_ _null_ n 0));
DESCR("distance between");

/* lseg_interpt(lseg, lseg) => point */ 
DATA(insert OID = 362 ( lseg_interpt  PGNSP PGUID 12 f f t f i 2 600 f "601 601" _null_ _null_ _null_ lseg_interpt _null_ _null_ n 0));
DESCR("intersection point");

/* dist_ps(point, lseg) => float8 */ 
DATA(insert OID = 363 ( dist_ps  PGNSP PGUID 12 f f t f i 2 701 f "600 601" _null_ _null_ _null_ dist_ps _null_ _null_ n 0));
DESCR("distance between");

/* dist_pb(point, box) => float8 */ 
DATA(insert OID = 364 ( dist_pb  PGNSP PGUID 12 f f t f i 2 701 f "600 603" _null_ _null_ _null_ dist_pb _null_ _null_ n 0));
DESCR("distance between point and box");

/* dist_sb(lseg, box) => float8 */ 
DATA(insert OID = 365 ( dist_sb  PGNSP PGUID 12 f f t f i 2 701 f "601 603" _null_ _null_ _null_ dist_sb _null_ _null_ n 0));
DESCR("distance between segment and box");

/* close_ps(point, lseg) => point */ 
DATA(insert OID = 366 ( close_ps  PGNSP PGUID 12 f f t f i 2 600 f "600 601" _null_ _null_ _null_ close_ps _null_ _null_ n 0));
DESCR("closest point on line segment");

/* close_pb(point, box) => point */ 
DATA(insert OID = 367 ( close_pb  PGNSP PGUID 12 f f t f i 2 600 f "600 603" _null_ _null_ _null_ close_pb _null_ _null_ n 0));
DESCR("closest point on box");

/* close_sb(lseg, box) => point */ 
DATA(insert OID = 368 ( close_sb  PGNSP PGUID 12 f f t f i 2 600 f "601 603" _null_ _null_ _null_ close_sb _null_ _null_ n 0));
DESCR("closest point to line segment on box");

/* on_ps(point, lseg) => bool */ 
DATA(insert OID = 369 ( on_ps  PGNSP PGUID 12 f f t f i 2 16 f "600 601" _null_ _null_ _null_ on_ps _null_ _null_ n 0));
DESCR("point contained in segment?");

/* path_distance(path, path) => float8 */ 
DATA(insert OID = 370 ( path_distance  PGNSP PGUID 12 f f t f i 2 701 f "602 602" _null_ _null_ _null_ path_distance _null_ _null_ n 0));
DESCR("distance between paths");

/* dist_ppath(point, path) => float8 */ 
DATA(insert OID = 371 ( dist_ppath  PGNSP PGUID 12 f f t f i 2 701 f "600 602" _null_ _null_ _null_ dist_ppath _null_ _null_ n 0));
DESCR("distance between point and path");

/* on_sb(lseg, box) => bool */ 
DATA(insert OID = 372 ( on_sb  PGNSP PGUID 12 f f t f i 2 16 f "601 603" _null_ _null_ _null_ on_sb _null_ _null_ n 0));
DESCR("lseg contained in box?");

/* inter_sb(lseg, box) => bool */ 
DATA(insert OID = 373 ( inter_sb  PGNSP PGUID 12 f f t f i 2 16 f "601 603" _null_ _null_ _null_ inter_sb _null_ _null_ n 0));
DESCR("intersect?");


/* OIDS 400 - 499 */
/* text(bpchar) => text */ 
DATA(insert OID = 401 ( text  PGNSP PGUID 12 f f t f i 1 25 f "1042" _null_ _null_ _null_ rtrim1 _null_ _null_ n 0));
DESCR("convert char(n) to text");

/* text(name) => text */ 
DATA(insert OID = 406 ( text  PGNSP PGUID 12 f f t f i 1 25 f "19" _null_ _null_ _null_ name_text _null_ _null_ n 0));
DESCR("convert name to text");

/* name(text) => name */ 
DATA(insert OID = 407 ( name  PGNSP PGUID 12 f f t f i 1 19 f "25" _null_ _null_ _null_ text_name _null_ _null_ n 0));
DESCR("convert text to name");

/* bpchar(name) => bpchar */ 
DATA(insert OID = 408 ( bpchar  PGNSP PGUID 12 f f t f i 1 1042 f "19" _null_ _null_ _null_ name_bpchar _null_ _null_ n 0));
DESCR("convert name to char(n)");

/* name(bpchar) => name */ 
DATA(insert OID = 409 ( name  PGNSP PGUID 12 f f t f i 1 19 f "1042" _null_ _null_ _null_ bpchar_name _null_ _null_ n 0));
DESCR("convert char(n) to name");

/* hashgettuple(internal, internal) => bool */ 
DATA(insert OID = 440 ( hashgettuple  PGNSP PGUID 12 f f t f v 2 16 f "2281 2281" _null_ _null_ _null_ hashgettuple _null_ _null_ n 0));
DESCR("hash(internal)");

/* hashgetmulti(internal, internal) => internal */ 
DATA(insert OID = 637 ( hashgetmulti  PGNSP PGUID 12 f f t f v 2 2281 f "2281 2281" _null_ _null_ _null_ hashgetmulti _null_ _null_ n 0));
DESCR("hash(internal)");

/* hashinsert(internal, internal, internal, internal, internal, internal) => bool */ 
DATA(insert OID = 441 ( hashinsert  PGNSP PGUID 12 f f t f v 6 16 f "2281 2281 2281 2281 2281 2281" _null_ _null_ _null_ hashinsert _null_ _null_ n 0));
DESCR("hash(internal)");

/* hashbeginscan(internal, internal, internal) => internal */ 
DATA(insert OID = 443 ( hashbeginscan  PGNSP PGUID 12 f f t f v 3 2281 f "2281 2281 2281" _null_ _null_ _null_ hashbeginscan _null_ _null_ n 0));
DESCR("hash(internal)");

/* hashrescan(internal, internal) => void */ 
DATA(insert OID = 444 ( hashrescan  PGNSP PGUID 12 f f t f v 2 2278 f "2281 2281" _null_ _null_ _null_ hashrescan _null_ _null_ n 0));
DESCR("hash(internal)");

/* hashendscan(internal) => void */ 
DATA(insert OID = 445 ( hashendscan  PGNSP PGUID 12 f f t f v 1 2278 f "2281" _null_ _null_ _null_ hashendscan _null_ _null_ n 0));
DESCR("hash(internal)");

/* hashmarkpos(internal) => void */ 
DATA(insert OID = 446 ( hashmarkpos  PGNSP PGUID 12 f f t f v 1 2278 f "2281" _null_ _null_ _null_ hashmarkpos _null_ _null_ n 0));
DESCR("hash(internal)");

/* hashrestrpos(internal) => void */ 
DATA(insert OID = 447 ( hashrestrpos  PGNSP PGUID 12 f f t f v 1 2278 f "2281" _null_ _null_ _null_ hashrestrpos _null_ _null_ n 0));
DESCR("hash(internal)");

/* hashbuild(internal, internal, internal) => internal */ 
DATA(insert OID = 448 ( hashbuild  PGNSP PGUID 12 f f t f v 3 2281 f "2281 2281 2281" _null_ _null_ _null_ hashbuild _null_ _null_ n 0));
DESCR("hash(internal)");

/* hashbulkdelete(internal, internal, internal, internal) => internal */ 
DATA(insert OID = 442 ( hashbulkdelete  PGNSP PGUID 12 f f t f v 4 2281 f "2281 2281 2281 2281" _null_ _null_ _null_ hashbulkdelete _null_ _null_ n 0));
DESCR("hash(internal)");

/* hashvacuumcleanup(internal, internal) => internal */ 
DATA(insert OID = 425 ( hashvacuumcleanup  PGNSP PGUID 12 f f t f v 2 2281 f "2281 2281" _null_ _null_ _null_ hashvacuumcleanup _null_ _null_ n 0));
DESCR("hash(internal)");

/* hashcostestimate(internal, internal, internal, internal, internal, internal, internal, internal) => void */ 
DATA(insert OID = 438 ( hashcostestimate  PGNSP PGUID 12 f f t f v 8 2278 f "2281 2281 2281 2281 2281 2281 2281 2281" _null_ _null_ _null_ hashcostestimate _null_ _null_ n 0));
DESCR("hash(internal)");

/* hashoptions(_text, bool) => bytea */ 
DATA(insert OID = 2786 ( hashoptions  PGNSP PGUID 12 f f t f s 2 17 f "1009 16" _null_ _null_ _null_ hashoptions _null_ _null_ n 0));
DESCR("hash(internal)");

/* hashint2(int2) => int4 */ 
DATA(insert OID = 449 ( hashint2  PGNSP PGUID 12 f f t f i 1 23 f "21" _null_ _null_ _null_ hashint2 _null_ _null_ n 0));
DESCR("hash");

/* hashint4(int4) => int4 */ 
DATA(insert OID = 450 ( hashint4  PGNSP PGUID 12 f f t f i 1 23 f "23" _null_ _null_ _null_ hashint4 _null_ _null_ n 0));
DESCR("hash");

/* hashint8(int8) => int4 */ 
DATA(insert OID = 949 ( hashint8  PGNSP PGUID 12 f f t f i 1 23 f "20" _null_ _null_ _null_ hashint8 _null_ _null_ n 0));
DESCR("hash");

/* hashfloat4(float4) => int4 */ 
DATA(insert OID = 451 ( hashfloat4  PGNSP PGUID 12 f f t f i 1 23 f "700" _null_ _null_ _null_ hashfloat4 _null_ _null_ n 0));
DESCR("hash");

/* hashfloat8(float8) => int4 */ 
DATA(insert OID = 452 ( hashfloat8  PGNSP PGUID 12 f f t f i 1 23 f "701" _null_ _null_ _null_ hashfloat8 _null_ _null_ n 0));
DESCR("hash");

/* hashoid(oid) => int4 */ 
DATA(insert OID = 453 ( hashoid  PGNSP PGUID 12 f f t f i 1 23 f "26" _null_ _null_ _null_ hashoid _null_ _null_ n 0));
DESCR("hash");

/* hashchar("char") => int4 */ 
DATA(insert OID = 454 ( hashchar  PGNSP PGUID 12 f f t f i 1 23 f "18" _null_ _null_ _null_ hashchar _null_ _null_ n 0));
DESCR("hash");

/* hashname(name) => int4 */ 
DATA(insert OID = 455 ( hashname  PGNSP PGUID 12 f f t f i 1 23 f "19" _null_ _null_ _null_ hashname _null_ _null_ n 0));
DESCR("hash");

/* hashtext(text) => int4 */ 
DATA(insert OID = 400 ( hashtext  PGNSP PGUID 12 f f t f i 1 23 f "25" _null_ _null_ _null_ hashtext _null_ _null_ n 0));
DESCR("hash");

/* hashvarlena(internal) => int4 */ 
DATA(insert OID = 456 ( hashvarlena  PGNSP PGUID 12 f f t f i 1 23 f "2281" _null_ _null_ _null_ hashvarlena _null_ _null_ n 0));
DESCR("hash any varlena type");

/* hashoidvector(oidvector) => int4 */ 
DATA(insert OID = 457 ( hashoidvector  PGNSP PGUID 12 f f t f i 1 23 f "30" _null_ _null_ _null_ hashoidvector _null_ _null_ n 0));
DESCR("hash");

/* hash_aclitem(aclitem) => int4 */ 
DATA(insert OID = 329 ( hash_aclitem  PGNSP PGUID 12 f f t f i 1 23 f "1033" _null_ _null_ _null_ hash_aclitem _null_ _null_ n 0));
DESCR("hash");

/* hashint2vector(int2vector) => int4 */ 
DATA(insert OID = 398 ( hashint2vector  PGNSP PGUID 12 f f t f i 1 23 f "22" _null_ _null_ _null_ hashint2vector _null_ _null_ n 0));
DESCR("hash");

/* hashmacaddr(macaddr) => int4 */ 
DATA(insert OID = 399 ( hashmacaddr  PGNSP PGUID 12 f f t f i 1 23 f "829" _null_ _null_ _null_ hashmacaddr _null_ _null_ n 0));
DESCR("hash");

/* hashinet(inet) => int4 */ 
DATA(insert OID = 422 ( hashinet  PGNSP PGUID 12 f f t f i 1 23 f "869" _null_ _null_ _null_ hashinet _null_ _null_ n 0));
DESCR("hash");

/* hash_numeric("numeric") => int4 */ 
DATA(insert OID = 6432 ( hash_numeric  PGNSP PGUID 12 f f t f i 1 23 f "1700" _null_ _null_ _null_ hash_numeric _null_ _null_ n 0));
DESCR("hash");

/* text_larger(text, text) => text */ 
DATA(insert OID = 458 ( text_larger  PGNSP PGUID 12 f f t f i 2 25 f "25 25" _null_ _null_ _null_ text_larger _null_ _null_ n 0));
DESCR("larger of two");

/* text_smaller(text, text) => text */ 
DATA(insert OID = 459 ( text_smaller  PGNSP PGUID 12 f f t f i 2 25 f "25 25" _null_ _null_ _null_ text_smaller _null_ _null_ n 0));
DESCR("smaller of two");

/* int8in(cstring) => int8 */ 
DATA(insert OID = 460 ( int8in  PGNSP PGUID 12 f f t f i 1 20 f "2275" _null_ _null_ _null_ int8in _null_ _null_ n 0));
DESCR("I/O");

/* int8out(int8) => cstring */ 
DATA(insert OID = 461 ( int8out  PGNSP PGUID 12 f f t f i 1 2275 f "20" _null_ _null_ _null_ int8out _null_ _null_ n 0));
DESCR("I/O");

/* int8um(int8) => int8 */ 
DATA(insert OID = 462 ( int8um  PGNSP PGUID 12 f f t f i 1 20 f "20" _null_ _null_ _null_ int8um _null_ _null_ n 0));
DESCR("negate");

/* int8pl(int8, int8) => int8 */ 
DATA(insert OID = 463 ( int8pl  PGNSP PGUID 12 f f t f i 2 20 f "20 20" _null_ _null_ _null_ int8pl _null_ _null_ n 0));
DESCR("add");

/* int8mi(int8, int8) => int8 */ 
DATA(insert OID = 464 ( int8mi  PGNSP PGUID 12 f f t f i 2 20 f "20 20" _null_ _null_ _null_ int8mi _null_ _null_ n 0));
DESCR("subtract");

/* int8mul(int8, int8) => int8 */ 
DATA(insert OID = 465 ( int8mul  PGNSP PGUID 12 f f t f i 2 20 f "20 20" _null_ _null_ _null_ int8mul _null_ _null_ n 0));
DESCR("multiply");

/* int8div(int8, int8) => int8 */ 
DATA(insert OID = 466 ( int8div  PGNSP PGUID 12 f f t f i 2 20 f "20 20" _null_ _null_ _null_ int8div _null_ _null_ n 0));
DESCR("divide");

/* int8eq(int8, int8) => bool */ 
DATA(insert OID = 467 ( int8eq  PGNSP PGUID 12 f f t f i 2 16 f "20 20" _null_ _null_ _null_ int8eq _null_ _null_ n 0));
DESCR("equal");

/* int8ne(int8, int8) => bool */ 
DATA(insert OID = 468 ( int8ne  PGNSP PGUID 12 f f t f i 2 16 f "20 20" _null_ _null_ _null_ int8ne _null_ _null_ n 0));
DESCR("not equal");

/* int8lt(int8, int8) => bool */ 
DATA(insert OID = 469 ( int8lt  PGNSP PGUID 12 f f t f i 2 16 f "20 20" _null_ _null_ _null_ int8lt _null_ _null_ n 0));
DESCR("less-than");

/* int8gt(int8, int8) => bool */ 
DATA(insert OID = 470 ( int8gt  PGNSP PGUID 12 f f t f i 2 16 f "20 20" _null_ _null_ _null_ int8gt _null_ _null_ n 0));
DESCR("greater-than");

/* int8le(int8, int8) => bool */ 
DATA(insert OID = 471 ( int8le  PGNSP PGUID 12 f f t f i 2 16 f "20 20" _null_ _null_ _null_ int8le _null_ _null_ n 0));
DESCR("less-than-or-equal");

/* int8ge(int8, int8) => bool */ 
DATA(insert OID = 472 ( int8ge  PGNSP PGUID 12 f f t f i 2 16 f "20 20" _null_ _null_ _null_ int8ge _null_ _null_ n 0));
DESCR("greater-than-or-equal");

/* int84eq(int8, int4) => bool */ 
DATA(insert OID = 474 ( int84eq  PGNSP PGUID 12 f f t f i 2 16 f "20 23" _null_ _null_ _null_ int84eq _null_ _null_ n 0));
DESCR("equal");

/* int84ne(int8, int4) => bool */ 
DATA(insert OID = 475 ( int84ne  PGNSP PGUID 12 f f t f i 2 16 f "20 23" _null_ _null_ _null_ int84ne _null_ _null_ n 0));
DESCR("not equal");

/* int84lt(int8, int4) => bool */ 
DATA(insert OID = 476 ( int84lt  PGNSP PGUID 12 f f t f i 2 16 f "20 23" _null_ _null_ _null_ int84lt _null_ _null_ n 0));
DESCR("less-than");

/* int84gt(int8, int4) => bool */ 
DATA(insert OID = 477 ( int84gt  PGNSP PGUID 12 f f t f i 2 16 f "20 23" _null_ _null_ _null_ int84gt _null_ _null_ n 0));
DESCR("greater-than");

/* int84le(int8, int4) => bool */ 
DATA(insert OID = 478 ( int84le  PGNSP PGUID 12 f f t f i 2 16 f "20 23" _null_ _null_ _null_ int84le _null_ _null_ n 0));
DESCR("less-than-or-equal");

/* int84ge(int8, int4) => bool */ 
DATA(insert OID = 479 ( int84ge  PGNSP PGUID 12 f f t f i 2 16 f "20 23" _null_ _null_ _null_ int84ge _null_ _null_ n 0));
DESCR("greater-than-or-equal");

/* int4(int8) => int4 */ 
DATA(insert OID = 480 ( int4  PGNSP PGUID 12 f f t f i 1 23 f "20" _null_ _null_ _null_ int84 _null_ _null_ n 0));
DESCR("convert int8 to int4");

/* int8(int4) => int8 */ 
DATA(insert OID = 481 ( int8  PGNSP PGUID 12 f f t f i 1 20 f "23" _null_ _null_ _null_ int48 _null_ _null_ n 0));
DESCR("convert int4 to int8");

/* float8(int8) => float8 */ 
DATA(insert OID = 482 ( float8  PGNSP PGUID 12 f f t f i 1 701 f "20" _null_ _null_ _null_ i8tod _null_ _null_ n 0));
DESCR("convert int8 to float8");

/* int8(float8) => int8 */ 
DATA(insert OID = 483 ( int8  PGNSP PGUID 12 f f t f i 1 20 f "701" _null_ _null_ _null_ dtoi8 _null_ _null_ n 0));
DESCR("convert float8 to int8");


/* OIDS 500 - 599 */

/* OIDS 600 - 699 */
/* float4(int8) => float4 */ 
DATA(insert OID = 652 ( float4  PGNSP PGUID 12 f f t f i 1 700 f "20" _null_ _null_ _null_ i8tof _null_ _null_ n 0));
DESCR("convert int8 to float4");

/* int8(float4) => int8 */ 
DATA(insert OID = 653 ( int8  PGNSP PGUID 12 f f t f i 1 20 f "700" _null_ _null_ _null_ ftoi8 _null_ _null_ n 0));
DESCR("convert float4 to int8");

/* int2(int8) => int2 */ 
DATA(insert OID = 714 ( int2  PGNSP PGUID 12 f f t f i 1 21 f "20" _null_ _null_ _null_ int82 _null_ _null_ n 0));
DESCR("convert int8 to int2");

/* int8(int2) => int8 */ 
DATA(insert OID = 754 ( int8  PGNSP PGUID 12 f f t f i 1 20 f "21" _null_ _null_ _null_ int28 _null_ _null_ n 0));
DESCR("convert int2 to int8");

/* int4notin(int4, text) => bool */ 
DATA(insert OID = 1285 ( int4notin  PGNSP PGUID 12 f f t f s 2 16 f "23 25" _null_ _null_ _null_ int4notin _null_ _null_ n 0));
DESCR("not in");

/* oidnotin(oid, text) => bool */ 
DATA(insert OID = 1286 ( oidnotin  PGNSP PGUID 12 f f t f s 2 16 f "26 25" _null_ _null_ _null_ oidnotin _null_ _null_ n 0));
DESCR("not in");

/* namelt(name, name) => bool */ 
DATA(insert OID = 655 ( namelt  PGNSP PGUID 12 f f t f i 2 16 f "19 19" _null_ _null_ _null_ namelt _null_ _null_ n 0));
DESCR("less-than");

/* namele(name, name) => bool */ 
DATA(insert OID = 656 ( namele  PGNSP PGUID 12 f f t f i 2 16 f "19 19" _null_ _null_ _null_ namele _null_ _null_ n 0));
DESCR("less-than-or-equal");

/* namegt(name, name) => bool */ 
DATA(insert OID = 657 ( namegt  PGNSP PGUID 12 f f t f i 2 16 f "19 19" _null_ _null_ _null_ namegt _null_ _null_ n 0));
DESCR("greater-than");

/* namege(name, name) => bool */ 
DATA(insert OID = 658 ( namege  PGNSP PGUID 12 f f t f i 2 16 f "19 19" _null_ _null_ _null_ namege _null_ _null_ n 0));
DESCR("greater-than-or-equal");

/* namene(name, name) => bool */ 
DATA(insert OID = 659 ( namene  PGNSP PGUID 12 f f t f i 2 16 f "19 19" _null_ _null_ _null_ namene _null_ _null_ n 0));
DESCR("not equal");

/* bpchar(bpchar, int4, bool) => bpchar */ 
DATA(insert OID = 668 ( bpchar  PGNSP PGUID 12 f f t f i 3 1042 f "1042 23 16" _null_ _null_ _null_ bpchar _null_ _null_ n 0));
DESCR("adjust char() to typmod length");

/* "varchar"("varchar", int4, bool) => "varchar" */ 
DATA(insert OID = 669 ( varchar  PGNSP PGUID 12 f f t f i 3 1043 f "1043 23 16" _null_ _null_ _null_ varchar _null_ _null_ n 0));
DESCR("adjust varchar() to typmod length");

/* mktinterval(abstime, abstime) => tinterval */ 
DATA(insert OID = 676 ( mktinterval  PGNSP PGUID 12 f f t f i 2 704 f "702 702" _null_ _null_ _null_ mktinterval _null_ _null_ n 0));
DESCR("convert to tinterval");

/* oidvectorne(oidvector, oidvector) => bool */ 
DATA(insert OID = 619 ( oidvectorne  PGNSP PGUID 12 f f t f i 2 16 f "30 30" _null_ _null_ _null_ oidvectorne _null_ _null_ n 0));
DESCR("not equal");

/* oidvectorlt(oidvector, oidvector) => bool */ 
DATA(insert OID = 677 ( oidvectorlt  PGNSP PGUID 12 f f t f i 2 16 f "30 30" _null_ _null_ _null_ oidvectorlt _null_ _null_ n 0));
DESCR("less-than");

/* oidvectorle(oidvector, oidvector) => bool */ 
DATA(insert OID = 678 ( oidvectorle  PGNSP PGUID 12 f f t f i 2 16 f "30 30" _null_ _null_ _null_ oidvectorle _null_ _null_ n 0));
DESCR("less-than-or-equal");

/* oidvectoreq(oidvector, oidvector) => bool */ 
DATA(insert OID = 679 ( oidvectoreq  PGNSP PGUID 12 f f t f i 2 16 f "30 30" _null_ _null_ _null_ oidvectoreq _null_ _null_ n 0));
DESCR("equal");

/* oidvectorge(oidvector, oidvector) => bool */ 
DATA(insert OID = 680 ( oidvectorge  PGNSP PGUID 12 f f t f i 2 16 f "30 30" _null_ _null_ _null_ oidvectorge _null_ _null_ n 0));
DESCR("greater-than-or-equal");

/* oidvectorgt(oidvector, oidvector) => bool */ 
DATA(insert OID = 681 ( oidvectorgt  PGNSP PGUID 12 f f t f i 2 16 f "30 30" _null_ _null_ _null_ oidvectorgt _null_ _null_ n 0));
DESCR("greater-than");


/* OIDS 700 - 799 */
/* getpgusername() => name */ 
DATA(insert OID = 710 ( getpgusername  PGNSP PGUID 12 f f t f s 0 19 f "" _null_ _null_ _null_ current_user _null_ _null_ n 0));
DESCR("deprecated -- use current_user");

/* oidlt(oid, oid) => bool */ 
DATA(insert OID = 716 ( oidlt  PGNSP PGUID 12 f f t f i 2 16 f "26 26" _null_ _null_ _null_ oidlt _null_ _null_ n 0));
DESCR("less-than");

/* oidle(oid, oid) => bool */ 
DATA(insert OID = 717 ( oidle  PGNSP PGUID 12 f f t f i 2 16 f "26 26" _null_ _null_ _null_ oidle _null_ _null_ n 0));
DESCR("less-than-or-equal");

/* octet_length(bytea) => int4 */ 
DATA(insert OID = 720 ( octet_length  PGNSP PGUID 12 f f t f i 1 23 f "17" _null_ _null_ _null_ byteaoctetlen _null_ _null_ n 0));
DESCR("octet length");

/* get_byte(bytea, int4) => int4 */ 
DATA(insert OID = 721 ( get_byte  PGNSP PGUID 12 f f t f i 2 23 f "17 23" _null_ _null_ _null_ byteaGetByte _null_ _null_ n 0));
DESCR("get byte");

/* set_byte(bytea, int4, int4) => bytea */ 
DATA(insert OID = 722 ( set_byte  PGNSP PGUID 12 f f t f i 3 17 f "17 23 23" _null_ _null_ _null_ byteaSetByte _null_ _null_ n 0));
DESCR("set byte");

/* get_bit(bytea, int4) => int4 */ 
DATA(insert OID = 723 ( get_bit  PGNSP PGUID 12 f f t f i 2 23 f "17 23" _null_ _null_ _null_ byteaGetBit _null_ _null_ n 0));
DESCR("get bit");

/* set_bit(bytea, int4, int4) => bytea */ 
DATA(insert OID = 724 ( set_bit  PGNSP PGUID 12 f f t f i 3 17 f "17 23 23" _null_ _null_ _null_ byteaSetBit _null_ _null_ n 0));
DESCR("set bit");

/* dist_pl(point, line) => float8 */ 
DATA(insert OID = 725 ( dist_pl  PGNSP PGUID 12 f f t f i 2 701 f "600 628" _null_ _null_ _null_ dist_pl _null_ _null_ n 0));
DESCR("distance between point and line");

/* dist_lb(line, box) => float8 */ 
DATA(insert OID = 726 ( dist_lb  PGNSP PGUID 12 f f t f i 2 701 f "628 603" _null_ _null_ _null_ dist_lb _null_ _null_ n 0));
DESCR("distance between line and box");

/* dist_sl(lseg, line) => float8 */ 
DATA(insert OID = 727 ( dist_sl  PGNSP PGUID 12 f f t f i 2 701 f "601 628" _null_ _null_ _null_ dist_sl _null_ _null_ n 0));
DESCR("distance between lseg and line");

/* dist_cpoly(circle, polygon) => float8 */ 
DATA(insert OID = 728 ( dist_cpoly  PGNSP PGUID 12 f f t f i 2 701 f "718 604" _null_ _null_ _null_ dist_cpoly _null_ _null_ n 0));
DESCR("distance between");

/* poly_distance(polygon, polygon) => float8 */ 
DATA(insert OID = 729 ( poly_distance  PGNSP PGUID 12 f f t f i 2 701 f "604 604" _null_ _null_ _null_ poly_distance _null_ _null_ n 0));
DESCR("distance between");

/* text_lt(text, text) => bool */ 
DATA(insert OID = 740 ( text_lt  PGNSP PGUID 12 f f t f i 2 16 f "25 25" _null_ _null_ _null_ text_lt _null_ _null_ n 0));
DESCR("less-than");

/* text_le(text, text) => bool */ 
DATA(insert OID = 741 ( text_le  PGNSP PGUID 12 f f t f i 2 16 f "25 25" _null_ _null_ _null_ text_le _null_ _null_ n 0));
DESCR("less-than-or-equal");

/* text_gt(text, text) => bool */ 
DATA(insert OID = 742 ( text_gt  PGNSP PGUID 12 f f t f i 2 16 f "25 25" _null_ _null_ _null_ text_gt _null_ _null_ n 0));
DESCR("greater-than");

/* text_ge(text, text) => bool */ 
DATA(insert OID = 743 ( text_ge  PGNSP PGUID 12 f f t f i 2 16 f "25 25" _null_ _null_ _null_ text_ge _null_ _null_ n 0));
DESCR("greater-than-or-equal");

/* "current_user"() => name */ 
DATA(insert OID = 745 ( current_user  PGNSP PGUID 12 f f t f s 0 19 f "" _null_ _null_ _null_ current_user _null_ _null_ n 0));
DESCR("current user name");

/* "session_user"() => name */ 
DATA(insert OID = 746 ( session_user  PGNSP PGUID 12 f f t f s 0 19 f "" _null_ _null_ _null_ session_user _null_ _null_ n 0));
DESCR("session user name");

/* array_eq(anyarray, anyarray) => bool */ 
DATA(insert OID = 744 ( array_eq  PGNSP PGUID 12 f f t f i 2 16 f "2277 2277" _null_ _null_ _null_ array_eq _null_ _null_ n 0));
DESCR("array equal");

/* array_ne(anyarray, anyarray) => bool */ 
DATA(insert OID = 390 ( array_ne  PGNSP PGUID 12 f f t f i 2 16 f "2277 2277" _null_ _null_ _null_ array_ne _null_ _null_ n 0));
DESCR("array not equal");

/* array_lt(anyarray, anyarray) => bool */ 
DATA(insert OID = 391 ( array_lt  PGNSP PGUID 12 f f t f i 2 16 f "2277 2277" _null_ _null_ _null_ array_lt _null_ _null_ n 0));
DESCR("array less than");

/* array_gt(anyarray, anyarray) => bool */ 
DATA(insert OID = 392 ( array_gt  PGNSP PGUID 12 f f t f i 2 16 f "2277 2277" _null_ _null_ _null_ array_gt _null_ _null_ n 0));
DESCR("array greater than");

/* array_le(anyarray, anyarray) => bool */ 
DATA(insert OID = 393 ( array_le  PGNSP PGUID 12 f f t f i 2 16 f "2277 2277" _null_ _null_ _null_ array_le _null_ _null_ n 0));
DESCR("array less than or equal");

/* array_ge(anyarray, anyarray) => bool */ 
DATA(insert OID = 396 ( array_ge  PGNSP PGUID 12 f f t f i 2 16 f "2277 2277" _null_ _null_ _null_ array_ge _null_ _null_ n 0));
DESCR("array greater than or equal");

/* array_dims(anyarray) => text */ 
DATA(insert OID = 747 ( array_dims  PGNSP PGUID 12 f f t f i 1 25 f "2277" _null_ _null_ _null_ array_dims _null_ _null_ n 0));
DESCR("array dimensions");

/* array_in(cstring, oid, int4) => anyarray */ 
DATA(insert OID = 750 ( array_in  PGNSP PGUID 12 f f t f s 3 2277 f "2275 26 23" _null_ _null_ _null_ array_in _null_ _null_ n 0));
DESCR("I/O");


#define ARRAY_OUT_OID 751
/* array_out(anyarray) => cstring */ 
DATA(insert OID = 751 ( array_out  PGNSP PGUID 12 f f t f s 1 2275 f "2277" _null_ _null_ _null_ array_out _null_ _null_ n 0));
DESCR("I/O");

/* array_lower(anyarray, int4) => int4 */ 
DATA(insert OID = 2091 ( array_lower  PGNSP PGUID 12 f f t f i 2 23 f "2277 23" _null_ _null_ _null_ array_lower _null_ _null_ n 0));
DESCR("array lower dimension");

/* array_upper(anyarray, int4) => int4 */ 
DATA(insert OID = 2092 ( array_upper  PGNSP PGUID 12 f f t f i 2 23 f "2277 23" _null_ _null_ _null_ array_upper _null_ _null_ n 0));
DESCR("array upper dimension");

/* array_append(anyarray, anyelement) => anyarray */ 
DATA(insert OID = 378 ( array_append  PGNSP PGUID 12 f f f f i 2 2277 f "2277 2283" _null_ _null_ _null_ array_push _null_ _null_ n 0));
DESCR("append element onto end of array");

/* array_prepend(anyelement, anyarray) => anyarray */ 
DATA(insert OID = 379 ( array_prepend  PGNSP PGUID 12 f f f f i 2 2277 f "2283 2277" _null_ _null_ _null_ array_push _null_ _null_ n 0));
DESCR("prepend element onto front of array");

/* array_cat(anyarray, anyarray) => anyarray */ 
DATA(insert OID = 383 ( array_cat  PGNSP PGUID 12 f f f f i 2 2277 f "2277 2277" _null_ _null_ _null_ array_cat _null_ _null_ n 0));
DESCR("concatenate two arrays");

/* array_coerce(anyarray) => anyarray */ 
DATA(insert OID = 384 ( array_coerce  PGNSP PGUID 12 f f t f s 1 2277 f "2277" _null_ _null_ _null_ array_type_coerce _null_ _null_ n 0));
DESCR("coerce array to another array type");

/* string_to_array(text, text) => _text */ 
DATA(insert OID = 394 ( string_to_array  PGNSP PGUID 12 f f t f i 2 1009 f "25 25" _null_ _null_ _null_ text_to_array _null_ _null_ n 0));
DESCR("split delimited text into text[]");

/* array_to_string(anyarray, text) => text */ 
DATA(insert OID = 395 ( array_to_string  PGNSP PGUID 12 f f t f i 2 25 f "2277 25" _null_ _null_ _null_ array_to_text _null_ _null_ n 0));
DESCR("concatenate array elements, using delimiter, into text");

/* array_larger(anyarray, anyarray) => anyarray */ 
DATA(insert OID = 515 ( array_larger  PGNSP PGUID 12 f f t f i 2 2277 f "2277 2277" _null_ _null_ _null_ array_larger _null_ _null_ n 0));
DESCR("larger of two");

/* array_smaller(anyarray, anyarray) => anyarray */ 
DATA(insert OID = 516 ( array_smaller  PGNSP PGUID 12 f f t f i 2 2277 f "2277 2277" _null_ _null_ _null_ array_smaller _null_ _null_ n 0));
DESCR("smaller of two");


/* MPP -- array_add -- special for prospective customer  */
/* array_add(_int4, _int4) => _int4 */ 
DATA(insert OID = 6012 ( array_add  PGNSP PGUID 12 f f t f i 2 1007 f "1007 1007" _null_ _null_ _null_ array_int4_add _null_ _null_ n 0));
DESCR("itemwise add two integer arrays");

/* array_agg_transfn(internal, anyelement) => internal */ 
DATA(insert OID = 2908 ( array_agg_transfn  PGNSP PGUID 12 f f f f i 2 2281 f "2281 2283" _null_ _null_ _null_ array_agg_transfn _null_ _null_ n 0));
DESCR("array_agg transition function");

/* array_agg_finalfn(internal) => anyarray */ 
DATA(insert OID = 2909 ( array_agg_finalfn  PGNSP PGUID 12 f f f f i 1 2277 f "2281" _null_ _null_ _null_ array_agg_finalfn _null_ _null_ n 0));
DESCR("array_agg final function");

/* array_agg(anyelement) => anyarray */ 
DATA(insert OID = 2910 ( array_agg  PGNSP PGUID 12 t f f f i 1 2277 f "2283" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));
DESCR("concatenate aggregate input into an array");

/* string_agg_transfn(internal, text) => internal */ 
DATA(insert OID = 3534 ( string_agg_transfn  PGNSP PGUID 12 f f f f i 2 2281 f "2281 25" _null_ _null_ _null_ string_agg_transfn _null_ _null_ n 0));
DESCR("string_agg(text) transition function");

/* string_agg_delim_transfn(internal, text, text) => internal */ 
DATA(insert OID = 3535 ( string_agg_delim_transfn  PGNSP PGUID 12 f f f f i 3 2281 f "2281 25 25" _null_ _null_ _null_ string_agg_delim_transfn _null_ _null_ n 0));
DESCR("string_agg(text, text) transition function");

/* string_agg_finalfn(internal) => text */ 
DATA(insert OID = 3536 ( string_agg_finalfn  PGNSP PGUID 12 f f f f i 1 25 f "2281" _null_ _null_ _null_ string_agg_finalfn _null_ _null_ n 0));
DESCR("string_agg final function");

/* string_agg(text) => text */ 
DATA(insert OID = 3537 ( string_agg  PGNSP PGUID 12 t f f f i 1 25 f "25" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));
DESCR("concatenate aggregate input into an string");

/* string_agg(text, text) => text */ 
DATA(insert OID = 3538 ( string_agg  PGNSP PGUID 12 t f f f i 2 25 f "25 25" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));
DESCR("concatenate aggregate input into an string with delimiter");

/* smgrin(cstring) => smgr */ 
DATA(insert OID = 760 ( smgrin  PGNSP PGUID 12 f f t f s 1 210 f "2275" _null_ _null_ _null_ smgrin _null_ _null_ n 0));
DESCR("I/O");

/* smgrout(smgr) => cstring */ 
DATA(insert OID = 761 ( smgrout  PGNSP PGUID 12 f f t f s 1 2275 f "210" _null_ _null_ _null_ smgrout _null_ _null_ n 0));
DESCR("I/O");

/* smgreq(smgr, smgr) => bool */ 
DATA(insert OID = 762 ( smgreq  PGNSP PGUID 12 f f t f i 2 16 f "210 210" _null_ _null_ _null_ smgreq _null_ _null_ n 0));
DESCR("storage manager");

/* smgrne(smgr, smgr) => bool */ 
DATA(insert OID = 763 ( smgrne  PGNSP PGUID 12 f f t f i 2 16 f "210 210" _null_ _null_ _null_ smgrne _null_ _null_ n 0));
DESCR("storage manager");

/* lo_import(text) => oid */ 
DATA(insert OID = 764 ( lo_import  PGNSP PGUID 12 f f t f v 1 26 f "25" _null_ _null_ _null_ lo_import _null_ _null_ n 0));
DESCR("large object import");

/* lo_export(oid, text) => int4 */ 
DATA(insert OID = 765 ( lo_export  PGNSP PGUID 12 f f t f v 2 23 f "26 25" _null_ _null_ _null_ lo_export _null_ _null_ n 0));
DESCR("large object export");

/* int4inc(int4) => int4 */ 
DATA(insert OID = 766 ( int4inc  PGNSP PGUID 12 f f t f i 1 23 f "23" _null_ _null_ _null_ int4inc _null_ _null_ n 0));
DESCR("increment");

/* int4larger(int4, int4) => int4 */ 
DATA(insert OID = 768 ( int4larger  PGNSP PGUID 12 f f t f i 2 23 f "23 23" _null_ _null_ _null_ int4larger _null_ _null_ n 0));
DESCR("larger of two");

/* int4smaller(int4, int4) => int4 */ 
DATA(insert OID = 769 ( int4smaller  PGNSP PGUID 12 f f t f i 2 23 f "23 23" _null_ _null_ _null_ int4smaller _null_ _null_ n 0));
DESCR("smaller of two");

/* int2larger(int2, int2) => int2 */ 
DATA(insert OID = 770 ( int2larger  PGNSP PGUID 12 f f t f i 2 21 f "21 21" _null_ _null_ _null_ int2larger _null_ _null_ n 0));
DESCR("larger of two");

/* int2smaller(int2, int2) => int2 */ 
DATA(insert OID = 771 ( int2smaller  PGNSP PGUID 12 f f t f i 2 21 f "21 21" _null_ _null_ _null_ int2smaller _null_ _null_ n 0));
DESCR("smaller of two");

/* gistgettuple(internal, internal) => bool */ 
DATA(insert OID = 774 ( gistgettuple  PGNSP PGUID 12 f f t f v 2 16 f "2281 2281" _null_ _null_ _null_ gistgettuple _null_ _null_ n 0));
DESCR("gist(internal)");

/* gistgetmulti(internal, internal) => internal */ 
DATA(insert OID = 638 ( gistgetmulti  PGNSP PGUID 12 f f t f v 2 2281 f "2281 2281" _null_ _null_ _null_ gistgetmulti _null_ _null_ n 0));
DESCR("gist(internal)");

/* gistinsert(internal, internal, internal, internal, internal, internal) => bool */ 
DATA(insert OID = 775 ( gistinsert  PGNSP PGUID 12 f f t f v 6 16 f "2281 2281 2281 2281 2281 2281" _null_ _null_ _null_ gistinsert _null_ _null_ n 0));
DESCR("gist(internal)");

/* gistbeginscan(internal, internal, internal) => internal */ 
DATA(insert OID = 777 ( gistbeginscan  PGNSP PGUID 12 f f t f v 3 2281 f "2281 2281 2281" _null_ _null_ _null_ gistbeginscan _null_ _null_ n 0));
DESCR("gist(internal)");

/* gistrescan(internal, internal) => void */ 
DATA(insert OID = 778 ( gistrescan  PGNSP PGUID 12 f f t f v 2 2278 f "2281 2281" _null_ _null_ _null_ gistrescan _null_ _null_ n 0));
DESCR("gist(internal)");

/* gistendscan(internal) => void */ 
DATA(insert OID = 779 ( gistendscan  PGNSP PGUID 12 f f t f v 1 2278 f "2281" _null_ _null_ _null_ gistendscan _null_ _null_ n 0));
DESCR("gist(internal)");

/* gistmarkpos(internal) => void */ 
DATA(insert OID = 780 ( gistmarkpos  PGNSP PGUID 12 f f t f v 1 2278 f "2281" _null_ _null_ _null_ gistmarkpos _null_ _null_ n 0));
DESCR("gist(internal)");

/* gistrestrpos(internal) => void */ 
DATA(insert OID = 781 ( gistrestrpos  PGNSP PGUID 12 f f t f v 1 2278 f "2281" _null_ _null_ _null_ gistrestrpos _null_ _null_ n 0));
DESCR("gist(internal)");

/* gistbuild(internal, internal, internal) => internal */ 
DATA(insert OID = 782 ( gistbuild  PGNSP PGUID 12 f f t f v 3 2281 f "2281 2281 2281" _null_ _null_ _null_ gistbuild _null_ _null_ n 0));
DESCR("gist(internal)");

/* gistbulkdelete(internal, internal, internal, internal) => internal */ 
DATA(insert OID = 776 ( gistbulkdelete  PGNSP PGUID 12 f f t f v 4 2281 f "2281 2281 2281 2281" _null_ _null_ _null_ gistbulkdelete _null_ _null_ n 0));
DESCR("gist(internal)");

/* gistvacuumcleanup(internal, internal) => internal */ 
DATA(insert OID = 2561 ( gistvacuumcleanup  PGNSP PGUID 12 f f t f v 2 2281 f "2281 2281" _null_ _null_ _null_ gistvacuumcleanup _null_ _null_ n 0));
DESCR("gist(internal)");

/* gistcostestimate(internal, internal, internal, internal, internal, internal, internal, internal) => void */ 
DATA(insert OID = 772 ( gistcostestimate  PGNSP PGUID 12 f f t f v 8 2278 f "2281 2281 2281 2281 2281 2281 2281 2281" _null_ _null_ _null_ gistcostestimate _null_ _null_ n 0));
DESCR("gist(internal)");

/* gistoptions(_text, bool) => bytea */ 
DATA(insert OID = 2787 ( gistoptions  PGNSP PGUID 12 f f t f s 2 17 f "1009 16" _null_ _null_ _null_ gistoptions _null_ _null_ n 0));
DESCR("gist(internal)");

/* tintervaleq(tinterval, tinterval) => bool */ 
DATA(insert OID = 784 ( tintervaleq  PGNSP PGUID 12 f f t f i 2 16 f "704 704" _null_ _null_ _null_ tintervaleq _null_ _null_ n 0));
DESCR("equal");

/* tintervalne(tinterval, tinterval) => bool */ 
DATA(insert OID = 785 ( tintervalne  PGNSP PGUID 12 f f t f i 2 16 f "704 704" _null_ _null_ _null_ tintervalne _null_ _null_ n 0));
DESCR("not equal");

/* tintervallt(tinterval, tinterval) => bool */ 
DATA(insert OID = 786 ( tintervallt  PGNSP PGUID 12 f f t f i 2 16 f "704 704" _null_ _null_ _null_ tintervallt _null_ _null_ n 0));
DESCR("less-than");

/* tintervalgt(tinterval, tinterval) => bool */ 
DATA(insert OID = 787 ( tintervalgt  PGNSP PGUID 12 f f t f i 2 16 f "704 704" _null_ _null_ _null_ tintervalgt _null_ _null_ n 0));
DESCR("greater-than");

/* tintervalle(tinterval, tinterval) => bool */ 
DATA(insert OID = 788 ( tintervalle  PGNSP PGUID 12 f f t f i 2 16 f "704 704" _null_ _null_ _null_ tintervalle _null_ _null_ n 0));
DESCR("less-than-or-equal");

/* tintervalge(tinterval, tinterval) => bool */ 
DATA(insert OID = 789 ( tintervalge  PGNSP PGUID 12 f f t f i 2 16 f "704 704" _null_ _null_ _null_ tintervalge _null_ _null_ n 0));
DESCR("greater-than-or-equal");


/* OIDS 800 - 899 */
/* oid(text) => oid */ 
DATA(insert OID = 817 ( oid  PGNSP PGUID 12 f f t f i 1 26 f "25" _null_ _null_ _null_ text_oid _null_ _null_ n 0));
DESCR("convert text to oid");

/* int2(text) => int2 */ 
DATA(insert OID = 818 ( int2  PGNSP PGUID 12 f f t f i 1 21 f "25" _null_ _null_ _null_ text_int2 _null_ _null_ n 0));
DESCR("convert text to int2");

/* int4(text) => int4 */ 
DATA(insert OID = 819 ( int4  PGNSP PGUID 12 f f t f i 1 23 f "25" _null_ _null_ _null_ text_int4 _null_ _null_ n 0));
DESCR("convert text to int4");

/* float8(text) => float8 */ 
DATA(insert OID = 838 ( float8  PGNSP PGUID 12 f f t f i 1 701 f "25" _null_ _null_ _null_ text_float8 _null_ _null_ n 0));
DESCR("convert text to float8");

/* float4(text) => float4 */ 
DATA(insert OID = 839 ( float4  PGNSP PGUID 12 f f t f i 1 700 f "25" _null_ _null_ _null_ text_float4 _null_ _null_ n 0));
DESCR("convert text to float4");

/* text(float8) => text */ 
DATA(insert OID = 840 ( text  PGNSP PGUID 12 f f t f i 1 25 f "701" _null_ _null_ _null_ float8_text _null_ _null_ n 0));
DESCR("convert float8 to text");

/* text(float4) => text */ 
DATA(insert OID = 841 ( text  PGNSP PGUID 12 f f t f i 1 25 f "700" _null_ _null_ _null_ float4_text _null_ _null_ n 0));
DESCR("convert float4 to text");

/* cash_mul_flt4(money, float4) => money */ 
DATA(insert OID = 846 ( cash_mul_flt4  PGNSP PGUID 12 f f t f i 2 790 f "790 700" _null_ _null_ _null_ cash_mul_flt4 _null_ _null_ n 0));
DESCR("multiply");

/* cash_div_flt4(money, float4) => money */ 
DATA(insert OID = 847 ( cash_div_flt4  PGNSP PGUID 12 f f t f i 2 790 f "790 700" _null_ _null_ _null_ cash_div_flt4 _null_ _null_ n 0));
DESCR("divide");

/* flt4_mul_cash(float4, money) => money */ 
DATA(insert OID = 848 ( flt4_mul_cash  PGNSP PGUID 12 f f t f i 2 790 f "700 790" _null_ _null_ _null_ flt4_mul_cash _null_ _null_ n 0));
DESCR("multiply");

/* "position"(text, text) => int4 */ 
DATA(insert OID = 849 ( position  PGNSP PGUID 12 f f t f i 2 23 f "25 25" _null_ _null_ _null_ textpos _null_ _null_ n 0));
DESCR("return position of substring");

/* textlike(text, text) => bool */ 
DATA(insert OID = 850 ( textlike  PGNSP PGUID 12 f f t f i 2 16 f "25 25" _null_ _null_ _null_ textlike _null_ _null_ n 0));
DESCR("matches LIKE expression");

/* textnlike(text, text) => bool */ 
DATA(insert OID = 851 ( textnlike  PGNSP PGUID 12 f f t f i 2 16 f "25 25" _null_ _null_ _null_ textnlike _null_ _null_ n 0));
DESCR("does not match LIKE expression");

/* int48eq(int4, int8) => bool */ 
DATA(insert OID = 852 ( int48eq  PGNSP PGUID 12 f f t f i 2 16 f "23 20" _null_ _null_ _null_ int48eq _null_ _null_ n 0));
DESCR("equal");

/* int48ne(int4, int8) => bool */ 
DATA(insert OID = 853 ( int48ne  PGNSP PGUID 12 f f t f i 2 16 f "23 20" _null_ _null_ _null_ int48ne _null_ _null_ n 0));
DESCR("not equal");

/* int48lt(int4, int8) => bool */ 
DATA(insert OID = 854 ( int48lt  PGNSP PGUID 12 f f t f i 2 16 f "23 20" _null_ _null_ _null_ int48lt _null_ _null_ n 0));
DESCR("less-than");

/* int48gt(int4, int8) => bool */ 
DATA(insert OID = 855 ( int48gt  PGNSP PGUID 12 f f t f i 2 16 f "23 20" _null_ _null_ _null_ int48gt _null_ _null_ n 0));
DESCR("greater-than");

/* int48le(int4, int8) => bool */ 
DATA(insert OID = 856 ( int48le  PGNSP PGUID 12 f f t f i 2 16 f "23 20" _null_ _null_ _null_ int48le _null_ _null_ n 0));
DESCR("less-than-or-equal");

/* int48ge(int4, int8) => bool */ 
DATA(insert OID = 857 ( int48ge  PGNSP PGUID 12 f f t f i 2 16 f "23 20" _null_ _null_ _null_ int48ge _null_ _null_ n 0));
DESCR("greater-than-or-equal");

/* namelike(name, text) => bool */ 
DATA(insert OID = 858 ( namelike  PGNSP PGUID 12 f f t f i 2 16 f "19 25" _null_ _null_ _null_ namelike _null_ _null_ n 0));
DESCR("matches LIKE expression");

/* namenlike(name, text) => bool */ 
DATA(insert OID = 859 ( namenlike  PGNSP PGUID 12 f f t f i 2 16 f "19 25" _null_ _null_ _null_ namenlike _null_ _null_ n 0));
DESCR("does not match LIKE expression");

/* bpchar("char") => bpchar */ 
DATA(insert OID = 860 ( bpchar  PGNSP PGUID 12 f f t f i 1 1042 f "18" _null_ _null_ _null_ char_bpchar _null_ _null_ n 0));
DESCR("convert char to char()");

/* current_database() => name */ 
DATA(insert OID = 861 ( current_database  PGNSP PGUID 12 f f t f i 0 19 f "" _null_ _null_ _null_ current_database _null_ _null_ n 0));
DESCR("returns the current database");

/* current_query() => text */ 
DATA(insert OID = 820 ( current_query  PGNSP PGUID 12 f f f f v 0 25 f "" _null_ _null_ _null_ current_query _null_ _null_ n 0));
DESCR("returns the currently executing query");

/* int4_mul_cash(int4, money) => money */ 
DATA(insert OID = 862 ( int4_mul_cash  PGNSP PGUID 12 f f t f i 2 790 f "23 790" _null_ _null_ _null_ int4_mul_cash _null_ _null_ n 0));
DESCR("multiply");

/* int2_mul_cash(int2, money) => money */ 
DATA(insert OID = 863 ( int2_mul_cash  PGNSP PGUID 12 f f t f i 2 790 f "21 790" _null_ _null_ _null_ int2_mul_cash _null_ _null_ n 0));
DESCR("multiply");

/* cash_mul_int4(money, int4) => money */ 
DATA(insert OID = 864 ( cash_mul_int4  PGNSP PGUID 12 f f t f i 2 790 f "790 23" _null_ _null_ _null_ cash_mul_int4 _null_ _null_ n 0));
DESCR("multiply");

/* cash_div_int4(money, int4) => money */ 
DATA(insert OID = 865 ( cash_div_int4  PGNSP PGUID 12 f f t f i 2 790 f "790 23" _null_ _null_ _null_ cash_div_int4 _null_ _null_ n 0));
DESCR("divide");

/* cash_mul_int2(money, int2) => money */ 
DATA(insert OID = 866 ( cash_mul_int2  PGNSP PGUID 12 f f t f i 2 790 f "790 21" _null_ _null_ _null_ cash_mul_int2 _null_ _null_ n 0));
DESCR("multiply");

/* cash_div_int2(money, int2) => money */ 
DATA(insert OID = 867 ( cash_div_int2  PGNSP PGUID 12 f f t f i 2 790 f "790 21" _null_ _null_ _null_ cash_div_int2 _null_ _null_ n 0));
DESCR("divide");

/* cash_in(cstring) => money */ 
DATA(insert OID = 886 ( cash_in  PGNSP PGUID 12 f f t f i 1 790 f "2275" _null_ _null_ _null_ cash_in _null_ _null_ n 0));
DESCR("I/O");

/* cash_out(money) => cstring */ 
DATA(insert OID = 887 ( cash_out  PGNSP PGUID 12 f f t f i 1 2275 f "790" _null_ _null_ _null_ cash_out _null_ _null_ n 0));
DESCR("I/O");

/* cash_eq(money, money) => bool */ 
DATA(insert OID = 888 ( cash_eq  PGNSP PGUID 12 f f t f i 2 16 f "790 790" _null_ _null_ _null_ cash_eq _null_ _null_ n 0));
DESCR("equal");

/* cash_ne(money, money) => bool */ 
DATA(insert OID = 889 ( cash_ne  PGNSP PGUID 12 f f t f i 2 16 f "790 790" _null_ _null_ _null_ cash_ne _null_ _null_ n 0));
DESCR("not equal");

/* cash_lt(money, money) => bool */ 
DATA(insert OID = 890 ( cash_lt  PGNSP PGUID 12 f f t f i 2 16 f "790 790" _null_ _null_ _null_ cash_lt _null_ _null_ n 0));
DESCR("less-than");

/* cash_le(money, money) => bool */ 
DATA(insert OID = 891 ( cash_le  PGNSP PGUID 12 f f t f i 2 16 f "790 790" _null_ _null_ _null_ cash_le _null_ _null_ n 0));
DESCR("less-than-or-equal");

/* cash_gt(money, money) => bool */ 
DATA(insert OID = 892 ( cash_gt  PGNSP PGUID 12 f f t f i 2 16 f "790 790" _null_ _null_ _null_ cash_gt _null_ _null_ n 0));
DESCR("greater-than");

/* cash_ge(money, money) => bool */ 
DATA(insert OID = 893 ( cash_ge  PGNSP PGUID 12 f f t f i 2 16 f "790 790" _null_ _null_ _null_ cash_ge _null_ _null_ n 0));
DESCR("greater-than-or-equal");

/* cash_pl(money, money) => money */ 
DATA(insert OID = 894 ( cash_pl  PGNSP PGUID 12 f f t f i 2 790 f "790 790" _null_ _null_ _null_ cash_pl _null_ _null_ n 0));
DESCR("add");

/* cash_mi(money, money) => money */ 
DATA(insert OID = 895 ( cash_mi  PGNSP PGUID 12 f f t f i 2 790 f "790 790" _null_ _null_ _null_ cash_mi _null_ _null_ n 0));
DESCR("subtract");

/* cash_mul_flt8(money, float8) => money */ 
DATA(insert OID = 896 ( cash_mul_flt8  PGNSP PGUID 12 f f t f i 2 790 f "790 701" _null_ _null_ _null_ cash_mul_flt8 _null_ _null_ n 0));
DESCR("multiply");

/* cash_div_flt8(money, float8) => money */ 
DATA(insert OID = 897 ( cash_div_flt8  PGNSP PGUID 12 f f t f i 2 790 f "790 701" _null_ _null_ _null_ cash_div_flt8 _null_ _null_ n 0));
DESCR("divide");

/* cashlarger(money, money) => money */ 
DATA(insert OID = 898 ( cashlarger  PGNSP PGUID 12 f f t f i 2 790 f "790 790" _null_ _null_ _null_ cashlarger _null_ _null_ n 0));
DESCR("larger of two");

/* cashsmaller(money, money) => money */ 
DATA(insert OID = 899 ( cashsmaller  PGNSP PGUID 12 f f t f i 2 790 f "790 790" _null_ _null_ _null_ cashsmaller _null_ _null_ n 0));
DESCR("smaller of two");

/* flt8_mul_cash(float8, money) => money */ 
DATA(insert OID = 919 ( flt8_mul_cash  PGNSP PGUID 12 f f t f i 2 790 f "701 790" _null_ _null_ _null_ flt8_mul_cash _null_ _null_ n 0));
DESCR("multiply");

/* cash_words(money) => text */ 
DATA(insert OID = 935 ( cash_words  PGNSP PGUID 12 f f t f i 1 25 f "790" _null_ _null_ _null_ cash_words _null_ _null_ n 0));
DESCR("output amount as words");


/* OIDS 900 - 999 */
/* mod(int2, int2) => int2 */ 
DATA(insert OID = 940 ( mod  PGNSP PGUID 12 f f t f i 2 21 f "21 21" _null_ _null_ _null_ int2mod _null_ _null_ n 0));
DESCR("modulus");

/* mod(int4, int4) => int4 */ 
DATA(insert OID = 941 ( mod  PGNSP PGUID 12 f f t f i 2 23 f "23 23" _null_ _null_ _null_ int4mod _null_ _null_ n 0));
DESCR("modulus");

/* mod(int2, int4) => int4 */ 
DATA(insert OID = 942 ( mod  PGNSP PGUID 12 f f t f i 2 23 f "21 23" _null_ _null_ _null_ int24mod _null_ _null_ n 0));
DESCR("modulus");

/* mod(int4, int2) => int4 */ 
DATA(insert OID = 943 ( mod  PGNSP PGUID 12 f f t f i 2 23 f "23 21" _null_ _null_ _null_ int42mod _null_ _null_ n 0));
DESCR("modulus");

/* int8mod(int8, int8) => int8 */ 
DATA(insert OID = 945 ( int8mod  PGNSP PGUID 12 f f t f i 2 20 f "20 20" _null_ _null_ _null_ int8mod _null_ _null_ n 0));
DESCR("modulus");

/* mod(int8, int8) => int8 */ 
DATA(insert OID = 947 ( mod  PGNSP PGUID 12 f f t f i 2 20 f "20 20" _null_ _null_ _null_ int8mod _null_ _null_ n 0));
DESCR("modulus");

/* "char"(text) => "char" */ 
DATA(insert OID = 944 ( char  PGNSP PGUID 12 f f t f i 1 18 f "25" _null_ _null_ _null_ text_char _null_ _null_ n 0));
DESCR("convert text to char");

/* text("char") => text */ 
DATA(insert OID = 946 ( text  PGNSP PGUID 12 f f t f i 1 25 f "18" _null_ _null_ _null_ char_text _null_ _null_ n 0));
DESCR("convert char to text");

/* istrue(bool) => bool */ 
DATA(insert OID = 950 ( istrue  PGNSP PGUID 12 f f f f i 1 16 f "16" _null_ _null_ _null_ istrue _null_ _null_ n 0));
DESCR("bool is true (not false or unknown)");

/* isfalse(bool) => bool */ 
DATA(insert OID = 951 ( isfalse  PGNSP PGUID 12 f f f f i 1 16 f "16" _null_ _null_ _null_ isfalse _null_ _null_ n 0));
DESCR("bool is false (not true or unknown)");

/* lo_open(oid, int4) => int4 */ 
DATA(insert OID = 952 ( lo_open  PGNSP PGUID 12 f f t f v 2 23 f "26 23" _null_ _null_ _null_ lo_open _null_ _null_ n 0));
DESCR("large object open");

/* lo_close(int4) => int4 */ 
DATA(insert OID = 953 ( lo_close  PGNSP PGUID 12 f f t f v 1 23 f "23" _null_ _null_ _null_ lo_close _null_ _null_ n 0));
DESCR("large object close");

/* loread(int4, int4) => bytea */ 
DATA(insert OID = 954 ( loread  PGNSP PGUID 12 f f t f v 2 17 f "23 23" _null_ _null_ _null_ loread _null_ _null_ n 0));
DESCR("large object read");

/* lowrite(int4, bytea) => int4 */ 
DATA(insert OID = 955 ( lowrite  PGNSP PGUID 12 f f t f v 2 23 f "23 17" _null_ _null_ _null_ lowrite _null_ _null_ n 0));
DESCR("large object write");

/* lo_lseek(int4, int4, int4) => int4 */ 
DATA(insert OID = 956 ( lo_lseek  PGNSP PGUID 12 f f t f v 3 23 f "23 23 23" _null_ _null_ _null_ lo_lseek _null_ _null_ n 0));
DESCR("large object seek");

/* lo_creat(int4) => oid */ 
DATA(insert OID = 957 ( lo_creat  PGNSP PGUID 12 f f t f v 1 26 f "23" _null_ _null_ _null_ lo_creat _null_ _null_ n 0));
DESCR("large object create");

/* lo_create(oid) => oid */ 
DATA(insert OID = 715 ( lo_create  PGNSP PGUID 12 f f t f v 1 26 f "26" _null_ _null_ _null_ lo_create _null_ _null_ n 0));
DESCR("large object create");

/* lo_tell(int4) => int4 */ 
DATA(insert OID = 958 ( lo_tell  PGNSP PGUID 12 f f t f v 1 23 f "23" _null_ _null_ _null_ lo_tell _null_ _null_ n 0));
DESCR("large object position");

/* lo_truncate(int4, int4) => int4 */ 
DATA(insert OID = 828 ( lo_truncate  PGNSP PGUID 12 f f t f v 2 23 f "23 23" _null_ _null_ _null_ lo_truncate _null_ _null_ n 0));
DESCR("truncate large object");

/* on_pl(point, line) => bool */ 
DATA(insert OID = 959 ( on_pl  PGNSP PGUID 12 f f t f i 2 16 f "600 628" _null_ _null_ _null_ on_pl _null_ _null_ n 0));
DESCR("point on line?");

/* on_sl(lseg, line) => bool */ 
DATA(insert OID = 960 ( on_sl  PGNSP PGUID 12 f f t f i 2 16 f "601 628" _null_ _null_ _null_ on_sl _null_ _null_ n 0));
DESCR("lseg on line?");

/* close_pl(point, line) => point */ 
DATA(insert OID = 961 ( close_pl  PGNSP PGUID 12 f f t f i 2 600 f "600 628" _null_ _null_ _null_ close_pl _null_ _null_ n 0));
DESCR("closest point on line");

/* close_sl(lseg, line) => point */ 
DATA(insert OID = 962 ( close_sl  PGNSP PGUID 12 f f t f i 2 600 f "601 628" _null_ _null_ _null_ close_sl _null_ _null_ n 0));
DESCR("closest point to line segment on line");

/* close_lb(line, box) => point */ 
DATA(insert OID = 963 ( close_lb  PGNSP PGUID 12 f f t f i 2 600 f "628 603" _null_ _null_ _null_ close_lb _null_ _null_ n 0));
DESCR("closest point to line on box");

/* lo_unlink(oid) => int4 */ 
DATA(insert OID = 964 ( lo_unlink  PGNSP PGUID 12 f f t f v 1 23 f "26" _null_ _null_ _null_ lo_unlink _null_ _null_ n 0));
DESCR("large object unlink(delete)");

/* path_inter(path, path) => bool */ 
DATA(insert OID = 973 ( path_inter  PGNSP PGUID 12 f f t f i 2 16 f "602 602" _null_ _null_ _null_ path_inter _null_ _null_ n 0));
DESCR("intersect?");

/* area(box) => float8 */ 
DATA(insert OID = 975 ( area  PGNSP PGUID 12 f f t f i 1 701 f "603" _null_ _null_ _null_ box_area _null_ _null_ n 0));
DESCR("box area");

/* width(box) => float8 */ 
DATA(insert OID = 976 ( width  PGNSP PGUID 12 f f t f i 1 701 f "603" _null_ _null_ _null_ box_width _null_ _null_ n 0));
DESCR("box width");

/* height(box) => float8 */ 
DATA(insert OID = 977 ( height  PGNSP PGUID 12 f f t f i 1 701 f "603" _null_ _null_ _null_ box_height _null_ _null_ n 0));
DESCR("box height");

/* box_distance(box, box) => float8 */ 
DATA(insert OID = 978 ( box_distance  PGNSP PGUID 12 f f t f i 2 701 f "603 603" _null_ _null_ _null_ box_distance _null_ _null_ n 0));
DESCR("distance between boxes");

/* area(path) => float8 */ 
DATA(insert OID = 979 ( area  PGNSP PGUID 12 f f t f i 1 701 f "602" _null_ _null_ _null_ path_area _null_ _null_ n 0));
DESCR("area of a closed path");

/* box_intersect(box, box) => box */ 
DATA(insert OID = 980 ( box_intersect  PGNSP PGUID 12 f f t f i 2 603 f "603 603" _null_ _null_ _null_ box_intersect _null_ _null_ n 0));
DESCR("box intersection (another box)");

/* diagonal(box) => lseg */ 
DATA(insert OID = 981 ( diagonal  PGNSP PGUID 12 f f t f i 1 601 f "603" _null_ _null_ _null_ box_diagonal _null_ _null_ n 0));
DESCR("box diagonal");

/* path_n_lt(path, path) => bool */ 
DATA(insert OID = 982 ( path_n_lt  PGNSP PGUID 12 f f t f i 2 16 f "602 602" _null_ _null_ _null_ path_n_lt _null_ _null_ n 0));
DESCR("less-than");

/* path_n_gt(path, path) => bool */ 
DATA(insert OID = 983 ( path_n_gt  PGNSP PGUID 12 f f t f i 2 16 f "602 602" _null_ _null_ _null_ path_n_gt _null_ _null_ n 0));
DESCR("greater-than");

/* path_n_eq(path, path) => bool */ 
DATA(insert OID = 984 ( path_n_eq  PGNSP PGUID 12 f f t f i 2 16 f "602 602" _null_ _null_ _null_ path_n_eq _null_ _null_ n 0));
DESCR("equal");

/* path_n_le(path, path) => bool */ 
DATA(insert OID = 985 ( path_n_le  PGNSP PGUID 12 f f t f i 2 16 f "602 602" _null_ _null_ _null_ path_n_le _null_ _null_ n 0));
DESCR("less-than-or-equal");

/* path_n_ge(path, path) => bool */ 
DATA(insert OID = 986 ( path_n_ge  PGNSP PGUID 12 f f t f i 2 16 f "602 602" _null_ _null_ _null_ path_n_ge _null_ _null_ n 0));
DESCR("greater-than-or-equal");

/* path_length(path) => float8 */ 
DATA(insert OID = 987 ( path_length  PGNSP PGUID 12 f f t f i 1 701 f "602" _null_ _null_ _null_ path_length _null_ _null_ n 0));
DESCR("sum of path segment lengths");

/* point_ne(point, point) => bool */ 
DATA(insert OID = 988 ( point_ne  PGNSP PGUID 12 f f t f i 2 16 f "600 600" _null_ _null_ _null_ point_ne _null_ _null_ n 0));
DESCR("not equal");

/* point_vert(point, point) => bool */ 
DATA(insert OID = 989 ( point_vert  PGNSP PGUID 12 f f t f i 2 16 f "600 600" _null_ _null_ _null_ point_vert _null_ _null_ n 0));
DESCR("vertically aligned?");

/* point_horiz(point, point) => bool */ 
DATA(insert OID = 990 ( point_horiz  PGNSP PGUID 12 f f t f i 2 16 f "600 600" _null_ _null_ _null_ point_horiz _null_ _null_ n 0));
DESCR("horizontally aligned?");

/* point_distance(point, point) => float8 */ 
DATA(insert OID = 991 ( point_distance  PGNSP PGUID 12 f f t f i 2 701 f "600 600" _null_ _null_ _null_ point_distance _null_ _null_ n 0));
DESCR("distance between");

/* slope(point, point) => float8 */ 
DATA(insert OID = 992 ( slope  PGNSP PGUID 12 f f t f i 2 701 f "600 600" _null_ _null_ _null_ point_slope _null_ _null_ n 0));
DESCR("slope between points");

/* lseg(point, point) => lseg */ 
DATA(insert OID = 993 ( lseg  PGNSP PGUID 12 f f t f i 2 601 f "600 600" _null_ _null_ _null_ lseg_construct _null_ _null_ n 0));
DESCR("convert points to line segment");

/* lseg_intersect(lseg, lseg) => bool */ 
DATA(insert OID = 994 ( lseg_intersect  PGNSP PGUID 12 f f t f i 2 16 f "601 601" _null_ _null_ _null_ lseg_intersect _null_ _null_ n 0));
DESCR("intersect?");

/* lseg_parallel(lseg, lseg) => bool */ 
DATA(insert OID = 995 ( lseg_parallel  PGNSP PGUID 12 f f t f i 2 16 f "601 601" _null_ _null_ _null_ lseg_parallel _null_ _null_ n 0));
DESCR("parallel?");

/* lseg_perp(lseg, lseg) => bool */ 
DATA(insert OID = 996 ( lseg_perp  PGNSP PGUID 12 f f t f i 2 16 f "601 601" _null_ _null_ _null_ lseg_perp _null_ _null_ n 0));
DESCR("perpendicular?");

/* lseg_vertical(lseg) => bool */ 
DATA(insert OID = 997 ( lseg_vertical  PGNSP PGUID 12 f f t f i 1 16 f "601" _null_ _null_ _null_ lseg_vertical _null_ _null_ n 0));
DESCR("vertical?");

/* lseg_horizontal(lseg) => bool */ 
DATA(insert OID = 998 ( lseg_horizontal  PGNSP PGUID 12 f f t f i 1 16 f "601" _null_ _null_ _null_ lseg_horizontal _null_ _null_ n 0));
DESCR("horizontal?");

/* lseg_eq(lseg, lseg) => bool */ 
DATA(insert OID = 999 ( lseg_eq  PGNSP PGUID 12 f f t f i 2 16 f "601 601" _null_ _null_ _null_ lseg_eq _null_ _null_ n 0));
DESCR("equal");

/* date(text) => date */ 
DATA(insert OID = 748 ( date  PGNSP PGUID 12 f f t f s 1 1082 f "25" _null_ _null_ _null_ text_date _null_ _null_ n 0));
DESCR("convert text to date");

/* text(date) => text */ 
DATA(insert OID = 749 ( text  PGNSP PGUID 12 f f t f s 1 25 f "1082" _null_ _null_ _null_ date_text _null_ _null_ n 0));
DESCR("convert date to text");

/* "time"(text) => "time" */ 
DATA(insert OID = 837 ( time  PGNSP PGUID 12 f f t f s 1 1083 f "25" _null_ _null_ _null_ text_time _null_ _null_ n 0));
DESCR("convert text to time");

/* text("time") => text */ 
DATA(insert OID = 948 ( text  PGNSP PGUID 12 f f t f i 1 25 f "1083" _null_ _null_ _null_ time_text _null_ _null_ n 0));
DESCR("convert time to text");

/* timetz(text) => timetz */ 
DATA(insert OID = 938 ( timetz  PGNSP PGUID 12 f f t f s 1 1266 f "25" _null_ _null_ _null_ text_timetz _null_ _null_ n 0));
DESCR("convert text to timetz");

/* text(timetz) => text */ 
DATA(insert OID = 939 ( text  PGNSP PGUID 12 f f t f i 1 25 f "1266" _null_ _null_ _null_ timetz_text _null_ _null_ n 0));
DESCR("convert timetz to text");


/* OIDS 1000 - 1099 */
/* timezone("interval", timestamptz) => "timestamp" */ 
DATA(insert OID = 1026 ( timezone  PGNSP PGUID 12 f f t f i 2 1114 f "1186 1184" _null_ _null_ _null_ timestamptz_izone _null_ _null_ n 0));
DESCR("adjust timestamp to new time zone");

/* nullvalue("any") => bool */ 
DATA(insert OID = 1029 ( nullvalue  PGNSP PGUID 12 f f f f i 1 16 f "2276" _null_ _null_ _null_ nullvalue _null_ _null_ n 0));
DESCR("(internal)");

/* nonnullvalue("any") => bool */ 
DATA(insert OID = 1030 ( nonnullvalue  PGNSP PGUID 12 f f f f i 1 16 f "2276" _null_ _null_ _null_ nonnullvalue _null_ _null_ n 0));
DESCR("(internal)");

/* aclitemin(cstring) => aclitem */ 
DATA(insert OID = 1031 ( aclitemin  PGNSP PGUID 12 f f t f s 1 1033 f "2275" _null_ _null_ _null_ aclitemin _null_ _null_ n 0));
DESCR("I/O");

/* aclitemout(aclitem) => cstring */ 
DATA(insert OID = 1032 ( aclitemout  PGNSP PGUID 12 f f t f s 1 2275 f "1033" _null_ _null_ _null_ aclitemout _null_ _null_ n 0));
DESCR("I/O");

/* aclinsert(_aclitem, aclitem) => _aclitem */ 
DATA(insert OID = 1035 ( aclinsert  PGNSP PGUID 12 f f t f i 2 1034 f "1034 1033" _null_ _null_ _null_ aclinsert _null_ _null_ n 0));
DESCR("add/update ACL item");

/* aclremove(_aclitem, aclitem) => _aclitem */ 
DATA(insert OID = 1036 ( aclremove  PGNSP PGUID 12 f f t f i 2 1034 f "1034 1033" _null_ _null_ _null_ aclremove _null_ _null_ n 0));
DESCR("remove ACL item");

/* aclcontains(_aclitem, aclitem) => bool */ 
DATA(insert OID = 1037 ( aclcontains  PGNSP PGUID 12 f f t f i 2 16 f "1034 1033" _null_ _null_ _null_ aclcontains _null_ _null_ n 0));
DESCR("ACL contains item?");

/* aclitemeq(aclitem, aclitem) => bool */ 
DATA(insert OID = 1062 ( aclitemeq  PGNSP PGUID 12 f f t f i 2 16 f "1033 1033" _null_ _null_ _null_ aclitem_eq _null_ _null_ n 0));
DESCR("equality operator for ACL items");

/* makeaclitem(oid, oid, text, bool) => aclitem */ 
DATA(insert OID = 1365 ( makeaclitem  PGNSP PGUID 12 f f t f i 4 1033 f "26 26 25 16" _null_ _null_ _null_ makeaclitem _null_ _null_ n 0));
DESCR("make ACL item");

/* bpcharin(cstring, oid, int4) => bpchar */ 
DATA(insert OID = 1044 ( bpcharin  PGNSP PGUID 12 f f t f i 3 1042 f "2275 26 23" _null_ _null_ _null_ bpcharin _null_ _null_ n 0));
DESCR("I/O");

/* bpcharout(bpchar) => cstring */ 
DATA(insert OID = 1045 ( bpcharout  PGNSP PGUID 12 f f t f i 1 2275 f "1042" _null_ _null_ _null_ bpcharout _null_ _null_ n 0));
DESCR("I/O");

/* varcharin(cstring, oid, int4) => "varchar" */ 
DATA(insert OID = 1046 ( varcharin  PGNSP PGUID 12 f f t f i 3 1043 f "2275 26 23" _null_ _null_ _null_ varcharin _null_ _null_ n 0));
DESCR("I/O");

/* varcharout("varchar") => cstring */ 
DATA(insert OID = 1047 ( varcharout  PGNSP PGUID 12 f f t f i 1 2275 f "1043" _null_ _null_ _null_ varcharout _null_ _null_ n 0));
DESCR("I/O");

/* bpchareq(bpchar, bpchar) => bool */ 
DATA(insert OID = 1048 ( bpchareq  PGNSP PGUID 12 f f t f i 2 16 f "1042 1042" _null_ _null_ _null_ bpchareq _null_ _null_ n 0));
DESCR("equal");

/* bpcharlt(bpchar, bpchar) => bool */ 
DATA(insert OID = 1049 ( bpcharlt  PGNSP PGUID 12 f f t f i 2 16 f "1042 1042" _null_ _null_ _null_ bpcharlt _null_ _null_ n 0));
DESCR("less-than");

/* bpcharle(bpchar, bpchar) => bool */ 
DATA(insert OID = 1050 ( bpcharle  PGNSP PGUID 12 f f t f i 2 16 f "1042 1042" _null_ _null_ _null_ bpcharle _null_ _null_ n 0));
DESCR("less-than-or-equal");

/* bpchargt(bpchar, bpchar) => bool */ 
DATA(insert OID = 1051 ( bpchargt  PGNSP PGUID 12 f f t f i 2 16 f "1042 1042" _null_ _null_ _null_ bpchargt _null_ _null_ n 0));
DESCR("greater-than");

/* bpcharge(bpchar, bpchar) => bool */ 
DATA(insert OID = 1052 ( bpcharge  PGNSP PGUID 12 f f t f i 2 16 f "1042 1042" _null_ _null_ _null_ bpcharge _null_ _null_ n 0));
DESCR("greater-than-or-equal");

/* bpcharne(bpchar, bpchar) => bool */ 
DATA(insert OID = 1053 ( bpcharne  PGNSP PGUID 12 f f t f i 2 16 f "1042 1042" _null_ _null_ _null_ bpcharne _null_ _null_ n 0));
DESCR("not equal");

/* bpchar_larger(bpchar, bpchar) => bpchar */ 
DATA(insert OID = 1063 ( bpchar_larger  PGNSP PGUID 12 f f t f i 2 1042 f "1042 1042" _null_ _null_ _null_ bpchar_larger _null_ _null_ n 0));
DESCR("larger of two");

/* bpchar_smaller(bpchar, bpchar) => bpchar */ 
DATA(insert OID = 1064 ( bpchar_smaller  PGNSP PGUID 12 f f t f i 2 1042 f "1042 1042" _null_ _null_ _null_ bpchar_smaller _null_ _null_ n 0));
DESCR("smaller of two");

/* bpcharcmp(bpchar, bpchar) => int4 */ 
DATA(insert OID = 1078 ( bpcharcmp  PGNSP PGUID 12 f f t f i 2 23 f "1042 1042" _null_ _null_ _null_ bpcharcmp _null_ _null_ n 0));
DESCR("less-equal-greater");

/* hashbpchar(bpchar) => int4 */ 
DATA(insert OID = 1080 ( hashbpchar  PGNSP PGUID 12 f f t f i 1 23 f "1042" _null_ _null_ _null_ hashbpchar _null_ _null_ n 0));
DESCR("hash");

/* format_type(oid, int4) => text */ 
DATA(insert OID = 1081 ( format_type  PGNSP PGUID 12 f f f f s 2 25 f "26 23" _null_ _null_ _null_ format_type _null_ _null_ n 0));
DESCR("format a type oid and atttypmod to canonical SQL");

/* date_in(cstring) => date */ 
DATA(insert OID = 1084 ( date_in  PGNSP PGUID 12 f f t f s 1 1082 f "2275" _null_ _null_ _null_ date_in _null_ _null_ n 0));
DESCR("I/O");

/* date_out(date) => cstring */ 
DATA(insert OID = 1085 ( date_out  PGNSP PGUID 12 f f t f s 1 2275 f "1082" _null_ _null_ _null_ date_out _null_ _null_ n 0));
DESCR("I/O");

/* date_eq(date, date) => bool */ 
DATA(insert OID = 1086 ( date_eq  PGNSP PGUID 12 f f t f i 2 16 f "1082 1082" _null_ _null_ _null_ date_eq _null_ _null_ n 0));
DESCR("equal");

/* date_lt(date, date) => bool */ 
DATA(insert OID = 1087 ( date_lt  PGNSP PGUID 12 f f t f i 2 16 f "1082 1082" _null_ _null_ _null_ date_lt _null_ _null_ n 0));
DESCR("less-than");

/* date_le(date, date) => bool */ 
DATA(insert OID = 1088 ( date_le  PGNSP PGUID 12 f f t f i 2 16 f "1082 1082" _null_ _null_ _null_ date_le _null_ _null_ n 0));
DESCR("less-than-or-equal");

/* date_gt(date, date) => bool */ 
DATA(insert OID = 1089 ( date_gt  PGNSP PGUID 12 f f t f i 2 16 f "1082 1082" _null_ _null_ _null_ date_gt _null_ _null_ n 0));
DESCR("greater-than");

/* date_ge(date, date) => bool */ 
DATA(insert OID = 1090 ( date_ge  PGNSP PGUID 12 f f t f i 2 16 f "1082 1082" _null_ _null_ _null_ date_ge _null_ _null_ n 0));
DESCR("greater-than-or-equal");

/* date_ne(date, date) => bool */ 
DATA(insert OID = 1091 ( date_ne  PGNSP PGUID 12 f f t f i 2 16 f "1082 1082" _null_ _null_ _null_ date_ne _null_ _null_ n 0));
DESCR("not equal");

/* date_cmp(date, date) => int4 */ 
DATA(insert OID = 1092 ( date_cmp  PGNSP PGUID 12 f f t f i 2 23 f "1082 1082" _null_ _null_ _null_ date_cmp _null_ _null_ n 0));
DESCR("less-equal-greater");


/* OIDS 1100 - 1199 */
/* time_lt("time", "time") => bool */ 
DATA(insert OID = 1102 ( time_lt  PGNSP PGUID 12 f f t f i 2 16 f "1083 1083" _null_ _null_ _null_ time_lt _null_ _null_ n 0));
DESCR("less-than");

/* time_le("time", "time") => bool */ 
DATA(insert OID = 1103 ( time_le  PGNSP PGUID 12 f f t f i 2 16 f "1083 1083" _null_ _null_ _null_ time_le _null_ _null_ n 0));
DESCR("less-than-or-equal");

/* time_gt("time", "time") => bool */ 
DATA(insert OID = 1104 ( time_gt  PGNSP PGUID 12 f f t f i 2 16 f "1083 1083" _null_ _null_ _null_ time_gt _null_ _null_ n 0));
DESCR("greater-than");

/* time_ge("time", "time") => bool */ 
DATA(insert OID = 1105 ( time_ge  PGNSP PGUID 12 f f t f i 2 16 f "1083 1083" _null_ _null_ _null_ time_ge _null_ _null_ n 0));
DESCR("greater-than-or-equal");

/* time_ne("time", "time") => bool */ 
DATA(insert OID = 1106 ( time_ne  PGNSP PGUID 12 f f t f i 2 16 f "1083 1083" _null_ _null_ _null_ time_ne _null_ _null_ n 0));
DESCR("not equal");

/* time_cmp("time", "time") => int4 */ 
DATA(insert OID = 1107 ( time_cmp  PGNSP PGUID 12 f f t f i 2 23 f "1083 1083" _null_ _null_ _null_ time_cmp _null_ _null_ n 0));
DESCR("less-equal-greater");

/* date_larger(date, date) => date */ 
DATA(insert OID = 1138 ( date_larger  PGNSP PGUID 12 f f t f i 2 1082 f "1082 1082" _null_ _null_ _null_ date_larger _null_ _null_ n 0));
DESCR("larger of two");

/* date_smaller(date, date) => date */ 
DATA(insert OID = 1139 ( date_smaller  PGNSP PGUID 12 f f t f i 2 1082 f "1082 1082" _null_ _null_ _null_ date_smaller _null_ _null_ n 0));
DESCR("smaller of two");

/* date_mi(date, date) => int4 */ 
DATA(insert OID = 1140 ( date_mi  PGNSP PGUID 12 f f t f i 2 23 f "1082 1082" _null_ _null_ _null_ date_mi _null_ _null_ n 0));
DESCR("subtract");

/* date_pli(date, int4) => date */ 
DATA(insert OID = 1141 ( date_pli  PGNSP PGUID 12 f f t f i 2 1082 f "1082 23" _null_ _null_ _null_ date_pli _null_ _null_ n 0));
DESCR("add");

/* date_mii(date, int4) => date */ 
DATA(insert OID = 1142 ( date_mii  PGNSP PGUID 12 f f t f i 2 1082 f "1082 23" _null_ _null_ _null_ date_mii _null_ _null_ n 0));
DESCR("subtract");

/* time_in(cstring, oid, int4) => "time" */ 
DATA(insert OID = 1143 ( time_in  PGNSP PGUID 12 f f t f s 3 1083 f "2275 26 23" _null_ _null_ _null_ time_in _null_ _null_ n 0));
DESCR("I/O");

/* time_out("time") => cstring */ 
DATA(insert OID = 1144 ( time_out  PGNSP PGUID 12 f f t f i 1 2275 f "1083" _null_ _null_ _null_ time_out _null_ _null_ n 0));
DESCR("I/O");

/* time_eq("time", "time") => bool */ 
DATA(insert OID = 1145 ( time_eq  PGNSP PGUID 12 f f t f i 2 16 f "1083 1083" _null_ _null_ _null_ time_eq _null_ _null_ n 0));
DESCR("equal");

/* circle_add_pt(circle, point) => circle */ 
DATA(insert OID = 1146 ( circle_add_pt  PGNSP PGUID 12 f f t f i 2 718 f "718 600" _null_ _null_ _null_ circle_add_pt _null_ _null_ n 0));
DESCR("add");

/* circle_sub_pt(circle, point) => circle */ 
DATA(insert OID = 1147 ( circle_sub_pt  PGNSP PGUID 12 f f t f i 2 718 f "718 600" _null_ _null_ _null_ circle_sub_pt _null_ _null_ n 0));
DESCR("subtract");

/* circle_mul_pt(circle, point) => circle */ 
DATA(insert OID = 1148 ( circle_mul_pt  PGNSP PGUID 12 f f t f i 2 718 f "718 600" _null_ _null_ _null_ circle_mul_pt _null_ _null_ n 0));
DESCR("multiply");

/* circle_div_pt(circle, point) => circle */ 
DATA(insert OID = 1149 ( circle_div_pt  PGNSP PGUID 12 f f t f i 2 718 f "718 600" _null_ _null_ _null_ circle_div_pt _null_ _null_ n 0));
DESCR("divide");

/* timestamptz_in(cstring, oid, int4) => timestamptz */ 
DATA(insert OID = 1150 ( timestamptz_in  PGNSP PGUID 12 f f t f s 3 1184 f "2275 26 23" _null_ _null_ _null_ timestamptz_in _null_ _null_ n 0));
DESCR("I/O");

/* timestamptz_out(timestamptz) => cstring */ 
DATA(insert OID = 1151 ( timestamptz_out  PGNSP PGUID 12 f f t f s 1 2275 f "1184" _null_ _null_ _null_ timestamptz_out _null_ _null_ n 0));
DESCR("I/O");

/* timestamptz_eq(timestamptz, timestamptz) => bool */ 
DATA(insert OID = 1152 ( timestamptz_eq  PGNSP PGUID 12 f f t f i 2 16 f "1184 1184" _null_ _null_ _null_ timestamp_eq _null_ _null_ n 0));
DESCR("equal");

/* timestamptz_ne(timestamptz, timestamptz) => bool */ 
DATA(insert OID = 1153 ( timestamptz_ne  PGNSP PGUID 12 f f t f i 2 16 f "1184 1184" _null_ _null_ _null_ timestamp_ne _null_ _null_ n 0));
DESCR("not equal");

/* timestamptz_lt(timestamptz, timestamptz) => bool */ 
DATA(insert OID = 1154 ( timestamptz_lt  PGNSP PGUID 12 f f t f i 2 16 f "1184 1184" _null_ _null_ _null_ timestamp_lt _null_ _null_ n 0));
DESCR("less-than");

/* timestamptz_le(timestamptz, timestamptz) => bool */ 
DATA(insert OID = 1155 ( timestamptz_le  PGNSP PGUID 12 f f t f i 2 16 f "1184 1184" _null_ _null_ _null_ timestamp_le _null_ _null_ n 0));
DESCR("less-than-or-equal");

/* timestamptz_ge(timestamptz, timestamptz) => bool */ 
DATA(insert OID = 1156 ( timestamptz_ge  PGNSP PGUID 12 f f t f i 2 16 f "1184 1184" _null_ _null_ _null_ timestamp_ge _null_ _null_ n 0));
DESCR("greater-than-or-equal");

/* timestamptz_gt(timestamptz, timestamptz) => bool */ 
DATA(insert OID = 1157 ( timestamptz_gt  PGNSP PGUID 12 f f t f i 2 16 f "1184 1184" _null_ _null_ _null_ timestamp_gt _null_ _null_ n 0));
DESCR("greater-than");

/* to_timestamp(float8) => pg_catalog.timestamptz */ 
DATA(insert OID = 1158 ( to_timestamp  PGNSP PGUID 14 f f t f i 1 1184 f "701" _null_ _null_ _null_ "select (''epoch''::timestamptz + $1 * ''1 second''::interval)" _null_ _null_ c 0));
DESCR("convert UNIX epoch to timestamptz");

/* timezone(text, timestamptz) => "timestamp" */ 
DATA(insert OID = 1159 ( timezone  PGNSP PGUID 12 f f t f i 2 1114 f "25 1184" _null_ _null_ _null_ timestamptz_zone _null_ _null_ n 0));
DESCR("adjust timestamp to new time zone");

/* interval_in(cstring, oid, int4) => "interval" */ 
DATA(insert OID = 1160 ( interval_in  PGNSP PGUID 12 f f t f s 3 1186 f "2275 26 23" _null_ _null_ _null_ interval_in _null_ _null_ n 0));
DESCR("I/O");

/* interval_out("interval") => cstring */ 
DATA(insert OID = 1161 ( interval_out  PGNSP PGUID 12 f f t f i 1 2275 f "1186" _null_ _null_ _null_ interval_out _null_ _null_ n 0));
DESCR("I/O");

/* interval_eq("interval", "interval") => bool */ 
DATA(insert OID = 1162 ( interval_eq  PGNSP PGUID 12 f f t f i 2 16 f "1186 1186" _null_ _null_ _null_ interval_eq _null_ _null_ n 0));
DESCR("equal");

/* interval_ne("interval", "interval") => bool */ 
DATA(insert OID = 1163 ( interval_ne  PGNSP PGUID 12 f f t f i 2 16 f "1186 1186" _null_ _null_ _null_ interval_ne _null_ _null_ n 0));
DESCR("not equal");

/* interval_lt("interval", "interval") => bool */ 
DATA(insert OID = 1164 ( interval_lt  PGNSP PGUID 12 f f t f i 2 16 f "1186 1186" _null_ _null_ _null_ interval_lt _null_ _null_ n 0));
DESCR("less-than");

/* interval_le("interval", "interval") => bool */ 
DATA(insert OID = 1165 ( interval_le  PGNSP PGUID 12 f f t f i 2 16 f "1186 1186" _null_ _null_ _null_ interval_le _null_ _null_ n 0));
DESCR("less-than-or-equal");

/* interval_ge("interval", "interval") => bool */ 
DATA(insert OID = 1166 ( interval_ge  PGNSP PGUID 12 f f t f i 2 16 f "1186 1186" _null_ _null_ _null_ interval_ge _null_ _null_ n 0));
DESCR("greater-than-or-equal");

/* interval_gt("interval", "interval") => bool */ 
DATA(insert OID = 1167 ( interval_gt  PGNSP PGUID 12 f f t f i 2 16 f "1186 1186" _null_ _null_ _null_ interval_gt _null_ _null_ n 0));
DESCR("greater-than");

/* interval_um("interval") => "interval" */ 
DATA(insert OID = 1168 ( interval_um  PGNSP PGUID 12 f f t f i 1 1186 f "1186" _null_ _null_ _null_ interval_um _null_ _null_ n 0));
DESCR("subtract");

/* interval_pl("interval", "interval") => "interval" */ 
DATA(insert OID = 1169 ( interval_pl  PGNSP PGUID 12 f f t f i 2 1186 f "1186 1186" _null_ _null_ _null_ interval_pl _null_ _null_ n 0));
DESCR("add");

/* interval_mi("interval", "interval") => "interval" */ 
DATA(insert OID = 1170 ( interval_mi  PGNSP PGUID 12 f f t f i 2 1186 f "1186 1186" _null_ _null_ _null_ interval_mi _null_ _null_ n 0));
DESCR("subtract");

/* date_part(text, timestamptz) => float8 */ 
DATA(insert OID = 1171 ( date_part  PGNSP PGUID 12 f f t f s 2 701 f "25 1184" _null_ _null_ _null_ timestamptz_part _null_ _null_ n 0));
DESCR("extract field from timestamp with time zone");

/* date_part(text, "interval") => float8 */ 
DATA(insert OID = 1172 ( date_part  PGNSP PGUID 12 f f t f i 2 701 f "25 1186" _null_ _null_ _null_ interval_part _null_ _null_ n 0));
DESCR("extract field from interval");

/* timestamptz(abstime) => timestamptz */ 
DATA(insert OID = 1173 ( timestamptz  PGNSP PGUID 12 f f t f i 1 1184 f "702" _null_ _null_ _null_ abstime_timestamptz _null_ _null_ n 0));
DESCR("convert abstime to timestamp with time zone");

/* timestamptz(date) => timestamptz */ 
DATA(insert OID = 1174 ( timestamptz  PGNSP PGUID 12 f f t f s 1 1184 f "1082" _null_ _null_ _null_ date_timestamptz _null_ _null_ n 0));
DESCR("convert date to timestamp with time zone");

/* justify_interval("interval") => "interval" */ 
DATA(insert OID = 2711 ( justify_interval  PGNSP PGUID 12 f f t f i 1 1186 f "1186" _null_ _null_ _null_ interval_justify_interval _null_ _null_ n 0));
DESCR("promote groups of 24 hours to numbers of days and promote groups of 30 days to numbers of months");

/* justify_hours("interval") => "interval" */ 
DATA(insert OID = 1175 ( justify_hours  PGNSP PGUID 12 f f t f i 1 1186 f "1186" _null_ _null_ _null_ interval_justify_hours _null_ _null_ n 0));
DESCR("promote groups of 24 hours to numbers of days");

/* justify_days("interval") => "interval" */ 
DATA(insert OID = 1295 ( justify_days  PGNSP PGUID 12 f f t f i 1 1186 f "1186" _null_ _null_ _null_ interval_justify_days _null_ _null_ n 0));
DESCR("promote groups of 30 days to numbers of months");

/* timestamptz(date, "time") => pg_catalog.timestamptz */ 
DATA(insert OID = 1176 ( timestamptz  PGNSP PGUID 14 f f t f s 2 1184 f "1082 1083" _null_ _null_ _null_ "select cast(($1 + $2) as timestamp with time zone)" _null_ _null_ c 0));
DESCR("convert date and time to timestamp with time zone");

/* "interval"(reltime) => "interval" */ 
DATA(insert OID = 1177 ( interval  PGNSP PGUID 12 f f t f i 1 1186 f "703" _null_ _null_ _null_ reltime_interval _null_ _null_ n 0));
DESCR("convert reltime to interval");

/* date(timestamptz) => date */ 
DATA(insert OID = 1178 ( date  PGNSP PGUID 12 f f t f s 1 1082 f "1184" _null_ _null_ _null_ timestamptz_date _null_ _null_ n 0));
DESCR("convert timestamp with time zone to date");

/* date(abstime) => date */ 
DATA(insert OID = 1179 ( date  PGNSP PGUID 12 f f t f s 1 1082 f "702" _null_ _null_ _null_ abstime_date _null_ _null_ n 0));
DESCR("convert abstime to date");

/* abstime(timestamptz) => abstime */ 
DATA(insert OID = 1180 ( abstime  PGNSP PGUID 12 f f t f i 1 702 f "1184" _null_ _null_ _null_ timestamptz_abstime _null_ _null_ n 0));
DESCR("convert timestamp with time zone to abstime");

/* age(xid) => int4 */ 
DATA(insert OID = 1181 ( age  PGNSP PGUID 12 f f t f s 1 23 f "28" _null_ _null_ _null_ xid_age _null_ _null_ n 0));
DESCR("age of a transaction ID, in transactions before current transaction");

/* timestamptz_mi(timestamptz, timestamptz) => "interval" */ 
DATA(insert OID = 1188 ( timestamptz_mi  PGNSP PGUID 12 f f t f i 2 1186 f "1184 1184" _null_ _null_ _null_ timestamp_mi _null_ _null_ n 0));
DESCR("subtract");

/* timestamptz_pl_interval(timestamptz, "interval") => timestamptz */ 
DATA(insert OID = 1189 ( timestamptz_pl_interval  PGNSP PGUID 12 f f t f s 2 1184 f "1184 1186" _null_ _null_ _null_ timestamptz_pl_interval _null_ _null_ n 0));
DESCR("plus");

/* timestamptz_mi_interval(timestamptz, "interval") => timestamptz */ 
DATA(insert OID = 1190 ( timestamptz_mi_interval  PGNSP PGUID 12 f f t f s 2 1184 f "1184 1186" _null_ _null_ _null_ timestamptz_mi_interval _null_ _null_ n 0));
DESCR("minus");

/* timestamptz(text) => timestamptz */ 
DATA(insert OID = 1191 ( timestamptz  PGNSP PGUID 12 f f t f s 1 1184 f "25" _null_ _null_ _null_ text_timestamptz _null_ _null_ n 0));
DESCR("convert text to timestamp with time zone");

/* text(timestamptz) => text */ 
DATA(insert OID = 1192 ( text  PGNSP PGUID 12 f f t f s 1 25 f "1184" _null_ _null_ _null_ timestamptz_text _null_ _null_ n 0));
DESCR("convert timestamp with time zone to text");

/* text("interval") => text */ 
DATA(insert OID = 1193 ( text  PGNSP PGUID 12 f f t f i 1 25 f "1186" _null_ _null_ _null_ interval_text _null_ _null_ n 0));
DESCR("convert interval to text");

/* reltime("interval") => reltime */ 
DATA(insert OID = 1194 ( reltime  PGNSP PGUID 12 f f t f i 1 703 f "1186" _null_ _null_ _null_ interval_reltime _null_ _null_ n 0));
DESCR("convert interval to reltime");

/* timestamptz_smaller(timestamptz, timestamptz) => timestamptz */ 
DATA(insert OID = 1195 ( timestamptz_smaller  PGNSP PGUID 12 f f t f i 2 1184 f "1184 1184" _null_ _null_ _null_ timestamp_smaller _null_ _null_ n 0));
DESCR("smaller of two");

/* timestamptz_larger(timestamptz, timestamptz) => timestamptz */ 
DATA(insert OID = 1196 ( timestamptz_larger  PGNSP PGUID 12 f f t f i 2 1184 f "1184 1184" _null_ _null_ _null_ timestamp_larger _null_ _null_ n 0));
DESCR("larger of two");

/* interval_smaller("interval", "interval") => "interval" */ 
DATA(insert OID = 1197 ( interval_smaller  PGNSP PGUID 12 f f t f i 2 1186 f "1186 1186" _null_ _null_ _null_ interval_smaller _null_ _null_ n 0));
DESCR("smaller of two");

/* interval_larger("interval", "interval") => "interval" */ 
DATA(insert OID = 1198 ( interval_larger  PGNSP PGUID 12 f f t f i 2 1186 f "1186 1186" _null_ _null_ _null_ interval_larger _null_ _null_ n 0));
DESCR("larger of two");

/* age(timestamptz, timestamptz) => "interval" */ 
DATA(insert OID = 1199 ( age  PGNSP PGUID 12 f f t f i 2 1186 f "1184 1184" _null_ _null_ _null_ timestamptz_age _null_ _null_ n 0));
DESCR("date difference preserving months and years");


/* OIDS 1200 - 1299 */
/* "interval"("interval", int4) => "interval" */ 
DATA(insert OID = 1200 ( interval  PGNSP PGUID 12 f f t f i 2 1186 f "1186 23" _null_ _null_ _null_ interval_scale _null_ _null_ n 0));
DESCR("adjust interval precision");

/* obj_description(oid, name) => pg_catalog.text */ 
DATA(insert OID = 1215 ( obj_description  PGNSP PGUID 14 f f t f s 2 25 f "26 19" _null_ _null_ _null_ "select description from pg_catalog.pg_description where objoid = $1 and classoid = (select oid from pg_catalog.pg_class where relname = $2 and relnamespace = PGNSP) and objsubid = 0" _null_ _null_ c 0));
DESCR("get description for object id and catalog name");

/* col_description(oid, int4) => pg_catalog.text */ 
DATA(insert OID = 1216 ( col_description  PGNSP PGUID 14 f f t f s 2 25 f "26 23" _null_ _null_ _null_ "select description from pg_catalog.pg_description where objoid = $1 and classoid = ''pg_catalog.pg_class''::regclass and objsubid = $2" _null_ _null_ c 0));
DESCR("get description for table column");

/* shobj_description(oid, name) => pg_catalog.text */ 
DATA(insert OID = 1993 ( shobj_description  PGNSP PGUID 14 f f t f s 2 25 f "26 19" _null_ _null_ _null_ "select description from pg_catalog.pg_shdescription where objoid = $1 and classoid = (select oid from pg_catalog.pg_class where relname = $2 and relnamespace = PGNSP)" _null_ _null_ c 0));
DESCR("get description for object id and shared catalog name");

/* date_trunc(text, timestamptz) => timestamptz */ 
DATA(insert OID = 1217 ( date_trunc  PGNSP PGUID 12 f f t f s 2 1184 f "25 1184" _null_ _null_ _null_ timestamptz_trunc _null_ _null_ n 0));
DESCR("truncate timestamp with time zone to specified units");

/* date_trunc(text, "interval") => "interval" */ 
DATA(insert OID = 1218 ( date_trunc  PGNSP PGUID 12 f f t f i 2 1186 f "25 1186" _null_ _null_ _null_ interval_trunc _null_ _null_ n 0));
DESCR("truncate interval to specified units");

/* int8inc(int8) => int8 */ 
DATA(insert OID = 1219 ( int8inc  PGNSP PGUID 12 f f t f i 1 20 f "20" _null_ _null_ _null_ int8inc _null_ _null_ n 0));
DESCR("increment");

/* int8dec(int8) => int8 */ 
DATA(insert OID = 2857 ( int8dec  PGNSP PGUID 12 f f t f i 1 20 f "20" _null_ _null_ _null_ int8dec _null_ _null_ n 0));

/* int8inc_any(int8, "any") => int8 */ 
DATA(insert OID = 2804 ( int8inc_any  PGNSP PGUID 12 f f t f i 2 20 f "20 2276" _null_ _null_ _null_ int8inc_any _null_ _null_ n 0));
DESCR("increment, ignores second argument");

/* int8abs(int8) => int8 */ 
DATA(insert OID = 1230 ( int8abs  PGNSP PGUID 12 f f t f i 1 20 f "20" _null_ _null_ _null_ int8abs _null_ _null_ n 0));
DESCR("absolute value");

/* int8larger(int8, int8) => int8 */ 
DATA(insert OID = 1236 ( int8larger  PGNSP PGUID 12 f f t f i 2 20 f "20 20" _null_ _null_ _null_ int8larger _null_ _null_ n 0));
DESCR("larger of two");

/* int8smaller(int8, int8) => int8 */ 
DATA(insert OID = 1237 ( int8smaller  PGNSP PGUID 12 f f t f i 2 20 f "20 20" _null_ _null_ _null_ int8smaller _null_ _null_ n 0));
DESCR("smaller of two");

/* texticregexeq(text, text) => bool */ 
DATA(insert OID = 1238 ( texticregexeq  PGNSP PGUID 12 f f t f i 2 16 f "25 25" _null_ _null_ _null_ texticregexeq _null_ _null_ n 0));
DESCR("matches regex., case-insensitive");

/* texticregexne(text, text) => bool */ 
DATA(insert OID = 1239 ( texticregexne  PGNSP PGUID 12 f f t f i 2 16 f "25 25" _null_ _null_ _null_ texticregexne _null_ _null_ n 0));
DESCR("does not match regex., case-insensitive");

/* nameicregexeq(name, text) => bool */ 
DATA(insert OID = 1240 ( nameicregexeq  PGNSP PGUID 12 f f t f i 2 16 f "19 25" _null_ _null_ _null_ nameicregexeq _null_ _null_ n 0));
DESCR("matches regex., case-insensitive");

/* nameicregexne(name, text) => bool */ 
DATA(insert OID = 1241 ( nameicregexne  PGNSP PGUID 12 f f t f i 2 16 f "19 25" _null_ _null_ _null_ nameicregexne _null_ _null_ n 0));
DESCR("does not match regex., case-insensitive");

/* int4abs(int4) => int4 */ 
DATA(insert OID = 1251 ( int4abs  PGNSP PGUID 12 f f t f i 1 23 f "23" _null_ _null_ _null_ int4abs _null_ _null_ n 0));
DESCR("absolute value");

/* int2abs(int2) => int2 */ 
DATA(insert OID = 1253 ( int2abs  PGNSP PGUID 12 f f t f i 1 21 f "21" _null_ _null_ _null_ int2abs _null_ _null_ n 0));
DESCR("absolute value");

/* "interval"(text) => "interval" */ 
DATA(insert OID = 1263 ( interval  PGNSP PGUID 12 f f t f s 1 1186 f "25" _null_ _null_ _null_ text_interval _null_ _null_ n 0));
DESCR("convert text to interval");

/* "overlaps"(timetz, timetz, timetz, timetz) => bool */ 
DATA(insert OID = 1271 ( overlaps  PGNSP PGUID 12 f f f f i 4 16 f "1266 1266 1266 1266" _null_ _null_ _null_ overlaps_timetz _null_ _null_ n 0));
DESCR("SQL92 interval comparison");

/* datetime_pl(date, "time") => "timestamp" */ 
DATA(insert OID = 1272 ( datetime_pl  PGNSP PGUID 12 f f t f i 2 1114 f "1082 1083" _null_ _null_ _null_ datetime_timestamp _null_ _null_ n 0));
DESCR("convert date and time to timestamp");

/* date_part(text, timetz) => float8 */ 
DATA(insert OID = 1273 ( date_part  PGNSP PGUID 12 f f t f i 2 701 f "25 1266" _null_ _null_ _null_ timetz_part _null_ _null_ n 0));
DESCR("extract field from time with time zone");

/* int84pl(int8, int4) => int8 */ 
DATA(insert OID = 1274 ( int84pl  PGNSP PGUID 12 f f t f i 2 20 f "20 23" _null_ _null_ _null_ int84pl _null_ _null_ n 0));
DESCR("add");

/* int84mi(int8, int4) => int8 */ 
DATA(insert OID = 1275 ( int84mi  PGNSP PGUID 12 f f t f i 2 20 f "20 23" _null_ _null_ _null_ int84mi _null_ _null_ n 0));
DESCR("subtract");

/* int84mul(int8, int4) => int8 */ 
DATA(insert OID = 1276 ( int84mul  PGNSP PGUID 12 f f t f i 2 20 f "20 23" _null_ _null_ _null_ int84mul _null_ _null_ n 0));
DESCR("multiply");

/* int84div(int8, int4) => int8 */ 
DATA(insert OID = 1277 ( int84div  PGNSP PGUID 12 f f t f i 2 20 f "20 23" _null_ _null_ _null_ int84div _null_ _null_ n 0));
DESCR("divide");

/* int48pl(int4, int8) => int8 */ 
DATA(insert OID = 1278 ( int48pl  PGNSP PGUID 12 f f t f i 2 20 f "23 20" _null_ _null_ _null_ int48pl _null_ _null_ n 0));
DESCR("add");

/* int48mi(int4, int8) => int8 */ 
DATA(insert OID = 1279 ( int48mi  PGNSP PGUID 12 f f t f i 2 20 f "23 20" _null_ _null_ _null_ int48mi _null_ _null_ n 0));
DESCR("subtract");

/* int48mul(int4, int8) => int8 */ 
DATA(insert OID = 1280 ( int48mul  PGNSP PGUID 12 f f t f i 2 20 f "23 20" _null_ _null_ _null_ int48mul _null_ _null_ n 0));
DESCR("multiply");

/* int48div(int4, int8) => int8 */ 
DATA(insert OID = 1281 ( int48div  PGNSP PGUID 12 f f t f i 2 20 f "23 20" _null_ _null_ _null_ int48div _null_ _null_ n 0));
DESCR("divide");

/* oid(int8) => oid */ 
DATA(insert OID = 1287 ( oid  PGNSP PGUID 12 f f t f i 1 26 f "20" _null_ _null_ _null_ i8tooid _null_ _null_ n 0));
DESCR("convert int8 to oid");

/* int8(oid) => int8 */ 
DATA(insert OID = 1288 ( int8  PGNSP PGUID 12 f f t f i 1 20 f "26" _null_ _null_ _null_ oidtoi8 _null_ _null_ n 0));
DESCR("convert oid to int8");

/* text(int8) => text */ 
DATA(insert OID = 1289 ( text  PGNSP PGUID 12 f f t f i 1 25 f "20" _null_ _null_ _null_ int8_text _null_ _null_ n 0));
DESCR("convert int8 to text");

/* int8(text) => int8 */ 
DATA(insert OID = 1290 ( int8  PGNSP PGUID 12 f f t f i 1 20 f "25" _null_ _null_ _null_ text_int8 _null_ _null_ n 0));
DESCR("convert text to int8");

/* array_length_coerce(anyarray, int4, bool) => anyarray */ 
DATA(insert OID = 1291 ( array_length_coerce  PGNSP PGUID 12 f f t f s 3 2277 f "2277 23 16" _null_ _null_ _null_ array_length_coerce _null_ _null_ n 0));
DESCR("adjust any array to new element typmod");

/* tideq(tid, tid) => bool */ 
DATA(insert OID = 1292 ( tideq  PGNSP PGUID 12 f f t f i 2 16 f "27 27" _null_ _null_ _null_ tideq _null_ _null_ n 0));
DESCR("equal");

/* currtid(oid, tid) => tid */ 
DATA(insert OID = 1293 ( currtid  PGNSP PGUID 12 f f t f v 2 27 f "26 27" _null_ _null_ _null_ currtid_byreloid _null_ _null_ n 0));
DESCR("latest tid of a tuple");

/* currtid2(text, tid) => tid */ 
DATA(insert OID = 1294 ( currtid2  PGNSP PGUID 12 f f t f v 2 27 f "25 27" _null_ _null_ _null_ currtid_byrelname _null_ _null_ n 0));
DESCR("latest tid of a tuple");

/* tidne(tid, tid) => bool */ 
DATA(insert OID = 1265 ( tidne  PGNSP PGUID 12 f f t f i 2 16 f "27 27" _null_ _null_ _null_ tidne _null_ _null_ n 0));
DESCR("not equal");

/* tidgt(tid, tid) => bool */ 
DATA(insert OID = 2790 ( tidgt  PGNSP PGUID 12 f f t f i 2 16 f "27 27" _null_ _null_ _null_ tidgt _null_ _null_ n 0));
DESCR("greater-than");

/* tidlt(tid, tid) => bool */ 
DATA(insert OID = 2791 ( tidlt  PGNSP PGUID 12 f f t f i 2 16 f "27 27" _null_ _null_ _null_ tidlt _null_ _null_ n 0));
DESCR("less-than");

/* tidge(tid, tid) => bool */ 
DATA(insert OID = 2792 ( tidge  PGNSP PGUID 12 f f t f i 2 16 f "27 27" _null_ _null_ _null_ tidge _null_ _null_ n 0));
DESCR("greater-than-or-equal");

/* tidle(tid, tid) => bool */ 
DATA(insert OID = 2793 ( tidle  PGNSP PGUID 12 f f t f i 2 16 f "27 27" _null_ _null_ _null_ tidle _null_ _null_ n 0));
DESCR("less-than-or-equal");

/* bttidcmp(tid, tid) => int4 */ 
DATA(insert OID = 2794 ( bttidcmp  PGNSP PGUID 12 f f t f i 2 23 f "27 27" _null_ _null_ _null_ bttidcmp _null_ _null_ n 0));
DESCR("btree less-equal-greater");

/* tidlarger(tid, tid) => tid */ 
DATA(insert OID = 2795 ( tidlarger  PGNSP PGUID 12 f f t f i 2 27 f "27 27" _null_ _null_ _null_ tidlarger _null_ _null_ n 0));
DESCR("larger of two");

/* tidsmaller(tid, tid) => tid */ 
DATA(insert OID = 2796 ( tidsmaller  PGNSP PGUID 12 f f t f i 2 27 f "27 27" _null_ _null_ _null_ tidsmaller _null_ _null_ n 0));
DESCR("smaller of two");

/* timedate_pl("time", date) => pg_catalog."timestamp" */ 
DATA(insert OID = 1296 ( timedate_pl  PGNSP PGUID 14 f f t f i 2 1114 f "1083 1082" _null_ _null_ _null_ "select ($2 + $1)" _null_ _null_ c 0));
DESCR("convert time and date to timestamp");

/* datetimetz_pl(date, timetz) => timestamptz */ 
DATA(insert OID = 1297 ( datetimetz_pl  PGNSP PGUID 12 f f t f i 2 1184 f "1082 1266" _null_ _null_ _null_ datetimetz_timestamptz _null_ _null_ n 0));
DESCR("convert date and time with time zone to timestamp with time zone");

/* timetzdate_pl(timetz, date) => pg_catalog.timestamptz */ 
DATA(insert OID = 1298 ( timetzdate_pl  PGNSP PGUID 14 f f t f i 2 1184 f "1266 1082" _null_ _null_ _null_ "select ($2 + $1)" _null_ _null_ c 0));
DESCR("convert time with time zone and date to timestamp with time zone");

/* now() => timestamptz */ 
DATA(insert OID = 1299 ( now  PGNSP PGUID 12 f f t f s 0 1184 f "" _null_ _null_ _null_ now _null_ _null_ n 0));
DESCR("current transaction time");

/* transaction_timestamp() => timestamptz */ 
DATA(insert OID = 2647 ( transaction_timestamp  PGNSP PGUID 12 f f t f s 0 1184 f "" _null_ _null_ _null_ now _null_ _null_ n 0));
DESCR("current transaction time");

/* statement_timestamp() => timestamptz */ 
DATA(insert OID = 2648 ( statement_timestamp  PGNSP PGUID 12 f f t f s 0 1184 f "" _null_ _null_ _null_ statement_timestamp _null_ _null_ n 0));
DESCR("current statement time");

/* clock_timestamp() => timestamptz */ 
DATA(insert OID = 2649 ( clock_timestamp  PGNSP PGUID 12 f f t f v 0 1184 f "" _null_ _null_ _null_ clock_timestamp _null_ _null_ n 0));
DESCR("current clock time");


/* OIDS 1300 - 1399 */
/* positionsel(internal, oid, internal, int4) => float8 */ 
DATA(insert OID = 1300 ( positionsel  PGNSP PGUID 12 f f t f s 4 701 f "2281 26 2281 23" _null_ _null_ _null_ positionsel _null_ _null_ n 0));
DESCR("restriction selectivity for position-comparison operators");

/* positionjoinsel(internal, oid, internal, int2) => float8 */ 
DATA(insert OID = 1301 ( positionjoinsel  PGNSP PGUID 12 f f t f s 4 701 f "2281 26 2281 21" _null_ _null_ _null_ positionjoinsel _null_ _null_ n 0));
DESCR("join selectivity for position-comparison operators");

/* contsel(internal, oid, internal, int4) => float8 */ 
DATA(insert OID = 1302 ( contsel  PGNSP PGUID 12 f f t f s 4 701 f "2281 26 2281 23" _null_ _null_ _null_ contsel _null_ _null_ n 0));
DESCR("restriction selectivity for containment comparison operators");

/* contjoinsel(internal, oid, internal, int2) => float8 */ 
DATA(insert OID = 1303 ( contjoinsel  PGNSP PGUID 12 f f t f s 4 701 f "2281 26 2281 21" _null_ _null_ _null_ contjoinsel _null_ _null_ n 0));
DESCR("join selectivity for containment comparison operators");

/* "overlaps"(timestamptz, timestamptz, timestamptz, timestamptz) => bool */ 
DATA(insert OID = 1304 ( overlaps  PGNSP PGUID 12 f f f f i 4 16 f "1184 1184 1184 1184" _null_ _null_ _null_ overlaps_timestamp _null_ _null_ n 0));
DESCR("SQL92 interval comparison");

/* "overlaps"(timestamptz, "interval", timestamptz, "interval") => pg_catalog.bool */ 
DATA(insert OID = 1305 ( overlaps  PGNSP PGUID 14 f f f f s 4 16 f "1184 1186 1184 1186" _null_ _null_ _null_ "select ($1, ($1 + $2)) overlaps ($3, ($3 + $4))" _null_ _null_ c 0));
DESCR("SQL92 interval comparison");

/* "overlaps"(timestamptz, timestamptz, timestamptz, "interval") => pg_catalog.bool */ 
DATA(insert OID = 1306 ( overlaps  PGNSP PGUID 14 f f f f s 4 16 f "1184 1184 1184 1186" _null_ _null_ _null_ "select ($1, $2) overlaps ($3, ($3 + $4))" _null_ _null_ c 0));
DESCR("SQL92 interval comparison");

/* "overlaps"(timestamptz, "interval", timestamptz, timestamptz) => pg_catalog.bool */ 
DATA(insert OID = 1307 ( overlaps  PGNSP PGUID 14 f f f f s 4 16 f "1184 1186 1184 1184" _null_ _null_ _null_ "select ($1, ($1 + $2)) overlaps ($3, $4)" _null_ _null_ c 0));
DESCR("SQL92 interval comparison");

/* "overlaps"("time", "time", "time", "time") => bool */ 
DATA(insert OID = 1308 ( overlaps  PGNSP PGUID 12 f f f f i 4 16 f "1083 1083 1083 1083" _null_ _null_ _null_ overlaps_time _null_ _null_ n 0));
DESCR("SQL92 interval comparison");

/* "overlaps"("time", "interval", "time", "interval") => pg_catalog.bool */ 
DATA(insert OID = 1309 ( overlaps  PGNSP PGUID 14 f f f f i 4 16 f "1083 1186 1083 1186" _null_ _null_ _null_ "select ($1, ($1 + $2)) overlaps ($3, ($3 + $4))" _null_ _null_ c 0));
DESCR("SQL92 interval comparison");

/* "overlaps"("time", "time", "time", "interval") => pg_catalog.bool */ 
DATA(insert OID = 1310 ( overlaps  PGNSP PGUID 14 f f f f i 4 16 f "1083 1083 1083 1186" _null_ _null_ _null_ "select ($1, $2) overlaps ($3, ($3 + $4))" _null_ _null_ c 0));
DESCR("SQL92 interval comparison");

/* "overlaps"("time", "interval", "time", "time") => pg_catalog.bool */ 
DATA(insert OID = 1311 ( overlaps  PGNSP PGUID 14 f f f f i 4 16 f "1083 1186 1083 1083" _null_ _null_ _null_ "select ($1, ($1 + $2)) overlaps ($3, $4)" _null_ _null_ c 0));
DESCR("SQL92 interval comparison");

/* timestamp_in(cstring, oid, int4) => "timestamp" */ 
DATA(insert OID = 1312 ( timestamp_in  PGNSP PGUID 12 f f t f s 3 1114 f "2275 26 23" _null_ _null_ _null_ timestamp_in _null_ _null_ n 0));
DESCR("I/O");

/* timestamp_out("timestamp") => cstring */ 
DATA(insert OID = 1313 ( timestamp_out  PGNSP PGUID 12 f f t f s 1 2275 f "1114" _null_ _null_ _null_ timestamp_out _null_ _null_ n 0));
DESCR("I/O");

/* timestamptz_cmp(timestamptz, timestamptz) => int4 */ 
DATA(insert OID = 1314 ( timestamptz_cmp  PGNSP PGUID 12 f f t f i 2 23 f "1184 1184" _null_ _null_ _null_ timestamp_cmp _null_ _null_ n 0));
DESCR("less-equal-greater");

/* interval_cmp("interval", "interval") => int4 */ 
DATA(insert OID = 1315 ( interval_cmp  PGNSP PGUID 12 f f t f i 2 23 f "1186 1186" _null_ _null_ _null_ interval_cmp _null_ _null_ n 0));
DESCR("less-equal-greater");

/* "time"("timestamp") => "time" */ 
DATA(insert OID = 1316 ( time  PGNSP PGUID 12 f f t f i 1 1083 f "1114" _null_ _null_ _null_ timestamp_time _null_ _null_ n 0));
DESCR("convert timestamp to time");

/* length(text) => int4 */ 
DATA(insert OID = 1317 ( length  PGNSP PGUID 12 f f t f i 1 23 f "25" _null_ _null_ _null_ textlen _null_ _null_ n 0));
DESCR("length");

/* length(bpchar) => int4 */ 
DATA(insert OID = 1318 ( length  PGNSP PGUID 12 f f t f i 1 23 f "1042" _null_ _null_ _null_ bpcharlen _null_ _null_ n 0));
DESCR("character length");

/* xideqint4(xid, int4) => bool */ 
DATA(insert OID = 1319 ( xideqint4  PGNSP PGUID 12 f f t f i 2 16 f "28 23" _null_ _null_ _null_ xideq _null_ _null_ n 0));
DESCR("equal");

/* interval_div("interval", float8) => "interval" */ 
DATA(insert OID = 1326 ( interval_div  PGNSP PGUID 12 f f t f i 2 1186 f "1186 701" _null_ _null_ _null_ interval_div _null_ _null_ n 0));
DESCR("divide");

/* interval_interval_div("interval", "interval") => float8 */ 
DATA(insert OID = 2918 ( interval_interval_div  PGNSP PGUID 12 f f t f i 2 701 f "1186 1186" _null_ _null_ _null_ interval_interval_div _null_ _null_ n 0));
DESCR("divide");

/* interval_interval_mod("interval", "interval") => "interval" */ 
DATA(insert OID = 2919 ( interval_interval_mod  PGNSP PGUID 12 f f t f i 2 1186 f "1186 1186" _null_ _null_ _null_ interval_interval_mod _null_ _null_ n 0));
DESCR("modulus");

/* dlog10(float8) => float8 */ 
DATA(insert OID = 1339 ( dlog10  PGNSP PGUID 12 f f t f i 1 701 f "701" _null_ _null_ _null_ dlog10 _null_ _null_ n 0));
DESCR("base 10 logarithm");

/* "log"(float8) => float8 */ 
DATA(insert OID = 1340 ( log  PGNSP PGUID 12 f f t f i 1 701 f "701" _null_ _null_ _null_ dlog10 _null_ _null_ n 0));
DESCR("base 10 logarithm");

/* ln(float8) => float8 */ 
DATA(insert OID = 1341 ( ln  PGNSP PGUID 12 f f t f i 1 701 f "701" _null_ _null_ _null_ dlog1 _null_ _null_ n 0));
DESCR("natural logarithm");

/* round(float8) => float8 */ 
DATA(insert OID = 1342 ( round  PGNSP PGUID 12 f f t f i 1 701 f "701" _null_ _null_ _null_ dround _null_ _null_ n 0));
DESCR("round to nearest integer");

/* trunc(float8) => float8 */ 
DATA(insert OID = 1343 ( trunc  PGNSP PGUID 12 f f t f i 1 701 f "701" _null_ _null_ _null_ dtrunc _null_ _null_ n 0));
DESCR("truncate to integer");

/* sqrt(float8) => float8 */ 
DATA(insert OID = 1344 ( sqrt  PGNSP PGUID 12 f f t f i 1 701 f "701" _null_ _null_ _null_ dsqrt _null_ _null_ n 0));
DESCR("square root");

/* cbrt(float8) => float8 */ 
DATA(insert OID = 1345 ( cbrt  PGNSP PGUID 12 f f t f i 1 701 f "701" _null_ _null_ _null_ dcbrt _null_ _null_ n 0));
DESCR("cube root");

/* pow(float8, float8) => float8 */ 
DATA(insert OID = 1346 ( pow  PGNSP PGUID 12 f f t f i 2 701 f "701 701" _null_ _null_ _null_ dpow _null_ _null_ n 0));
DESCR("exponentiation");

/* power(float8, float8) => float8 */ 
DATA(insert OID = 1368 ( power  PGNSP PGUID 12 f f t f i 2 701 f "701 701" _null_ _null_ _null_ dpow _null_ _null_ n 0));
DESCR("exponentiation");

/* exp(float8) => float8 */ 
DATA(insert OID = 1347 ( exp  PGNSP PGUID 12 f f t f i 1 701 f "701" _null_ _null_ _null_ dexp _null_ _null_ n 0));
DESCR("exponential");


/* This form of obj_description is now deprecated, since it will fail if */
/* OIDs are not unique across system catalogs.	Use the other forms instead. */
/* obj_description(oid) => pg_catalog.text */ 
DATA(insert OID = 1348 ( obj_description  PGNSP PGUID 14 f f t f s 1 25 f "26" _null_ _null_ _null_ "select description from pg_catalog.pg_description where objoid = $1 and objsubid = 0" _null_ _null_ c 0));
DESCR("get description for object id (deprecated)");

/* oidvectortypes(oidvector) => text */ 
DATA(insert OID = 1349 ( oidvectortypes  PGNSP PGUID 12 f f t f s 1 25 f "30" _null_ _null_ _null_ oidvectortypes _null_ _null_ n 0));
DESCR("print type names of oidvector field");

/* timetz_in(cstring, oid, int4) => timetz */ 
DATA(insert OID = 1350 ( timetz_in  PGNSP PGUID 12 f f t f s 3 1266 f "2275 26 23" _null_ _null_ _null_ timetz_in _null_ _null_ n 0));
DESCR("I/O");

/* timetz_out(timetz) => cstring */ 
DATA(insert OID = 1351 ( timetz_out  PGNSP PGUID 12 f f t f i 1 2275 f "1266" _null_ _null_ _null_ timetz_out _null_ _null_ n 0));
DESCR("I/O");

/* timetz_eq(timetz, timetz) => bool */ 
DATA(insert OID = 1352 ( timetz_eq  PGNSP PGUID 12 f f t f i 2 16 f "1266 1266" _null_ _null_ _null_ timetz_eq _null_ _null_ n 0));
DESCR("equal");

/* timetz_ne(timetz, timetz) => bool */ 
DATA(insert OID = 1353 ( timetz_ne  PGNSP PGUID 12 f f t f i 2 16 f "1266 1266" _null_ _null_ _null_ timetz_ne _null_ _null_ n 0));
DESCR("not equal");

/* timetz_lt(timetz, timetz) => bool */ 
DATA(insert OID = 1354 ( timetz_lt  PGNSP PGUID 12 f f t f i 2 16 f "1266 1266" _null_ _null_ _null_ timetz_lt _null_ _null_ n 0));
DESCR("less-than");

/* timetz_le(timetz, timetz) => bool */ 
DATA(insert OID = 1355 ( timetz_le  PGNSP PGUID 12 f f t f i 2 16 f "1266 1266" _null_ _null_ _null_ timetz_le _null_ _null_ n 0));
DESCR("less-than-or-equal");

/* timetz_ge(timetz, timetz) => bool */ 
DATA(insert OID = 1356 ( timetz_ge  PGNSP PGUID 12 f f t f i 2 16 f "1266 1266" _null_ _null_ _null_ timetz_ge _null_ _null_ n 0));
DESCR("greater-than-or-equal");

/* timetz_gt(timetz, timetz) => bool */ 
DATA(insert OID = 1357 ( timetz_gt  PGNSP PGUID 12 f f t f i 2 16 f "1266 1266" _null_ _null_ _null_ timetz_gt _null_ _null_ n 0));
DESCR("greater-than");

/* timetz_cmp(timetz, timetz) => int4 */ 
DATA(insert OID = 1358 ( timetz_cmp  PGNSP PGUID 12 f f t f i 2 23 f "1266 1266" _null_ _null_ _null_ timetz_cmp _null_ _null_ n 0));
DESCR("less-equal-greater");

/* timestamptz(date, timetz) => timestamptz */ 
DATA(insert OID = 1359 ( timestamptz  PGNSP PGUID 12 f f t f i 2 1184 f "1082 1266" _null_ _null_ _null_ datetimetz_timestamptz _null_ _null_ n 0));
DESCR("convert date and time with time zone to timestamp with time zone");

/* "time"(abstime) => pg_catalog."time" */ 
DATA(insert OID = 1364 ( time  PGNSP PGUID 14 f f t f s 1 1083 f "702" _null_ _null_ _null_ "select cast(cast($1 as timestamp without time zone) as time)" _null_ _null_ c 0));
DESCR("convert abstime to time");

/* character_length(bpchar) => int4 */ 
DATA(insert OID = 1367 ( character_length  PGNSP PGUID 12 f f t f i 1 23 f "1042" _null_ _null_ _null_ bpcharlen _null_ _null_ n 0));
DESCR("character length");

/* character_length(text) => int4 */ 
DATA(insert OID = 1369 ( character_length  PGNSP PGUID 12 f f t f i 1 23 f "25" _null_ _null_ _null_ textlen _null_ _null_ n 0));
DESCR("character length");

/* "interval"("time") => "interval" */ 
DATA(insert OID = 1370 ( interval  PGNSP PGUID 12 f f t f i 1 1186 f "1083" _null_ _null_ _null_ time_interval _null_ _null_ n 0));
DESCR("convert time to interval");

/* char_length(bpchar) => int4 */ 
DATA(insert OID = 1372 ( char_length  PGNSP PGUID 12 f f t f i 1 23 f "1042" _null_ _null_ _null_ bpcharlen _null_ _null_ n 0));
DESCR("character length");

/* array_type_length_coerce(anyarray, int4, bool) => anyarray */ 
DATA(insert OID = 1373 ( array_type_length_coerce  PGNSP PGUID 12 f f t f s 3 2277 f "2277 23 16" _null_ _null_ _null_ array_type_length_coerce _null_ _null_ n 0));
DESCR("coerce array to another type and adjust element typmod");

/* octet_length(text) => int4 */ 
DATA(insert OID = 1374 ( octet_length  PGNSP PGUID 12 f f t f i 1 23 f "25" _null_ _null_ _null_ textoctetlen _null_ _null_ n 0));
DESCR("octet length");

/* octet_length(bpchar) => int4 */ 
DATA(insert OID = 1375 ( octet_length  PGNSP PGUID 12 f f t f i 1 23 f "1042" _null_ _null_ _null_ bpcharoctetlen _null_ _null_ n 0));
DESCR("octet length");

/* time_larger("time", "time") => "time" */ 
DATA(insert OID = 1377 ( time_larger  PGNSP PGUID 12 f f t f i 2 1083 f "1083 1083" _null_ _null_ _null_ time_larger _null_ _null_ n 0));
DESCR("larger of two");

/* time_smaller("time", "time") => "time" */ 
DATA(insert OID = 1378 ( time_smaller  PGNSP PGUID 12 f f t f i 2 1083 f "1083 1083" _null_ _null_ _null_ time_smaller _null_ _null_ n 0));
DESCR("smaller of two");

/* timetz_larger(timetz, timetz) => timetz */ 
DATA(insert OID = 1379 ( timetz_larger  PGNSP PGUID 12 f f t f i 2 1266 f "1266 1266" _null_ _null_ _null_ timetz_larger _null_ _null_ n 0));
DESCR("larger of two");

/* timetz_smaller(timetz, timetz) => timetz */ 
DATA(insert OID = 1380 ( timetz_smaller  PGNSP PGUID 12 f f t f i 2 1266 f "1266 1266" _null_ _null_ _null_ timetz_smaller _null_ _null_ n 0));
DESCR("smaller of two");

/* char_length(text) => int4 */ 
DATA(insert OID = 1381 ( char_length  PGNSP PGUID 12 f f t f i 1 23 f "25" _null_ _null_ _null_ textlen _null_ _null_ n 0));
DESCR("character length");

/* date_part(text, abstime) => pg_catalog.float8 */ 
DATA(insert OID = 1382 ( date_part  PGNSP PGUID 14 f f t f s 2 701 f "25 702" _null_ _null_ _null_ "select pg_catalog.date_part($1, cast($2 as timestamp with time zone))" _null_ _null_ c 0));
DESCR("extract field from abstime");

/* date_part(text, reltime) => pg_catalog.float8 */ 
DATA(insert OID = 1383 ( date_part  PGNSP PGUID 14 f f t f s 2 701 f "25 703" _null_ _null_ _null_ "select pg_catalog.date_part($1, cast($2 as pg_catalog.interval))" _null_ _null_ c 0));
DESCR("extract field from reltime");

/* date_part(text, date) => pg_catalog.float8 */ 
DATA(insert OID = 1384 ( date_part  PGNSP PGUID 14 f f t f i 2 701 f "25 1082" _null_ _null_ _null_ "select pg_catalog.date_part($1, cast($2 as timestamp without time zone))" _null_ _null_ c 0));
DESCR("extract field from date");

/* date_part(text, "time") => float8 */ 
DATA(insert OID = 1385 ( date_part  PGNSP PGUID 12 f f t f i 2 701 f "25 1083" _null_ _null_ _null_ time_part _null_ _null_ n 0));
DESCR("extract field from time");

/* age(timestamptz) => pg_catalog."interval" */ 
DATA(insert OID = 1386 ( age  PGNSP PGUID 14 f f t f s 1 1186 f "1184" _null_ _null_ _null_ "select pg_catalog.age(cast(current_date as timestamp with time zone), $1)" _null_ _null_ c 0));
DESCR("date difference from today preserving months and years");

/* timetz(timestamptz) => timetz */ 
DATA(insert OID = 1388 ( timetz  PGNSP PGUID 12 f f t f s 1 1266 f "1184" _null_ _null_ _null_ timestamptz_timetz _null_ _null_ n 0));
DESCR("convert timestamptz to timetz");

/* isfinite(timestamptz) => bool */ 
DATA(insert OID = 1389 ( isfinite  PGNSP PGUID 12 f f t f i 1 16 f "1184" _null_ _null_ _null_ timestamp_finite _null_ _null_ n 0));
DESCR("finite timestamp?");

/* isfinite("interval") => bool */ 
DATA(insert OID = 1390 ( isfinite  PGNSP PGUID 12 f f t f i 1 16 f "1186" _null_ _null_ _null_ interval_finite _null_ _null_ n 0));
DESCR("finite interval?");

/* factorial(int8) => "numeric" */ 
DATA(insert OID = 1376 ( factorial  PGNSP PGUID 12 f f t f i 1 1700 f "20" _null_ _null_ _null_ numeric_fac _null_ _null_ n 0));
DESCR("factorial");

/* abs(float4) => float4 */ 
DATA(insert OID = 1394 ( abs  PGNSP PGUID 12 f f t f i 1 700 f "700" _null_ _null_ _null_ float4abs _null_ _null_ n 0));
DESCR("absolute value");

/* abs(float8) => float8 */ 
DATA(insert OID = 1395 ( abs  PGNSP PGUID 12 f f t f i 1 701 f "701" _null_ _null_ _null_ float8abs _null_ _null_ n 0));
DESCR("absolute value");

/* abs(int8) => int8 */ 
DATA(insert OID = 1396 ( abs  PGNSP PGUID 12 f f t f i 1 20 f "20" _null_ _null_ _null_ int8abs _null_ _null_ n 0));
DESCR("absolute value");

/* abs(int4) => int4 */ 
DATA(insert OID = 1397 ( abs  PGNSP PGUID 12 f f t f i 1 23 f "23" _null_ _null_ _null_ int4abs _null_ _null_ n 0));
DESCR("absolute value");

/* abs(int2) => int2 */ 
DATA(insert OID = 1398 ( abs  PGNSP PGUID 12 f f t f i 1 21 f "21" _null_ _null_ _null_ int2abs _null_ _null_ n 0));
DESCR("absolute value");


/* OIDS 1400 - 1499 */
/* name("varchar") => name */ 
DATA(insert OID = 1400 ( name  PGNSP PGUID 12 f f t f i 1 19 f "1043" _null_ _null_ _null_ text_name _null_ _null_ n 0));
DESCR("convert varchar to name");

/* "varchar"(name) => "varchar" */ 
DATA(insert OID = 1401 ( varchar  PGNSP PGUID 12 f f t f i 1 1043 f "19" _null_ _null_ _null_ name_text _null_ _null_ n 0));
DESCR("convert name to varchar");

/* "current_schema"() => name */ 
DATA(insert OID = 1402 ( current_schema  PGNSP PGUID 12 f f t f s 0 19 f "" _null_ _null_ _null_ current_schema _null_ _null_ n 0));
DESCR("current schema name");

/* current_schemas(bool) => _name */ 
DATA(insert OID = 1403 ( current_schemas  PGNSP PGUID 12 f f t f s 1 1003 f "16" _null_ _null_ _null_ current_schemas _null_ _null_ n 0));
DESCR("current schema search list");

/* "overlay"(text, text, int4, int4) => pg_catalog.text */ 
DATA(insert OID = 1404 ( overlay  PGNSP PGUID 14 f f t f i 4 25 f "25 25 23 23" _null_ _null_ _null_ "select pg_catalog.substring($1, 1, ($3 - 1)) || $2 || pg_catalog.substring($1, ($3 + $4))" _null_ _null_ c 0));
DESCR("substitute portion of string");

/* "overlay"(text, text, int4) => pg_catalog.text */ 
DATA(insert OID = 1405 ( overlay  PGNSP PGUID 14 f f t f i 3 25 f "25 25 23" _null_ _null_ _null_ "select pg_catalog.substring($1, 1, ($3 - 1)) || $2 || pg_catalog.substring($1, ($3 + pg_catalog.char_length($2)))" _null_ _null_ c 0));
DESCR("substitute portion of string");

/* isvertical(point, point) => bool */ 
DATA(insert OID = 1406 ( isvertical  PGNSP PGUID 12 f f t f i 2 16 f "600 600" _null_ _null_ _null_ point_vert _null_ _null_ n 0));
DESCR("vertically aligned?");

/* ishorizontal(point, point) => bool */ 
DATA(insert OID = 1407 ( ishorizontal  PGNSP PGUID 12 f f t f i 2 16 f "600 600" _null_ _null_ _null_ point_horiz _null_ _null_ n 0));
DESCR("horizontally aligned?");

/* isparallel(lseg, lseg) => bool */ 
DATA(insert OID = 1408 ( isparallel  PGNSP PGUID 12 f f t f i 2 16 f "601 601" _null_ _null_ _null_ lseg_parallel _null_ _null_ n 0));
DESCR("parallel?");

/* isperp(lseg, lseg) => bool */ 
DATA(insert OID = 1409 ( isperp  PGNSP PGUID 12 f f t f i 2 16 f "601 601" _null_ _null_ _null_ lseg_perp _null_ _null_ n 0));
DESCR("perpendicular?");

/* isvertical(lseg) => bool */ 
DATA(insert OID = 1410 ( isvertical  PGNSP PGUID 12 f f t f i 1 16 f "601" _null_ _null_ _null_ lseg_vertical _null_ _null_ n 0));
DESCR("vertical?");

/* ishorizontal(lseg) => bool */ 
DATA(insert OID = 1411 ( ishorizontal  PGNSP PGUID 12 f f t f i 1 16 f "601" _null_ _null_ _null_ lseg_horizontal _null_ _null_ n 0));
DESCR("horizontal?");

/* isparallel(line, line) => bool */ 
DATA(insert OID = 1412 ( isparallel  PGNSP PGUID 12 f f t f i 2 16 f "628 628" _null_ _null_ _null_ line_parallel _null_ _null_ n 0));
DESCR("parallel?");

/* isperp(line, line) => bool */ 
DATA(insert OID = 1413 ( isperp  PGNSP PGUID 12 f f t f i 2 16 f "628 628" _null_ _null_ _null_ line_perp _null_ _null_ n 0));
DESCR("perpendicular?");

/* isvertical(line) => bool */ 
DATA(insert OID = 1414 ( isvertical  PGNSP PGUID 12 f f t f i 1 16 f "628" _null_ _null_ _null_ line_vertical _null_ _null_ n 0));
DESCR("vertical?");

/* ishorizontal(line) => bool */ 
DATA(insert OID = 1415 ( ishorizontal  PGNSP PGUID 12 f f t f i 1 16 f "628" _null_ _null_ _null_ line_horizontal _null_ _null_ n 0));
DESCR("horizontal?");

/* point(circle) => point */ 
DATA(insert OID = 1416 ( point  PGNSP PGUID 12 f f t f i 1 600 f "718" _null_ _null_ _null_ circle_center _null_ _null_ n 0));
DESCR("center of");

/* isnottrue(bool) => bool */ 
DATA(insert OID = 1417 ( isnottrue  PGNSP PGUID 12 f f f f i 1 16 f "16" _null_ _null_ _null_ isnottrue _null_ _null_ n 0));
DESCR("bool is not true (ie, false or unknown)");

/* isnotfalse(bool) => bool */ 
DATA(insert OID = 1418 ( isnotfalse  PGNSP PGUID 12 f f f f i 1 16 f "16" _null_ _null_ _null_ isnotfalse _null_ _null_ n 0));
DESCR("bool is not false (ie, true or unknown)");

/* "time"("interval") => "time" */ 
DATA(insert OID = 1419 ( time  PGNSP PGUID 12 f f t f i 1 1083 f "1186" _null_ _null_ _null_ interval_time _null_ _null_ n 0));
DESCR("convert interval to time");

/* box(point, point) => box */ 
DATA(insert OID = 1421 ( box  PGNSP PGUID 12 f f t f i 2 603 f "600 600" _null_ _null_ _null_ points_box _null_ _null_ n 0));
DESCR("convert points to box");

/* box_add(box, point) => box */ 
DATA(insert OID = 1422 ( box_add  PGNSP PGUID 12 f f t f i 2 603 f "603 600" _null_ _null_ _null_ box_add _null_ _null_ n 0));
DESCR("add point to box (translate)");

/* box_sub(box, point) => box */ 
DATA(insert OID = 1423 ( box_sub  PGNSP PGUID 12 f f t f i 2 603 f "603 600" _null_ _null_ _null_ box_sub _null_ _null_ n 0));
DESCR("subtract point from box (translate)");

/* box_mul(box, point) => box */ 
DATA(insert OID = 1424 ( box_mul  PGNSP PGUID 12 f f t f i 2 603 f "603 600" _null_ _null_ _null_ box_mul _null_ _null_ n 0));
DESCR("multiply box by point (scale)");

/* box_div(box, point) => box */ 
DATA(insert OID = 1425 ( box_div  PGNSP PGUID 12 f f t f i 2 603 f "603 600" _null_ _null_ _null_ box_div _null_ _null_ n 0));
DESCR("divide box by point (scale)");

/* path_contain_pt(path, point) => pg_catalog.bool */ 
DATA(insert OID = 1426 ( path_contain_pt  PGNSP PGUID 14 f f t f i 2 16 f "602 600" _null_ _null_ _null_ "select pg_catalog.on_ppath($2, $1)" _null_ _null_ c 0));
DESCR("path contains point?");

/* poly_contain_pt(polygon, point) => bool */ 
DATA(insert OID = 1428 ( poly_contain_pt  PGNSP PGUID 12 f f t f i 2 16 f "604 600" _null_ _null_ _null_ poly_contain_pt _null_ _null_ n 0));
DESCR("polygon contains point?");

/* pt_contained_poly(point, polygon) => bool */ 
DATA(insert OID = 1429 ( pt_contained_poly  PGNSP PGUID 12 f f t f i 2 16 f "600 604" _null_ _null_ _null_ pt_contained_poly _null_ _null_ n 0));
DESCR("point contained in polygon?");

/* isclosed(path) => bool */ 
DATA(insert OID = 1430 ( isclosed  PGNSP PGUID 12 f f t f i 1 16 f "602" _null_ _null_ _null_ path_isclosed _null_ _null_ n 0));
DESCR("path closed?");

/* isopen(path) => bool */ 
DATA(insert OID = 1431 ( isopen  PGNSP PGUID 12 f f t f i 1 16 f "602" _null_ _null_ _null_ path_isopen _null_ _null_ n 0));
DESCR("path open?");

/* path_npoints(path) => int4 */ 
DATA(insert OID = 1432 ( path_npoints  PGNSP PGUID 12 f f t f i 1 23 f "602" _null_ _null_ _null_ path_npoints _null_ _null_ n 0));
DESCR("number of points in path");


/* pclose and popen might better be named close and open, but that crashes initdb. */
/* - thomas 97/04/20 */
/* pclose(path) => path */ 
DATA(insert OID = 1433 ( pclose  PGNSP PGUID 12 f f t f i 1 602 f "602" _null_ _null_ _null_ path_close _null_ _null_ n 0));
DESCR("close path");

/* popen(path) => path */ 
DATA(insert OID = 1434 ( popen  PGNSP PGUID 12 f f t f i 1 602 f "602" _null_ _null_ _null_ path_open _null_ _null_ n 0));
DESCR("open path");

/* path_add(path, path) => path */ 
DATA(insert OID = 1435 ( path_add  PGNSP PGUID 12 f f t f i 2 602 f "602 602" _null_ _null_ _null_ path_add _null_ _null_ n 0));
DESCR("concatenate open paths");

/* path_add_pt(path, point) => path */ 
DATA(insert OID = 1436 ( path_add_pt  PGNSP PGUID 12 f f t f i 2 602 f "602 600" _null_ _null_ _null_ path_add_pt _null_ _null_ n 0));
DESCR("add (translate path)");

/* path_sub_pt(path, point) => path */ 
DATA(insert OID = 1437 ( path_sub_pt  PGNSP PGUID 12 f f t f i 2 602 f "602 600" _null_ _null_ _null_ path_sub_pt _null_ _null_ n 0));
DESCR("subtract (translate path)");

/* path_mul_pt(path, point) => path */ 
DATA(insert OID = 1438 ( path_mul_pt  PGNSP PGUID 12 f f t f i 2 602 f "602 600" _null_ _null_ _null_ path_mul_pt _null_ _null_ n 0));
DESCR("multiply (rotate/scale path)");

/* path_div_pt(path, point) => path */ 
DATA(insert OID = 1439 ( path_div_pt  PGNSP PGUID 12 f f t f i 2 602 f "602 600" _null_ _null_ _null_ path_div_pt _null_ _null_ n 0));
DESCR("divide (rotate/scale path)");

/* point(float8, float8) => point */ 
DATA(insert OID = 1440 ( point  PGNSP PGUID 12 f f t f i 2 600 f "701 701" _null_ _null_ _null_ construct_point _null_ _null_ n 0));
DESCR("convert x, y to point");

/* point_add(point, point) => point */ 
DATA(insert OID = 1441 ( point_add  PGNSP PGUID 12 f f t f i 2 600 f "600 600" _null_ _null_ _null_ point_add _null_ _null_ n 0));
DESCR("add points (translate)");

/* point_sub(point, point) => point */ 
DATA(insert OID = 1442 ( point_sub  PGNSP PGUID 12 f f t f i 2 600 f "600 600" _null_ _null_ _null_ point_sub _null_ _null_ n 0));
DESCR("subtract points (translate)");

/* point_mul(point, point) => point */ 
DATA(insert OID = 1443 ( point_mul  PGNSP PGUID 12 f f t f i 2 600 f "600 600" _null_ _null_ _null_ point_mul _null_ _null_ n 0));
DESCR("multiply points (scale/rotate)");

/* point_div(point, point) => point */ 
DATA(insert OID = 1444 ( point_div  PGNSP PGUID 12 f f t f i 2 600 f "600 600" _null_ _null_ _null_ point_div _null_ _null_ n 0));
DESCR("divide points (scale/rotate)");

/* poly_npoints(polygon) => int4 */ 
DATA(insert OID = 1445 ( poly_npoints  PGNSP PGUID 12 f f t f i 1 23 f "604" _null_ _null_ _null_ poly_npoints _null_ _null_ n 0));
DESCR("number of points in polygon");

/* box(polygon) => box */ 
DATA(insert OID = 1446 ( box  PGNSP PGUID 12 f f t f i 1 603 f "604" _null_ _null_ _null_ poly_box _null_ _null_ n 0));
DESCR("convert polygon to bounding box");

/* path(polygon) => path */ 
DATA(insert OID = 1447 ( path  PGNSP PGUID 12 f f t f i 1 602 f "604" _null_ _null_ _null_ poly_path _null_ _null_ n 0));
DESCR("convert polygon to path");

/* polygon(box) => polygon */ 
DATA(insert OID = 1448 ( polygon  PGNSP PGUID 12 f f t f i 1 604 f "603" _null_ _null_ _null_ box_poly _null_ _null_ n 0));
DESCR("convert box to polygon");

/* polygon(path) => polygon */ 
DATA(insert OID = 1449 ( polygon  PGNSP PGUID 12 f f t f i 1 604 f "602" _null_ _null_ _null_ path_poly _null_ _null_ n 0));
DESCR("convert path to polygon");

/* circle_in(cstring) => circle */ 
DATA(insert OID = 1450 ( circle_in  PGNSP PGUID 12 f f t f i 1 718 f "2275" _null_ _null_ _null_ circle_in _null_ _null_ n 0));
DESCR("I/O");

/* circle_out(circle) => cstring */ 
DATA(insert OID = 1451 ( circle_out  PGNSP PGUID 12 f f t f i 1 2275 f "718" _null_ _null_ _null_ circle_out _null_ _null_ n 0));
DESCR("I/O");

/* circle_same(circle, circle) => bool */ 
DATA(insert OID = 1452 ( circle_same  PGNSP PGUID 12 f f t f i 2 16 f "718 718" _null_ _null_ _null_ circle_same _null_ _null_ n 0));
DESCR("same as?");

/* circle_contain(circle, circle) => bool */ 
DATA(insert OID = 1453 ( circle_contain  PGNSP PGUID 12 f f t f i 2 16 f "718 718" _null_ _null_ _null_ circle_contain _null_ _null_ n 0));
DESCR("contains?");

/* circle_left(circle, circle) => bool */ 
DATA(insert OID = 1454 ( circle_left  PGNSP PGUID 12 f f t f i 2 16 f "718 718" _null_ _null_ _null_ circle_left _null_ _null_ n 0));
DESCR("is left of");

/* circle_overleft(circle, circle) => bool */ 
DATA(insert OID = 1455 ( circle_overleft  PGNSP PGUID 12 f f t f i 2 16 f "718 718" _null_ _null_ _null_ circle_overleft _null_ _null_ n 0));
DESCR("overlaps or is left of");

/* circle_overright(circle, circle) => bool */ 
DATA(insert OID = 1456 ( circle_overright  PGNSP PGUID 12 f f t f i 2 16 f "718 718" _null_ _null_ _null_ circle_overright _null_ _null_ n 0));
DESCR("overlaps or is right of");

/* circle_right(circle, circle) => bool */ 
DATA(insert OID = 1457 ( circle_right  PGNSP PGUID 12 f f t f i 2 16 f "718 718" _null_ _null_ _null_ circle_right _null_ _null_ n 0));
DESCR("is right of");

/* circle_contained(circle, circle) => bool */ 
DATA(insert OID = 1458 ( circle_contained  PGNSP PGUID 12 f f t f i 2 16 f "718 718" _null_ _null_ _null_ circle_contained _null_ _null_ n 0));
DESCR("is contained by?");

/* circle_overlap(circle, circle) => bool */ 
DATA(insert OID = 1459 ( circle_overlap  PGNSP PGUID 12 f f t f i 2 16 f "718 718" _null_ _null_ _null_ circle_overlap _null_ _null_ n 0));
DESCR("overlaps");

/* circle_below(circle, circle) => bool */ 
DATA(insert OID = 1460 ( circle_below  PGNSP PGUID 12 f f t f i 2 16 f "718 718" _null_ _null_ _null_ circle_below _null_ _null_ n 0));
DESCR("is below");

/* circle_above(circle, circle) => bool */ 
DATA(insert OID = 1461 ( circle_above  PGNSP PGUID 12 f f t f i 2 16 f "718 718" _null_ _null_ _null_ circle_above _null_ _null_ n 0));
DESCR("is above");

/* circle_eq(circle, circle) => bool */ 
DATA(insert OID = 1462 ( circle_eq  PGNSP PGUID 12 f f t f i 2 16 f "718 718" _null_ _null_ _null_ circle_eq _null_ _null_ n 0));
DESCR("equal by area");

/* circle_ne(circle, circle) => bool */ 
DATA(insert OID = 1463 ( circle_ne  PGNSP PGUID 12 f f t f i 2 16 f "718 718" _null_ _null_ _null_ circle_ne _null_ _null_ n 0));
DESCR("not equal by area");

/* circle_lt(circle, circle) => bool */ 
DATA(insert OID = 1464 ( circle_lt  PGNSP PGUID 12 f f t f i 2 16 f "718 718" _null_ _null_ _null_ circle_lt _null_ _null_ n 0));
DESCR("less-than by area");

/* circle_gt(circle, circle) => bool */ 
DATA(insert OID = 1465 ( circle_gt  PGNSP PGUID 12 f f t f i 2 16 f "718 718" _null_ _null_ _null_ circle_gt _null_ _null_ n 0));
DESCR("greater-than by area");

/* circle_le(circle, circle) => bool */ 
DATA(insert OID = 1466 ( circle_le  PGNSP PGUID 12 f f t f i 2 16 f "718 718" _null_ _null_ _null_ circle_le _null_ _null_ n 0));
DESCR("less-than-or-equal by area");

/* circle_ge(circle, circle) => bool */ 
DATA(insert OID = 1467 ( circle_ge  PGNSP PGUID 12 f f t f i 2 16 f "718 718" _null_ _null_ _null_ circle_ge _null_ _null_ n 0));
DESCR("greater-than-or-equal by area");

/* area(circle) => float8 */ 
DATA(insert OID = 1468 ( area  PGNSP PGUID 12 f f t f i 1 701 f "718" _null_ _null_ _null_ circle_area _null_ _null_ n 0));
DESCR("area of circle");

/* diameter(circle) => float8 */ 
DATA(insert OID = 1469 ( diameter  PGNSP PGUID 12 f f t f i 1 701 f "718" _null_ _null_ _null_ circle_diameter _null_ _null_ n 0));
DESCR("diameter of circle");

/* radius(circle) => float8 */ 
DATA(insert OID = 1470 ( radius  PGNSP PGUID 12 f f t f i 1 701 f "718" _null_ _null_ _null_ circle_radius _null_ _null_ n 0));
DESCR("radius of circle");

/* circle_distance(circle, circle) => float8 */ 
DATA(insert OID = 1471 ( circle_distance  PGNSP PGUID 12 f f t f i 2 701 f "718 718" _null_ _null_ _null_ circle_distance _null_ _null_ n 0));
DESCR("distance between");

/* circle_center(circle) => point */ 
DATA(insert OID = 1472 ( circle_center  PGNSP PGUID 12 f f t f i 1 600 f "718" _null_ _null_ _null_ circle_center _null_ _null_ n 0));
DESCR("center of");

/* circle(point, float8) => circle */ 
DATA(insert OID = 1473 ( circle  PGNSP PGUID 12 f f t f i 2 718 f "600 701" _null_ _null_ _null_ cr_circle _null_ _null_ n 0));
DESCR("convert point and radius to circle");

/* circle(polygon) => circle */ 
DATA(insert OID = 1474 ( circle  PGNSP PGUID 12 f f t f i 1 718 f "604" _null_ _null_ _null_ poly_circle _null_ _null_ n 0));
DESCR("convert polygon to circle");

/* polygon(int4, circle) => polygon */ 
DATA(insert OID = 1475 ( polygon  PGNSP PGUID 12 f f t f i 2 604 f "23 718" _null_ _null_ _null_ circle_poly _null_ _null_ n 0));
DESCR("convert vertex count and circle to polygon");

/* dist_pc(point, circle) => float8 */ 
DATA(insert OID = 1476 ( dist_pc  PGNSP PGUID 12 f f t f i 2 701 f "600 718" _null_ _null_ _null_ dist_pc _null_ _null_ n 0));
DESCR("distance between point and circle");

/* circle_contain_pt(circle, point) => bool */ 
DATA(insert OID = 1477 ( circle_contain_pt  PGNSP PGUID 12 f f t f i 2 16 f "718 600" _null_ _null_ _null_ circle_contain_pt _null_ _null_ n 0));
DESCR("circle contains point?");

/* pt_contained_circle(point, circle) => bool */ 
DATA(insert OID = 1478 ( pt_contained_circle  PGNSP PGUID 12 f f t f i 2 16 f "600 718" _null_ _null_ _null_ pt_contained_circle _null_ _null_ n 0));
DESCR("point contained in circle?");

/* circle(box) => circle */ 
DATA(insert OID = 1479 ( circle  PGNSP PGUID 12 f f t f i 1 718 f "603" _null_ _null_ _null_ box_circle _null_ _null_ n 0));
DESCR("convert box to circle");

/* box(circle) => box */ 
DATA(insert OID = 1480 ( box  PGNSP PGUID 12 f f t f i 1 603 f "718" _null_ _null_ _null_ circle_box _null_ _null_ n 0));
DESCR("convert circle to box");

/* tinterval(abstime, abstime) => tinterval */ 
DATA(insert OID = 1481 ( tinterval  PGNSP PGUID 12 f f t f i 2 704 f "702 702" _null_ _null_ _null_ mktinterval _null_ _null_ n 0));
DESCR("convert to tinterval");

/* lseg_ne(lseg, lseg) => bool */ 
DATA(insert OID = 1482 ( lseg_ne  PGNSP PGUID 12 f f t f i 2 16 f "601 601" _null_ _null_ _null_ lseg_ne _null_ _null_ n 0));
DESCR("not equal");

/* lseg_lt(lseg, lseg) => bool */ 
DATA(insert OID = 1483 ( lseg_lt  PGNSP PGUID 12 f f t f i 2 16 f "601 601" _null_ _null_ _null_ lseg_lt _null_ _null_ n 0));
DESCR("less-than by length");

/* lseg_le(lseg, lseg) => bool */ 
DATA(insert OID = 1484 ( lseg_le  PGNSP PGUID 12 f f t f i 2 16 f "601 601" _null_ _null_ _null_ lseg_le _null_ _null_ n 0));
DESCR("less-than-or-equal by length");

/* lseg_gt(lseg, lseg) => bool */ 
DATA(insert OID = 1485 ( lseg_gt  PGNSP PGUID 12 f f t f i 2 16 f "601 601" _null_ _null_ _null_ lseg_gt _null_ _null_ n 0));
DESCR("greater-than by length");

/* lseg_ge(lseg, lseg) => bool */ 
DATA(insert OID = 1486 ( lseg_ge  PGNSP PGUID 12 f f t f i 2 16 f "601 601" _null_ _null_ _null_ lseg_ge _null_ _null_ n 0));
DESCR("greater-than-or-equal by length");

/* lseg_length(lseg) => float8 */ 
DATA(insert OID = 1487 ( lseg_length  PGNSP PGUID 12 f f t f i 1 701 f "601" _null_ _null_ _null_ lseg_length _null_ _null_ n 0));
DESCR("distance between endpoints");

/* close_ls(line, lseg) => point */ 
DATA(insert OID = 1488 ( close_ls  PGNSP PGUID 12 f f t f i 2 600 f "628 601" _null_ _null_ _null_ close_ls _null_ _null_ n 0));
DESCR("closest point to line on line segment");

/* close_lseg(lseg, lseg) => point */ 
DATA(insert OID = 1489 ( close_lseg  PGNSP PGUID 12 f f t f i 2 600 f "601 601" _null_ _null_ _null_ close_lseg _null_ _null_ n 0));
DESCR("closest point to line segment on line segment");

/* line_in(cstring) => line */ 
DATA(insert OID = 1490 ( line_in  PGNSP PGUID 12 f f t f i 1 628 f "2275" _null_ _null_ _null_ line_in _null_ _null_ n 0));
DESCR("I/O");

/* line_out(line) => cstring */ 
DATA(insert OID = 1491 ( line_out  PGNSP PGUID 12 f f t f i 1 2275 f "628" _null_ _null_ _null_ line_out _null_ _null_ n 0));
DESCR("I/O");

/* line_eq(line, line) => bool */ 
DATA(insert OID = 1492 ( line_eq  PGNSP PGUID 12 f f t f i 2 16 f "628 628" _null_ _null_ _null_ line_eq _null_ _null_ n 0));
DESCR("lines equal?");

/* line(point, point) => line */ 
DATA(insert OID = 1493 ( line  PGNSP PGUID 12 f f t f i 2 628 f "600 600" _null_ _null_ _null_ line_construct_pp _null_ _null_ n 0));
DESCR("line from points");

/* line_interpt(line, line) => point */ 
DATA(insert OID = 1494 ( line_interpt  PGNSP PGUID 12 f f t f i 2 600 f "628 628" _null_ _null_ _null_ line_interpt _null_ _null_ n 0));
DESCR("intersection point");

/* line_intersect(line, line) => bool */ 
DATA(insert OID = 1495 ( line_intersect  PGNSP PGUID 12 f f t f i 2 16 f "628 628" _null_ _null_ _null_ line_intersect _null_ _null_ n 0));
DESCR("intersect?");

/* line_parallel(line, line) => bool */ 
DATA(insert OID = 1496 ( line_parallel  PGNSP PGUID 12 f f t f i 2 16 f "628 628" _null_ _null_ _null_ line_parallel _null_ _null_ n 0));
DESCR("parallel?");

/* line_perp(line, line) => bool */ 
DATA(insert OID = 1497 ( line_perp  PGNSP PGUID 12 f f t f i 2 16 f "628 628" _null_ _null_ _null_ line_perp _null_ _null_ n 0));
DESCR("perpendicular?");

/* line_vertical(line) => bool */ 
DATA(insert OID = 1498 ( line_vertical  PGNSP PGUID 12 f f t f i 1 16 f "628" _null_ _null_ _null_ line_vertical _null_ _null_ n 0));
DESCR("vertical?");

/* line_horizontal(line) => bool */ 
DATA(insert OID = 1499 ( line_horizontal  PGNSP PGUID 12 f f t f i 1 16 f "628" _null_ _null_ _null_ line_horizontal _null_ _null_ n 0));
DESCR("horizontal?");


/* OIDS 1500 - 1599 */
/* length(lseg) => float8 */ 
DATA(insert OID = 1530 ( length  PGNSP PGUID 12 f f t f i 1 701 f "601" _null_ _null_ _null_ lseg_length _null_ _null_ n 0));
DESCR("distance between endpoints");

/* length(path) => float8 */ 
DATA(insert OID = 1531 ( length  PGNSP PGUID 12 f f t f i 1 701 f "602" _null_ _null_ _null_ path_length _null_ _null_ n 0));
DESCR("sum of path segments");

/* point(lseg) => point */ 
DATA(insert OID = 1532 ( point  PGNSP PGUID 12 f f t f i 1 600 f "601" _null_ _null_ _null_ lseg_center _null_ _null_ n 0));
DESCR("center of");

/* point(path) => point */ 
DATA(insert OID = 1533 ( point  PGNSP PGUID 12 f f t f i 1 600 f "602" _null_ _null_ _null_ path_center _null_ _null_ n 0));
DESCR("center of");

/* point(box) => point */ 
DATA(insert OID = 1534 ( point  PGNSP PGUID 12 f f t f i 1 600 f "603" _null_ _null_ _null_ box_center _null_ _null_ n 0));
DESCR("center of");

/* point(polygon) => point */ 
DATA(insert OID = 1540 ( point  PGNSP PGUID 12 f f t f i 1 600 f "604" _null_ _null_ _null_ poly_center _null_ _null_ n 0));
DESCR("center of");

/* lseg(box) => lseg */ 
DATA(insert OID = 1541 ( lseg  PGNSP PGUID 12 f f t f i 1 601 f "603" _null_ _null_ _null_ box_diagonal _null_ _null_ n 0));
DESCR("diagonal of");

/* center(box) => point */ 
DATA(insert OID = 1542 ( center  PGNSP PGUID 12 f f t f i 1 600 f "603" _null_ _null_ _null_ box_center _null_ _null_ n 0));
DESCR("center of");

/* center(circle) => point */ 
DATA(insert OID = 1543 ( center  PGNSP PGUID 12 f f t f i 1 600 f "718" _null_ _null_ _null_ circle_center _null_ _null_ n 0));
DESCR("center of");

/* polygon(circle) => pg_catalog.polygon */ 
DATA(insert OID = 1544 ( polygon  PGNSP PGUID 14 f f t f i 1 604 f "718" _null_ _null_ _null_ "select pg_catalog.polygon(12, $1)" _null_ _null_ c 0));
DESCR("convert circle to 12-vertex polygon");

/* npoints(path) => int4 */ 
DATA(insert OID = 1545 ( npoints  PGNSP PGUID 12 f f t f i 1 23 f "602" _null_ _null_ _null_ path_npoints _null_ _null_ n 0));
DESCR("number of points in path");

/* npoints(polygon) => int4 */ 
DATA(insert OID = 1556 ( npoints  PGNSP PGUID 12 f f t f i 1 23 f "604" _null_ _null_ _null_ poly_npoints _null_ _null_ n 0));
DESCR("number of points in polygon");

/* bit_in(cstring, oid, int4) => "bit" */ 
DATA(insert OID = 1564 ( bit_in  PGNSP PGUID 12 f f t f i 3 1560 f "2275 26 23" _null_ _null_ _null_ bit_in _null_ _null_ n 0));
DESCR("I/O");

/* bit_out("bit") => cstring */ 
DATA(insert OID = 1565 ( bit_out  PGNSP PGUID 12 f f t f i 1 2275 f "1560" _null_ _null_ _null_ bit_out _null_ _null_ n 0));
DESCR("I/O");

/* "like"(text, text) => bool */ 
DATA(insert OID = 1569 ( like  PGNSP PGUID 12 f f t f i 2 16 f "25 25" _null_ _null_ _null_ textlike _null_ _null_ n 0));
DESCR("matches LIKE expression");

/* notlike(text, text) => bool */ 
DATA(insert OID = 1570 ( notlike  PGNSP PGUID 12 f f t f i 2 16 f "25 25" _null_ _null_ _null_ textnlike _null_ _null_ n 0));
DESCR("does not match LIKE expression");

/* "like"(name, text) => bool */ 
DATA(insert OID = 1571 ( like  PGNSP PGUID 12 f f t f i 2 16 f "19 25" _null_ _null_ _null_ namelike _null_ _null_ n 0));
DESCR("matches LIKE expression");

/* notlike(name, text) => bool */ 
DATA(insert OID = 1572 ( notlike  PGNSP PGUID 12 f f t f i 2 16 f "19 25" _null_ _null_ _null_ namenlike _null_ _null_ n 0));
DESCR("does not match LIKE expression");


/* SEQUENCE functions */
/* nextval(regclass) => int8 */ 
DATA(insert OID = 1574 ( nextval  PGNSP PGUID 12 f f t f v 1 20 f "2205" _null_ _null_ _null_ nextval_oid _null_ _null_ n 0));
DESCR("sequence next value");


#define NEXTVAL_FUNC_OID 1574
/* currval(regclass) => int8 */ 
DATA(insert OID = 1575 ( currval  PGNSP PGUID 12 f f t f v 1 20 f "2205" _null_ _null_ _null_ currval_oid _null_ _null_ n 0));
DESCR("sequence current value");


#define CURRVAL_FUNC_OID 1575
/* setval(regclass, int8) => int8 */ 
DATA(insert OID = 1576 ( setval  PGNSP PGUID 12 f f t f v 2 20 f "2205 20" _null_ _null_ _null_ setval_oid _null_ _null_ n 0));
DESCR("set sequence value");


#define SETVAL_FUNC_OID 1576
/* setval(regclass, int8, bool) => int8 */ 
DATA(insert OID = 1765 ( setval  PGNSP PGUID 12 f f t f v 3 20 f "2205 20 16" _null_ _null_ _null_ setval3_oid _null_ _null_ n 0));
DESCR("set sequence value and iscalled status");

/* varbit_in(cstring, oid, int4) => varbit */ 
DATA(insert OID = 1579 ( varbit_in  PGNSP PGUID 12 f f t f i 3 1562 f "2275 26 23" _null_ _null_ _null_ varbit_in _null_ _null_ n 0));
DESCR("I/O");

/* varbit_out(varbit) => cstring */ 
DATA(insert OID = 1580 ( varbit_out  PGNSP PGUID 12 f f t f i 1 2275 f "1562" _null_ _null_ _null_ varbit_out _null_ _null_ n 0));
DESCR("I/O");

/* biteq("bit", "bit") => bool */ 
DATA(insert OID = 1581 ( biteq  PGNSP PGUID 12 f f t f i 2 16 f "1560 1560" _null_ _null_ _null_ biteq _null_ _null_ n 0));
DESCR("equal");

/* bitne("bit", "bit") => bool */ 
DATA(insert OID = 1582 ( bitne  PGNSP PGUID 12 f f t f i 2 16 f "1560 1560" _null_ _null_ _null_ bitne _null_ _null_ n 0));
DESCR("not equal");

/* bitge("bit", "bit") => bool */ 
DATA(insert OID = 1592 ( bitge  PGNSP PGUID 12 f f t f i 2 16 f "1560 1560" _null_ _null_ _null_ bitge _null_ _null_ n 0));
DESCR("greater than or equal");

/* bitgt("bit", "bit") => bool */ 
DATA(insert OID = 1593 ( bitgt  PGNSP PGUID 12 f f t f i 2 16 f "1560 1560" _null_ _null_ _null_ bitgt _null_ _null_ n 0));
DESCR("greater than");

/* bitle("bit", "bit") => bool */ 
DATA(insert OID = 1594 ( bitle  PGNSP PGUID 12 f f t f i 2 16 f "1560 1560" _null_ _null_ _null_ bitle _null_ _null_ n 0));
DESCR("less than or equal");

/* bitlt("bit", "bit") => bool */ 
DATA(insert OID = 1595 ( bitlt  PGNSP PGUID 12 f f t f i 2 16 f "1560 1560" _null_ _null_ _null_ bitlt _null_ _null_ n 0));
DESCR("less than");

/* bitcmp("bit", "bit") => int4 */ 
DATA(insert OID = 1596 ( bitcmp  PGNSP PGUID 12 f f t f i 2 23 f "1560 1560" _null_ _null_ _null_ bitcmp _null_ _null_ n 0));
DESCR("compare");

/* random() => float8 */ 
DATA(insert OID = 1598 ( random  PGNSP PGUID 12 f f t f v 0 701 f "" _null_ _null_ _null_ drandom _null_ _null_ n 0));
DESCR("random value");

/* setseed(float8) => int4 */ 
DATA(insert OID = 1599 ( setseed  PGNSP PGUID 12 f f t f v 1 23 f "701" _null_ _null_ _null_ setseed _null_ _null_ n 0));
DESCR("set random seed");


/* OIDS 1600 - 1699 */
/* asin(float8) => float8 */ 
DATA(insert OID = 1600 ( asin  PGNSP PGUID 12 f f t f i 1 701 f "701" _null_ _null_ _null_ dasin _null_ _null_ n 0));
DESCR("arcsine");

/* acos(float8) => float8 */ 
DATA(insert OID = 1601 ( acos  PGNSP PGUID 12 f f t f i 1 701 f "701" _null_ _null_ _null_ dacos _null_ _null_ n 0));
DESCR("arccosine");

/* atan(float8) => float8 */ 
DATA(insert OID = 1602 ( atan  PGNSP PGUID 12 f f t f i 1 701 f "701" _null_ _null_ _null_ datan _null_ _null_ n 0));
DESCR("arctangent");

/* atan2(float8, float8) => float8 */ 
DATA(insert OID = 1603 ( atan2  PGNSP PGUID 12 f f t f i 2 701 f "701 701" _null_ _null_ _null_ datan2 _null_ _null_ n 0));
DESCR("arctangent, two arguments");

/* sin(float8) => float8 */ 
DATA(insert OID = 1604 ( sin  PGNSP PGUID 12 f f t f i 1 701 f "701" _null_ _null_ _null_ dsin _null_ _null_ n 0));
DESCR("sine");

/* cos(float8) => float8 */ 
DATA(insert OID = 1605 ( cos  PGNSP PGUID 12 f f t f i 1 701 f "701" _null_ _null_ _null_ dcos _null_ _null_ n 0));
DESCR("cosine");

/* tan(float8) => float8 */ 
DATA(insert OID = 1606 ( tan  PGNSP PGUID 12 f f t f i 1 701 f "701" _null_ _null_ _null_ dtan _null_ _null_ n 0));
DESCR("tangent");

/* cot(float8) => float8 */ 
DATA(insert OID = 1607 ( cot  PGNSP PGUID 12 f f t f i 1 701 f "701" _null_ _null_ _null_ dcot _null_ _null_ n 0));
DESCR("cotangent");

/* degrees(float8) => float8 */ 
DATA(insert OID = 1608 ( degrees  PGNSP PGUID 12 f f t f i 1 701 f "701" _null_ _null_ _null_ degrees _null_ _null_ n 0));
DESCR("radians to degrees");

/* radians(float8) => float8 */ 
DATA(insert OID = 1609 ( radians  PGNSP PGUID 12 f f t f i 1 701 f "701" _null_ _null_ _null_ radians _null_ _null_ n 0));
DESCR("degrees to radians");

/* pi() => float8 */ 
DATA(insert OID = 1610 ( pi  PGNSP PGUID 12 f f t f i 0 701 f "" _null_ _null_ _null_ dpi _null_ _null_ n 0));
DESCR("PI");

/* interval_mul("interval", float8) => "interval" */ 
DATA(insert OID = 1618 ( interval_mul  PGNSP PGUID 12 f f t f i 2 1186 f "1186 701" _null_ _null_ _null_ interval_mul _null_ _null_ n 0));
DESCR("multiply interval");

/* ascii(text) => int4 */ 
DATA(insert OID = 1620 ( ascii  PGNSP PGUID 12 f f t f i 1 23 f "25" _null_ _null_ _null_ ascii _null_ _null_ n 0));
DESCR("convert first char to int4");

/* chr(int4) => text */ 
DATA(insert OID = 1621 ( chr  PGNSP PGUID 12 f f t f i 1 25 f "23" _null_ _null_ _null_ chr _null_ _null_ n 0));
DESCR("convert int4 to char");

/* repeat(text, int4) => text */ 
DATA(insert OID = 1622 ( repeat  PGNSP PGUID 12 f f t f i 2 25 f "25 23" _null_ _null_ _null_ repeat _null_ _null_ n 0));
DESCR("replicate string int4 times");

/* similar_escape(text, text) => text */ 
DATA(insert OID = 1623 ( similar_escape  PGNSP PGUID 12 f f f f i 2 25 f "25 25" _null_ _null_ _null_ similar_escape _null_ _null_ n 0));
DESCR("convert SQL99 regexp pattern to POSIX style");

/* mul_d_interval(float8, "interval") => "interval" */ 
DATA(insert OID = 1624 ( mul_d_interval  PGNSP PGUID 12 f f t f i 2 1186 f "701 1186" _null_ _null_ _null_ mul_d_interval _null_ _null_ n 0));

/* bpcharlike(bpchar, text) => bool */ 
DATA(insert OID = 1631 ( bpcharlike  PGNSP PGUID 12 f f t f i 2 16 f "1042 25" _null_ _null_ _null_ textlike _null_ _null_ n 0));
DESCR("matches LIKE expression");

/* bpcharnlike(bpchar, text) => bool */ 
DATA(insert OID = 1632 ( bpcharnlike  PGNSP PGUID 12 f f t f i 2 16 f "1042 25" _null_ _null_ _null_ textnlike _null_ _null_ n 0));
DESCR("does not match LIKE expression");

/* texticlike(text, text) => bool */ 
DATA(insert OID = 1633 ( texticlike  PGNSP PGUID 12 f f t f i 2 16 f "25 25" _null_ _null_ _null_ texticlike _null_ _null_ n 0));
DESCR("matches LIKE expression, case-insensitive");

/* texticnlike(text, text) => bool */ 
DATA(insert OID = 1634 ( texticnlike  PGNSP PGUID 12 f f t f i 2 16 f "25 25" _null_ _null_ _null_ texticnlike _null_ _null_ n 0));
DESCR("does not match LIKE expression, case-insensitive");

/* nameiclike(name, text) => bool */ 
DATA(insert OID = 1635 ( nameiclike  PGNSP PGUID 12 f f t f i 2 16 f "19 25" _null_ _null_ _null_ nameiclike _null_ _null_ n 0));
DESCR("matches LIKE expression, case-insensitive");

/* nameicnlike(name, text) => bool */ 
DATA(insert OID = 1636 ( nameicnlike  PGNSP PGUID 12 f f t f i 2 16 f "19 25" _null_ _null_ _null_ nameicnlike _null_ _null_ n 0));
DESCR("does not match LIKE expression, case-insensitive");

/* like_escape(text, text) => text */ 
DATA(insert OID = 1637 ( like_escape  PGNSP PGUID 12 f f t f i 2 25 f "25 25" _null_ _null_ _null_ like_escape _null_ _null_ n 0));
DESCR("convert LIKE pattern to use backslash escapes");

/* bpcharicregexeq(bpchar, text) => bool */ 
DATA(insert OID = 1656 ( bpcharicregexeq  PGNSP PGUID 12 f f t f i 2 16 f "1042 25" _null_ _null_ _null_ texticregexeq _null_ _null_ n 0));
DESCR("matches regex., case-insensitive");

/* bpcharicregexne(bpchar, text) => bool */ 
DATA(insert OID = 1657 ( bpcharicregexne  PGNSP PGUID 12 f f t f i 2 16 f "1042 25" _null_ _null_ _null_ texticregexne _null_ _null_ n 0));
DESCR("does not match regex., case-insensitive");

/* bpcharregexeq(bpchar, text) => bool */ 
DATA(insert OID = 1658 ( bpcharregexeq  PGNSP PGUID 12 f f t f i 2 16 f "1042 25" _null_ _null_ _null_ textregexeq _null_ _null_ n 0));
DESCR("matches regex., case-sensitive");

/* bpcharregexne(bpchar, text) => bool */ 
DATA(insert OID = 1659 ( bpcharregexne  PGNSP PGUID 12 f f t f i 2 16 f "1042 25" _null_ _null_ _null_ textregexne _null_ _null_ n 0));
DESCR("does not match regex., case-sensitive");

/* bpchariclike(bpchar, text) => bool */ 
DATA(insert OID = 1660 ( bpchariclike  PGNSP PGUID 12 f f t f i 2 16 f "1042 25" _null_ _null_ _null_ texticlike _null_ _null_ n 0));
DESCR("matches LIKE expression, case-insensitive");

/* bpcharicnlike(bpchar, text) => bool */ 
DATA(insert OID = 1661 ( bpcharicnlike  PGNSP PGUID 12 f f t f i 2 16 f "1042 25" _null_ _null_ _null_ texticnlike _null_ _null_ n 0));
DESCR("does not match LIKE expression, case-insensitive");

/* flatfile_update_trigger() => trigger */ 
DATA(insert OID = 1689 ( flatfile_update_trigger  PGNSP PGUID 12 f f t f v 0 2279 f "" _null_ _null_ _null_ flatfile_update_trigger _null_ _null_ n 0));
DESCR("update flat-file copy of a shared catalog");


/* Oracle Compatibility Related Functions - By Edmund Mergl <E.Mergl@bawue.de>  */
/* strpos(text, text) => int4 */ 
DATA(insert OID = 868 ( strpos  PGNSP PGUID 12 f f t f i 2 23 f "25 25" _null_ _null_ _null_ textpos _null_ _null_ n 0));
DESCR("find position of substring");

/* lower(text) => text */ 
DATA(insert OID = 870 ( lower  PGNSP PGUID 12 f f t f i 1 25 f "25" _null_ _null_ _null_ lower _null_ _null_ n 0));
DESCR("lowercase");

/* upper(text) => text */ 
DATA(insert OID = 871 ( upper  PGNSP PGUID 12 f f t f i 1 25 f "25" _null_ _null_ _null_ upper _null_ _null_ n 0));
DESCR("uppercase");

/* initcap(text) => text */ 
DATA(insert OID = 872 ( initcap  PGNSP PGUID 12 f f t f i 1 25 f "25" _null_ _null_ _null_ initcap _null_ _null_ n 0));
DESCR("capitalize each word");

/* lpad(text, int4, text) => text */ 
DATA(insert OID = 873 ( lpad  PGNSP PGUID 12 f f t f i 3 25 f "25 23 25" _null_ _null_ _null_ lpad _null_ _null_ n 0));
DESCR("left-pad string to length");

/* rpad(text, int4, text) => text */ 
DATA(insert OID = 874 ( rpad  PGNSP PGUID 12 f f t f i 3 25 f "25 23 25" _null_ _null_ _null_ rpad _null_ _null_ n 0));
DESCR("right-pad string to length");

/* ltrim(text, text) => text */ 
DATA(insert OID = 875 ( ltrim  PGNSP PGUID 12 f f t f i 2 25 f "25 25" _null_ _null_ _null_ ltrim _null_ _null_ n 0));
DESCR("trim selected characters from left end of string");

/* rtrim(text, text) => text */ 
DATA(insert OID = 876 ( rtrim  PGNSP PGUID 12 f f t f i 2 25 f "25 25" _null_ _null_ _null_ rtrim _null_ _null_ n 0));
DESCR("trim selected characters from right end of string");

/* substr(text, int4, int4) => text */ 
DATA(insert OID = 877 ( substr  PGNSP PGUID 12 f f t f i 3 25 f "25 23 23" _null_ _null_ _null_ text_substr _null_ _null_ n 0));
DESCR("return portion of string");

/* translate(text, text, text) => text */ 
DATA(insert OID = 878 ( translate  PGNSP PGUID 12 f f t f i 3 25 f "25 25 25" _null_ _null_ _null_ translate _null_ _null_ n 0));
DESCR("map a set of character appearing in string");

/* lpad(text, int4) => pg_catalog.text */ 
DATA(insert OID = 879 ( lpad  PGNSP PGUID 14 f f t f i 2 25 f "25 23" _null_ _null_ _null_ "select pg_catalog.lpad($1, $2, '' '')" _null_ _null_ c 0));
DESCR("left-pad string to length");

/* rpad(text, int4) => pg_catalog.text */ 
DATA(insert OID = 880 ( rpad  PGNSP PGUID 14 f f t f i 2 25 f "25 23" _null_ _null_ _null_ "select pg_catalog.rpad($1, $2, '' '')" _null_ _null_ c 0));
DESCR("right-pad string to length");

/* ltrim(text) => text */ 
DATA(insert OID = 881 ( ltrim  PGNSP PGUID 12 f f t f i 1 25 f "25" _null_ _null_ _null_ ltrim1 _null_ _null_ n 0));
DESCR("trim spaces from left end of string");

/* rtrim(text) => text */ 
DATA(insert OID = 882 ( rtrim  PGNSP PGUID 12 f f t f i 1 25 f "25" _null_ _null_ _null_ rtrim1 _null_ _null_ n 0));
DESCR("trim spaces from right end of string");

/* substr(text, int4) => text */ 
DATA(insert OID = 883 ( substr  PGNSP PGUID 12 f f t f i 2 25 f "25 23" _null_ _null_ _null_ text_substr_no_len _null_ _null_ n 0));
DESCR("return portion of string");

/* btrim(text, text) => text */ 
DATA(insert OID = 884 ( btrim  PGNSP PGUID 12 f f t f i 2 25 f "25 25" _null_ _null_ _null_ btrim _null_ _null_ n 0));
DESCR("trim selected characters from both ends of string");

/* btrim(text) => text */ 
DATA(insert OID = 885 ( btrim  PGNSP PGUID 12 f f t f i 1 25 f "25" _null_ _null_ _null_ btrim1 _null_ _null_ n 0));
DESCR("trim spaces from both ends of string");

/* "substring"(text, int4, int4) => text */ 
DATA(insert OID = 936 ( substring  PGNSP PGUID 12 f f t f i 3 25 f "25 23 23" _null_ _null_ _null_ text_substr _null_ _null_ n 0));
DESCR("return portion of string");

/* "substring"(text, int4) => text */ 
DATA(insert OID = 937 ( substring  PGNSP PGUID 12 f f t f i 2 25 f "25 23" _null_ _null_ _null_ text_substr_no_len _null_ _null_ n 0));
DESCR("return portion of string");

/* replace(text, text, text) => text */ 
DATA(insert OID = 2087 ( replace  PGNSP PGUID 12 f f t f i 3 25 f "25 25 25" _null_ _null_ _null_ replace_text _null_ _null_ n 0));
DESCR("replace all occurrences of old_substr with new_substr in string");

/* regexp_replace(text, text, text) => text */ 
DATA(insert OID = 2284 ( regexp_replace  PGNSP PGUID 12 f f t f i 3 25 f "25 25 25" _null_ _null_ _null_ textregexreplace_noopt _null_ _null_ n 0));
DESCR("replace text using regexp");

/* regexp_replace(text, text, text, text) => text */ 
DATA(insert OID = 2285 ( regexp_replace  PGNSP PGUID 12 f f t f i 4 25 f "25 25 25 25" _null_ _null_ _null_ textregexreplace _null_ _null_ n 0));
DESCR("replace text using regexp");

/* regexp_matches(text, text) => SETOF _text */ 
DATA(insert OID = 5018 ( regexp_matches  PGNSP PGUID 12 f f t t i 2 1009 f "25 25" _null_ _null_ _null_ regexp_matches_no_flags _null_ _null_ n 0));
DESCR("return all match groups for regexp");

/* regexp_matches(text, text, text) => SETOF _text */ 
DATA(insert OID = 5019 ( regexp_matches  PGNSP PGUID 12 f f t t i 3 1009 f "25 25 25" _null_ _null_ _null_ regexp_matches _null_ _null_ n 0));
DESCR("return all match groups for regexp");

/* regexp_split_to_table(text, text) => SETOF text */ 
DATA(insert OID = 5020 ( regexp_split_to_table  PGNSP PGUID 12 f f t t i 2 25 f "25 25" _null_ _null_ _null_ regexp_split_to_table_no_flags _null_ _null_ n 0));
DESCR("split string by pattern");

/* regexp_split_to_table(text, text, text) => SETOF text */ 
DATA(insert OID = 5021 ( regexp_split_to_table  PGNSP PGUID 12 f f t t i 3 25 f "25 25 25" _null_ _null_ _null_ regexp_split_to_table _null_ _null_ n 0));
DESCR("split string by pattern");

/* regexp_split_to_array(text, text) => _text */ 
DATA(insert OID = 5022 ( regexp_split_to_array  PGNSP PGUID 12 f f t f i 2 1009 f "25 25" _null_ _null_ _null_ regexp_split_to_array_no_flags _null_ _null_ n 0));
DESCR("split string by pattern");

/* regexp_split_to_array(text, text, text) => _text */ 
DATA(insert OID = 5023 ( regexp_split_to_array  PGNSP PGUID 12 f f t f i 3 1009 f "25 25 25" _null_ _null_ _null_ regexp_split_to_array _null_ _null_ n 0));
DESCR("split string by pattern");

/* split_part(text, text, int4) => text */ 
DATA(insert OID = 2088 ( split_part  PGNSP PGUID 12 f f t f i 3 25 f "25 25 23" _null_ _null_ _null_ split_text _null_ _null_ n 0));
DESCR("split string by field_sep and return field_num");

/* to_hex(int4) => text */ 
DATA(insert OID = 2089 ( to_hex  PGNSP PGUID 12 f f t f i 1 25 f "23" _null_ _null_ _null_ to_hex32 _null_ _null_ n 0));
DESCR("convert int4 number to hex");

/* to_hex(int8) => text */ 
DATA(insert OID = 2090 ( to_hex  PGNSP PGUID 12 f f t f i 1 25 f "20" _null_ _null_ _null_ to_hex64 _null_ _null_ n 0));
DESCR("convert int8 number to hex");


/*  for character set encoding support  */

/* return database encoding name  */
/* getdatabaseencoding() => name */ 
DATA(insert OID = 1039 ( getdatabaseencoding  PGNSP PGUID 12 f f t f s 0 19 f "" _null_ _null_ _null_ getdatabaseencoding _null_ _null_ n 0));
DESCR("encoding name of current database");


/* return client encoding name i.e. session encoding  */
/* pg_client_encoding() => name */ 
DATA(insert OID = 810 ( pg_client_encoding  PGNSP PGUID 12 f f t f s 0 19 f "" _null_ _null_ _null_ pg_client_encoding _null_ _null_ n 0));
DESCR("encoding name of current database");

/* "convert"(text, name) => text */ 
DATA(insert OID = 1717 ( convert  PGNSP PGUID 12 f f t f s 2 25 f "25 19" _null_ _null_ _null_ pg_convert _null_ _null_ n 0));
DESCR("convert string with specified destination encoding name");

/* "convert"(text, name, name) => text */ 
DATA(insert OID = 1813 ( convert  PGNSP PGUID 12 f f t f s 3 25 f "25 19 19" _null_ _null_ _null_ pg_convert2 _null_ _null_ n 0));
DESCR("convert string with specified encoding names");

/* convert_using(text, text) => text */ 
DATA(insert OID = 1619 ( convert_using  PGNSP PGUID 12 f f t f s 2 25 f "25 25" _null_ _null_ _null_ pg_convert_using _null_ _null_ n 0));
DESCR("convert string with specified conversion name");

/* pg_char_to_encoding(name) => int4 */ 
DATA(insert OID = 1264 ( pg_char_to_encoding  PGNSP PGUID 12 f f t f s 1 23 f "19" _null_ _null_ _null_ PG_char_to_encoding _null_ _null_ n 0));
DESCR("convert encoding name to encoding id");

/* pg_encoding_to_char(int4) => name */ 
DATA(insert OID = 1597 ( pg_encoding_to_char  PGNSP PGUID 12 f f t f s 1 19 f "23" _null_ _null_ _null_ PG_encoding_to_char _null_ _null_ n 0));
DESCR("convert encoding id to encoding name");

/* oidgt(oid, oid) => bool */ 
DATA(insert OID = 1638 ( oidgt  PGNSP PGUID 12 f f t f i 2 16 f "26 26" _null_ _null_ _null_ oidgt _null_ _null_ n 0));
DESCR("greater-than");

/* oidge(oid, oid) => bool */ 
DATA(insert OID = 1639 ( oidge  PGNSP PGUID 12 f f t f i 2 16 f "26 26" _null_ _null_ _null_ oidge _null_ _null_ n 0));
DESCR("greater-than-or-equal");


/* System-view support functions  */
/* pg_get_ruledef(oid) => text */ 
DATA(insert OID = 1573 ( pg_get_ruledef  PGNSP PGUID 12 f f t f s 1 25 f "26" _null_ _null_ _null_ pg_get_ruledef _null_ _null_ n 0));
DESCR("source text of a rule");

/* pg_get_viewdef(text) => text */ 
DATA(insert OID = 1640 ( pg_get_viewdef  PGNSP PGUID 12 f f t f s 1 25 f "25" _null_ _null_ _null_ pg_get_viewdef_name _null_ _null_ n 0));
DESCR("select statement of a view");

/* pg_get_viewdef(oid) => text */ 
DATA(insert OID = 1641 ( pg_get_viewdef  PGNSP PGUID 12 f f t f s 1 25 f "26" _null_ _null_ _null_ pg_get_viewdef _null_ _null_ n 0));
DESCR("select statement of a view");

/* pg_get_userbyid(oid) => name */ 
DATA(insert OID = 1642 ( pg_get_userbyid  PGNSP PGUID 12 f f t f s 1 19 f "26" _null_ _null_ _null_ pg_get_userbyid _null_ _null_ n 0));
DESCR("role name by OID (with fallback)");

/* pg_get_indexdef(oid) => text */ 
DATA(insert OID = 1643 ( pg_get_indexdef  PGNSP PGUID 12 f f t f s 1 25 f "26" _null_ _null_ _null_ pg_get_indexdef _null_ _null_ n 0));
DESCR("index description");

/* pg_get_triggerdef(oid) => text */ 
DATA(insert OID = 1662 ( pg_get_triggerdef  PGNSP PGUID 12 f f t f s 1 25 f "26" _null_ _null_ _null_ pg_get_triggerdef _null_ _null_ n 0));
DESCR("trigger description");

/* pg_get_constraintdef(oid) => text */ 
DATA(insert OID = 1387 ( pg_get_constraintdef  PGNSP PGUID 12 f f t f s 1 25 f "26" _null_ _null_ _null_ pg_get_constraintdef _null_ _null_ n 0));
DESCR("constraint description");

/* pg_get_expr(text, oid) => text */ 
DATA(insert OID = 1716 ( pg_get_expr  PGNSP PGUID 12 f f t f s 2 25 f "25 26" _null_ _null_ _null_ pg_get_expr _null_ _null_ n 0));
DESCR("deparse an encoded expression");

/* pg_get_serial_sequence(text, text) => text */ 
DATA(insert OID = 1665 ( pg_get_serial_sequence  PGNSP PGUID 12 f f t f s 2 25 f "25 25" _null_ _null_ _null_ pg_get_serial_sequence _null_ _null_ n 0));
DESCR("name of sequence for a serial column");

/* pg_get_partition_def(oid) => text */ 
DATA(insert OID = 5024 ( pg_get_partition_def  PGNSP PGUID 12 f f t f s 1 25 f "26" _null_ _null_ _null_ pg_get_partition_def _null_ _null_ n 0));

/* pg_get_partition_def(oid, bool) => text */ 
DATA(insert OID = 5025 ( pg_get_partition_def  PGNSP PGUID 12 f f t f s 2 25 f "26 16" _null_ _null_ _null_ pg_get_partition_def_ext _null_ _null_ n 0));
DESCR("partition configuration for a given relation");

/* pg_get_partition_def(oid, bool, bool) => text */ 
DATA(insert OID = 5034 ( pg_get_partition_def  PGNSP PGUID 12 f f t f s 3 25 f "26 16 16" _null_ _null_ _null_ pg_get_partition_def_ext2 _null_ _null_ n 0));
DESCR("partition configuration for a given relation");

/* pg_get_partition_rule_def(oid) => text */ 
DATA(insert OID = 5027 ( pg_get_partition_rule_def  PGNSP PGUID 12 f f t f s 1 25 f "26" _null_ _null_ _null_ pg_get_partition_rule_def _null_ _null_ n 0));

/* pg_get_partition_rule_def(oid, bool) => text */ 
DATA(insert OID = 5028 ( pg_get_partition_rule_def  PGNSP PGUID 12 f f t f s 2 25 f "26 16" _null_ _null_ _null_ pg_get_partition_rule_def_ext _null_ _null_ n 0));
DESCR("partition configuration for a given rule");

/* pg_get_partition_template_def(oid, bool, bool) => text */ 
DATA(insert OID = 5037 ( pg_get_partition_template_def  PGNSP PGUID 12 f f t f s 3 25 f "26 16 16" _null_ _null_ _null_ pg_get_partition_template_def _null_ _null_ n 0));
DESCR("ALTER statement to recreate subpartition templates for a give relation");

/* pg_get_keywords(OUT word text, OUT catcode "char", OUT catdesc text) => SETOF pg_catalog.record */ 
DATA(insert OID = 821 ( pg_get_keywords  PGNSP PGUID 12 f f t t s 0 2249 f "" "{25,18,25}" "{o,o,o}" "{word,catcode,catdesc}" pg_get_keywords _null_ _null_ n 0));
DESCR("list of SQL keywords");

/* pg_typeof("any") => regtype */ 
DATA(insert OID = 822 ( pg_typeof  PGNSP PGUID 12 f f f f s 1 2206 f "2276" _null_ _null_ _null_ pg_typeof _null_ _null_ n 0));
DESCR("returns the type of the argument");


/* Generic referential integrity constraint triggers  */
/* "RI_FKey_check_ins"() => trigger */ 
DATA(insert OID = 1644 ( RI_FKey_check_ins  PGNSP PGUID 12 f f t f v 0 2279 f "" _null_ _null_ _null_ RI_FKey_check_ins _null_ _null_ m 0));
DESCR("referential integrity FOREIGN KEY ... REFERENCES");

/* "RI_FKey_check_upd"() => trigger */ 
DATA(insert OID = 1645 ( RI_FKey_check_upd  PGNSP PGUID 12 f f t f v 0 2279 f "" _null_ _null_ _null_ RI_FKey_check_upd _null_ _null_ m 0));
DESCR("referential integrity FOREIGN KEY ... REFERENCES");

/* "RI_FKey_cascade_del"() => trigger */ 
DATA(insert OID = 1646 ( RI_FKey_cascade_del  PGNSP PGUID 12 f f t f v 0 2279 f "" _null_ _null_ _null_ RI_FKey_cascade_del _null_ _null_ m 0));
DESCR("referential integrity ON DELETE CASCADE");

/* "RI_FKey_cascade_upd"() => trigger */ 
DATA(insert OID = 1647 ( RI_FKey_cascade_upd  PGNSP PGUID 12 f f t f v 0 2279 f "" _null_ _null_ _null_ RI_FKey_cascade_upd _null_ _null_ m 0));
DESCR("referential integrity ON UPDATE CASCADE");

/* "RI_FKey_restrict_del"() => trigger */ 
DATA(insert OID = 1648 ( RI_FKey_restrict_del  PGNSP PGUID 12 f f t f v 0 2279 f "" _null_ _null_ _null_ RI_FKey_restrict_del _null_ _null_ m 0));
DESCR("referential integrity ON DELETE RESTRICT");

/* "RI_FKey_restrict_upd"() => trigger */ 
DATA(insert OID = 1649 ( RI_FKey_restrict_upd  PGNSP PGUID 12 f f t f v 0 2279 f "" _null_ _null_ _null_ RI_FKey_restrict_upd _null_ _null_ m 0));
DESCR("referential integrity ON UPDATE RESTRICT");

/* "RI_FKey_setnull_del"() => trigger */ 
DATA(insert OID = 1650 ( RI_FKey_setnull_del  PGNSP PGUID 12 f f t f v 0 2279 f "" _null_ _null_ _null_ RI_FKey_setnull_del _null_ _null_ m 0));
DESCR("referential integrity ON DELETE SET NULL");

/* "RI_FKey_setnull_upd"() => trigger */ 
DATA(insert OID = 1651 ( RI_FKey_setnull_upd  PGNSP PGUID 12 f f t f v 0 2279 f "" _null_ _null_ _null_ RI_FKey_setnull_upd _null_ _null_ m 0));
DESCR("referential integrity ON UPDATE SET NULL");

/* "RI_FKey_setdefault_del"() => trigger */ 
DATA(insert OID = 1652 ( RI_FKey_setdefault_del  PGNSP PGUID 12 f f t f v 0 2279 f "" _null_ _null_ _null_ RI_FKey_setdefault_del _null_ _null_ m 0));
DESCR("referential integrity ON DELETE SET DEFAULT");

/* "RI_FKey_setdefault_upd"() => trigger */ 
DATA(insert OID = 1653 ( RI_FKey_setdefault_upd  PGNSP PGUID 12 f f t f v 0 2279 f "" _null_ _null_ _null_ RI_FKey_setdefault_upd _null_ _null_ m 0));
DESCR("referential integrity ON UPDATE SET DEFAULT");

/* "RI_FKey_noaction_del"() => trigger */ 
DATA(insert OID = 1654 ( RI_FKey_noaction_del  PGNSP PGUID 12 f f t f v 0 2279 f "" _null_ _null_ _null_ RI_FKey_noaction_del _null_ _null_ m 0));
DESCR("referential integrity ON DELETE NO ACTION");

/* "RI_FKey_noaction_upd"() => trigger */ 
DATA(insert OID = 1655 ( RI_FKey_noaction_upd  PGNSP PGUID 12 f f t f v 0 2279 f "" _null_ _null_ _null_ RI_FKey_noaction_upd _null_ _null_ m 0));
DESCR("referential integrity ON UPDATE NO ACTION");

/* varbiteq(varbit, varbit) => bool */ 
DATA(insert OID = 1666 ( varbiteq  PGNSP PGUID 12 f f t f i 2 16 f "1562 1562" _null_ _null_ _null_ biteq _null_ _null_ n 0));
DESCR("equal");

/* varbitne(varbit, varbit) => bool */ 
DATA(insert OID = 1667 ( varbitne  PGNSP PGUID 12 f f t f i 2 16 f "1562 1562" _null_ _null_ _null_ bitne _null_ _null_ n 0));
DESCR("not equal");

/* varbitge(varbit, varbit) => bool */ 
DATA(insert OID = 1668 ( varbitge  PGNSP PGUID 12 f f t f i 2 16 f "1562 1562" _null_ _null_ _null_ bitge _null_ _null_ n 0));
DESCR("greater than or equal");

/* varbitgt(varbit, varbit) => bool */ 
DATA(insert OID = 1669 ( varbitgt  PGNSP PGUID 12 f f t f i 2 16 f "1562 1562" _null_ _null_ _null_ bitgt _null_ _null_ n 0));
DESCR("greater than");

/* varbitle(varbit, varbit) => bool */ 
DATA(insert OID = 1670 ( varbitle  PGNSP PGUID 12 f f t f i 2 16 f "1562 1562" _null_ _null_ _null_ bitle _null_ _null_ n 0));
DESCR("less than or equal");

/* varbitlt(varbit, varbit) => bool */ 
DATA(insert OID = 1671 ( varbitlt  PGNSP PGUID 12 f f t f i 2 16 f "1562 1562" _null_ _null_ _null_ bitlt _null_ _null_ n 0));
DESCR("less than");

/* varbitcmp(varbit, varbit) => int4 */ 
DATA(insert OID = 1672 ( varbitcmp  PGNSP PGUID 12 f f t f i 2 23 f "1562 1562" _null_ _null_ _null_ bitcmp _null_ _null_ n 0));
DESCR("compare");

/* bitand("bit", "bit") => "bit" */ 
DATA(insert OID = 1673 ( bitand  PGNSP PGUID 12 f f t f i 2 1560 f "1560 1560" _null_ _null_ _null_ bitand _null_ _null_ n 0));
DESCR("bitwise and");

/* bitor("bit", "bit") => "bit" */ 
DATA(insert OID = 1674 ( bitor  PGNSP PGUID 12 f f t f i 2 1560 f "1560 1560" _null_ _null_ _null_ bitor _null_ _null_ n 0));
DESCR("bitwise or");

/* bitxor("bit", "bit") => "bit" */ 
DATA(insert OID = 1675 ( bitxor  PGNSP PGUID 12 f f t f i 2 1560 f "1560 1560" _null_ _null_ _null_ bitxor _null_ _null_ n 0));
DESCR("bitwise exclusive or");

/* bitnot("bit") => "bit" */ 
DATA(insert OID = 1676 ( bitnot  PGNSP PGUID 12 f f t f i 1 1560 f "1560" _null_ _null_ _null_ bitnot _null_ _null_ n 0));
DESCR("bitwise not");

/* bitshiftleft("bit", int4) => "bit" */ 
DATA(insert OID = 1677 ( bitshiftleft  PGNSP PGUID 12 f f t f i 2 1560 f "1560 23" _null_ _null_ _null_ bitshiftleft _null_ _null_ n 0));
DESCR("bitwise left shift");

/* bitshiftright("bit", int4) => "bit" */ 
DATA(insert OID = 1678 ( bitshiftright  PGNSP PGUID 12 f f t f i 2 1560 f "1560 23" _null_ _null_ _null_ bitshiftright _null_ _null_ n 0));
DESCR("bitwise right shift");

/* bitcat("bit", "bit") => "bit" */ 
DATA(insert OID = 1679 ( bitcat  PGNSP PGUID 12 f f t f i 2 1560 f "1560 1560" _null_ _null_ _null_ bitcat _null_ _null_ n 0));
DESCR("bitwise concatenation");

/* "substring"("bit", int4, int4) => "bit" */ 
DATA(insert OID = 1680 ( substring  PGNSP PGUID 12 f f t f i 3 1560 f "1560 23 23" _null_ _null_ _null_ bitsubstr _null_ _null_ n 0));
DESCR("return portion of bitstring");

/* length("bit") => int4 */ 
DATA(insert OID = 1681 ( length  PGNSP PGUID 12 f f t f i 1 23 f "1560" _null_ _null_ _null_ bitlength _null_ _null_ n 0));
DESCR("bitstring length");

/* octet_length("bit") => int4 */ 
DATA(insert OID = 1682 ( octet_length  PGNSP PGUID 12 f f t f i 1 23 f "1560" _null_ _null_ _null_ bitoctetlength _null_ _null_ n 0));
DESCR("octet length");

/* "bit"(int4, int4) => "bit" */ 
DATA(insert OID = 1683 ( bit  PGNSP PGUID 12 f f t f i 2 1560 f "23 23" _null_ _null_ _null_ bitfromint4 _null_ _null_ n 0));
DESCR("int4 to bitstring");

/* int4("bit") => int4 */ 
DATA(insert OID = 1684 ( int4  PGNSP PGUID 12 f f t f i 1 23 f "1560" _null_ _null_ _null_ bittoint4 _null_ _null_ n 0));
DESCR("bitstring to int4");

/* "bit"("bit", int4, bool) => "bit" */ 
DATA(insert OID = 1685 ( bit  PGNSP PGUID 12 f f t f i 3 1560 f "1560 23 16" _null_ _null_ _null_ bit _null_ _null_ n 0));
DESCR("adjust bit() to typmod length");

/* varbit(varbit, int4, bool) => varbit */ 
DATA(insert OID = 1687 ( varbit  PGNSP PGUID 12 f f t f i 3 1562 f "1562 23 16" _null_ _null_ _null_ varbit _null_ _null_ n 0));
DESCR("adjust varbit() to typmod length");

/* "position"("bit", "bit") => int4 */ 
DATA(insert OID = 1698 ( position  PGNSP PGUID 12 f f t f i 2 23 f "1560 1560" _null_ _null_ _null_ bitposition _null_ _null_ n 0));
DESCR("return position of sub-bitstring");

/* "substring"("bit", int4) => pg_catalog."bit" */ 
DATA(insert OID = 1699 ( substring  PGNSP PGUID 14 f f t f i 2 1560 f "1560 23" _null_ _null_ _null_ "select pg_catalog.substring($1, $2, -1)" _null_ _null_ c 0));
DESCR("return portion of bitstring");


/* for mac type support  */
/* macaddr_in(cstring) => macaddr */ 
DATA(insert OID = 436 ( macaddr_in  PGNSP PGUID 12 f f t f i 1 829 f "2275" _null_ _null_ _null_ macaddr_in _null_ _null_ n 0));
DESCR("I/O");

/* macaddr_out(macaddr) => cstring */ 
DATA(insert OID = 437 ( macaddr_out  PGNSP PGUID 12 f f t f i 1 2275 f "829" _null_ _null_ _null_ macaddr_out _null_ _null_ n 0));
DESCR("I/O");

/* text(macaddr) => text */ 
DATA(insert OID = 752 ( text  PGNSP PGUID 12 f f t f i 1 25 f "829" _null_ _null_ _null_ macaddr_text _null_ _null_ n 0));
DESCR("MAC address to text");

/* trunc(macaddr) => macaddr */ 
DATA(insert OID = 753 ( trunc  PGNSP PGUID 12 f f t f i 1 829 f "829" _null_ _null_ _null_ macaddr_trunc _null_ _null_ n 0));
DESCR("MAC manufacturer fields");

/* macaddr(text) => macaddr */ 
DATA(insert OID = 767 ( macaddr  PGNSP PGUID 12 f f t f i 1 829 f "25" _null_ _null_ _null_ text_macaddr _null_ _null_ n 0));
DESCR("text to MAC address");

/* macaddr_eq(macaddr, macaddr) => bool */ 
DATA(insert OID = 830 ( macaddr_eq  PGNSP PGUID 12 f f t f i 2 16 f "829 829" _null_ _null_ _null_ macaddr_eq _null_ _null_ n 0));
DESCR("equal");

/* macaddr_lt(macaddr, macaddr) => bool */ 
DATA(insert OID = 831 ( macaddr_lt  PGNSP PGUID 12 f f t f i 2 16 f "829 829" _null_ _null_ _null_ macaddr_lt _null_ _null_ n 0));
DESCR("less-than");

/* macaddr_le(macaddr, macaddr) => bool */ 
DATA(insert OID = 832 ( macaddr_le  PGNSP PGUID 12 f f t f i 2 16 f "829 829" _null_ _null_ _null_ macaddr_le _null_ _null_ n 0));
DESCR("less-than-or-equal");

/* macaddr_gt(macaddr, macaddr) => bool */ 
DATA(insert OID = 833 ( macaddr_gt  PGNSP PGUID 12 f f t f i 2 16 f "829 829" _null_ _null_ _null_ macaddr_gt _null_ _null_ n 0));
DESCR("greater-than");

/* macaddr_ge(macaddr, macaddr) => bool */ 
DATA(insert OID = 834 ( macaddr_ge  PGNSP PGUID 12 f f t f i 2 16 f "829 829" _null_ _null_ _null_ macaddr_ge _null_ _null_ n 0));
DESCR("greater-than-or-equal");

/* macaddr_ne(macaddr, macaddr) => bool */ 
DATA(insert OID = 835 ( macaddr_ne  PGNSP PGUID 12 f f t f i 2 16 f "829 829" _null_ _null_ _null_ macaddr_ne _null_ _null_ n 0));
DESCR("not equal");

/* macaddr_cmp(macaddr, macaddr) => int4 */ 
DATA(insert OID = 836 ( macaddr_cmp  PGNSP PGUID 12 f f t f i 2 23 f "829 829" _null_ _null_ _null_ macaddr_cmp _null_ _null_ n 0));
DESCR("less-equal-greater");


/* for inet type support  */
/* inet_in(cstring) => inet */ 
DATA(insert OID = 910 ( inet_in  PGNSP PGUID 12 f f t f i 1 869 f "2275" _null_ _null_ _null_ inet_in _null_ _null_ n 0));
DESCR("I/O");

/* inet_out(inet) => cstring */ 
DATA(insert OID = 911 ( inet_out  PGNSP PGUID 12 f f t f i 1 2275 f "869" _null_ _null_ _null_ inet_out _null_ _null_ n 0));
DESCR("I/O");


/* for cidr type support  */
/* cidr_in(cstring) => cidr */ 
DATA(insert OID = 1267 ( cidr_in  PGNSP PGUID 12 f f t f i 1 650 f "2275" _null_ _null_ _null_ cidr_in _null_ _null_ n 0));
DESCR("I/O");

/* cidr_out(cidr) => cstring */ 
DATA(insert OID = 1427 ( cidr_out  PGNSP PGUID 12 f f t f i 1 2275 f "650" _null_ _null_ _null_ cidr_out _null_ _null_ n 0));
DESCR("I/O");


/* these are used for both inet and cidr  */
/* network_eq(inet, inet) => bool */ 
DATA(insert OID = 920 ( network_eq  PGNSP PGUID 12 f f t f i 2 16 f "869 869" _null_ _null_ _null_ network_eq _null_ _null_ n 0));
DESCR("equal");

/* network_lt(inet, inet) => bool */ 
DATA(insert OID = 921 ( network_lt  PGNSP PGUID 12 f f t f i 2 16 f "869 869" _null_ _null_ _null_ network_lt _null_ _null_ n 0));
DESCR("less-than");

/* network_le(inet, inet) => bool */ 
DATA(insert OID = 922 ( network_le  PGNSP PGUID 12 f f t f i 2 16 f "869 869" _null_ _null_ _null_ network_le _null_ _null_ n 0));
DESCR("less-than-or-equal");

/* network_gt(inet, inet) => bool */ 
DATA(insert OID = 923 ( network_gt  PGNSP PGUID 12 f f t f i 2 16 f "869 869" _null_ _null_ _null_ network_gt _null_ _null_ n 0));
DESCR("greater-than");

/* network_ge(inet, inet) => bool */ 
DATA(insert OID = 924 ( network_ge  PGNSP PGUID 12 f f t f i 2 16 f "869 869" _null_ _null_ _null_ network_ge _null_ _null_ n 0));
DESCR("greater-than-or-equal");

/* network_ne(inet, inet) => bool */ 
DATA(insert OID = 925 ( network_ne  PGNSP PGUID 12 f f t f i 2 16 f "869 869" _null_ _null_ _null_ network_ne _null_ _null_ n 0));
DESCR("not equal");

/* network_cmp(inet, inet) => int4 */ 
DATA(insert OID = 926 ( network_cmp  PGNSP PGUID 12 f f t f i 2 23 f "869 869" _null_ _null_ _null_ network_cmp _null_ _null_ n 0));
DESCR("less-equal-greater");

/* network_sub(inet, inet) => bool */ 
DATA(insert OID = 927 ( network_sub  PGNSP PGUID 12 f f t f i 2 16 f "869 869" _null_ _null_ _null_ network_sub _null_ _null_ n 0));
DESCR("is-subnet");

/* network_subeq(inet, inet) => bool */ 
DATA(insert OID = 928 ( network_subeq  PGNSP PGUID 12 f f t f i 2 16 f "869 869" _null_ _null_ _null_ network_subeq _null_ _null_ n 0));
DESCR("is-subnet-or-equal");

/* network_sup(inet, inet) => bool */ 
DATA(insert OID = 929 ( network_sup  PGNSP PGUID 12 f f t f i 2 16 f "869 869" _null_ _null_ _null_ network_sup _null_ _null_ n 0));
DESCR("is-supernet");

/* network_supeq(inet, inet) => bool */ 
DATA(insert OID = 930 ( network_supeq  PGNSP PGUID 12 f f t f i 2 16 f "869 869" _null_ _null_ _null_ network_supeq _null_ _null_ n 0));
DESCR("is-supernet-or-equal");


/* inet/cidr functions  */
/* abbrev(inet) => text */ 
DATA(insert OID = 598 ( abbrev  PGNSP PGUID 12 f f t f i 1 25 f "869" _null_ _null_ _null_ inet_abbrev _null_ _null_ n 0));
DESCR("abbreviated display of inet value");

/* abbrev(cidr) => text */ 
DATA(insert OID = 599 ( abbrev  PGNSP PGUID 12 f f t f i 1 25 f "650" _null_ _null_ _null_ cidr_abbrev _null_ _null_ n 0));
DESCR("abbreviated display of cidr value");

/* set_masklen(inet, int4) => inet */ 
DATA(insert OID = 605 ( set_masklen  PGNSP PGUID 12 f f t f i 2 869 f "869 23" _null_ _null_ _null_ inet_set_masklen _null_ _null_ n 0));
DESCR("change netmask of inet");

/* set_masklen(cidr, int4) => cidr */ 
DATA(insert OID = 635 ( set_masklen  PGNSP PGUID 12 f f t f i 2 650 f "650 23" _null_ _null_ _null_ cidr_set_masklen _null_ _null_ n 0));
DESCR("change netmask of cidr");

/* family(inet) => int4 */ 
DATA(insert OID = 711 ( family  PGNSP PGUID 12 f f t f i 1 23 f "869" _null_ _null_ _null_ network_family _null_ _null_ n 0));
DESCR("address family (4 for IPv4, 6 for IPv6)");

/* network(inet) => cidr */ 
DATA(insert OID = 683 ( network  PGNSP PGUID 12 f f t f i 1 650 f "869" _null_ _null_ _null_ network_network _null_ _null_ n 0));
DESCR("network part of address");

/* netmask(inet) => inet */ 
DATA(insert OID = 696 ( netmask  PGNSP PGUID 12 f f t f i 1 869 f "869" _null_ _null_ _null_ network_netmask _null_ _null_ n 0));
DESCR("netmask of address");

/* masklen(inet) => int4 */ 
DATA(insert OID = 697 ( masklen  PGNSP PGUID 12 f f t f i 1 23 f "869" _null_ _null_ _null_ network_masklen _null_ _null_ n 0));
DESCR("netmask length");

/* broadcast(inet) => inet */ 
DATA(insert OID = 698 ( broadcast  PGNSP PGUID 12 f f t f i 1 869 f "869" _null_ _null_ _null_ network_broadcast _null_ _null_ n 0));
DESCR("broadcast address of network");

/* "host"(inet) => text */ 
DATA(insert OID = 699 ( host  PGNSP PGUID 12 f f t f i 1 25 f "869" _null_ _null_ _null_ network_host _null_ _null_ n 0));
DESCR("show address octets only");

/* text(inet) => text */ 
DATA(insert OID = 730 ( text  PGNSP PGUID 12 f f t f i 1 25 f "869" _null_ _null_ _null_ network_show _null_ _null_ n 0));
DESCR("show all parts of inet/cidr value");

/* hostmask(inet) => inet */ 
DATA(insert OID = 1362 ( hostmask  PGNSP PGUID 12 f f t f i 1 869 f "869" _null_ _null_ _null_ network_hostmask _null_ _null_ n 0));
DESCR("hostmask of address");

/* inet(text) => inet */ 
DATA(insert OID = 1713 ( inet  PGNSP PGUID 12 f f t f i 1 869 f "25" _null_ _null_ _null_ text_inet _null_ _null_ n 0));
DESCR("text to inet");

/* cidr(text) => cidr */ 
DATA(insert OID = 1714 ( cidr  PGNSP PGUID 12 f f t f i 1 650 f "25" _null_ _null_ _null_ text_cidr _null_ _null_ n 0));
DESCR("text to cidr");

/* cidr(inet) => cidr */ 
DATA(insert OID = 1715 ( cidr  PGNSP PGUID 12 f f t f i 1 650 f "869" _null_ _null_ _null_ inet_to_cidr _null_ _null_ n 0));
DESCR("coerce inet to cidr");

/* inet_client_addr() => inet */ 
DATA(insert OID = 2196 ( inet_client_addr  PGNSP PGUID 12 f f f f s 0 869 f "" _null_ _null_ _null_ inet_client_addr _null_ _null_ n 0));
DESCR("inet address of the client");

/* inet_client_port() => int4 */ 
DATA(insert OID = 2197 ( inet_client_port  PGNSP PGUID 12 f f f f s 0 23 f "" _null_ _null_ _null_ inet_client_port _null_ _null_ n 0));
DESCR("client's port number for this connection");

/* inet_server_addr() => inet */ 
DATA(insert OID = 2198 ( inet_server_addr  PGNSP PGUID 12 f f f f s 0 869 f "" _null_ _null_ _null_ inet_server_addr _null_ _null_ n 0));
DESCR("inet address of the server");

/* inet_server_port() => int4 */ 
DATA(insert OID = 2199 ( inet_server_port  PGNSP PGUID 12 f f f f s 0 23 f "" _null_ _null_ _null_ inet_server_port _null_ _null_ n 0));
DESCR("server's port number for this connection");

/* inetnot(inet) => inet */ 
DATA(insert OID = 2627 ( inetnot  PGNSP PGUID 12 f f t f i 1 869 f "869" _null_ _null_ _null_ inetnot _null_ _null_ n 0));
DESCR("bitwise not");

/* inetand(inet, inet) => inet */ 
DATA(insert OID = 2628 ( inetand  PGNSP PGUID 12 f f t f i 2 869 f "869 869" _null_ _null_ _null_ inetand _null_ _null_ n 0));
DESCR("bitwise and");

/* inetor(inet, inet) => inet */ 
DATA(insert OID = 2629 ( inetor  PGNSP PGUID 12 f f t f i 2 869 f "869 869" _null_ _null_ _null_ inetor _null_ _null_ n 0));
DESCR("bitwise or");

/* inetpl(inet, int8) => inet */ 
DATA(insert OID = 2630 ( inetpl  PGNSP PGUID 12 f f t f i 2 869 f "869 20" _null_ _null_ _null_ inetpl _null_ _null_ n 0));
DESCR("add integer to inet value");

/* int8pl_inet(int8, inet) => pg_catalog.inet */ 
DATA(insert OID = 2631 ( int8pl_inet  PGNSP PGUID 14 f f t f i 2 869 f "20 869" _null_ _null_ _null_ "select $2 + $1" _null_ _null_ c 0));
DESCR("add integer to inet value");

/* inetmi_int8(inet, int8) => inet */ 
DATA(insert OID = 2632 ( inetmi_int8  PGNSP PGUID 12 f f t f i 2 869 f "869 20" _null_ _null_ _null_ inetmi_int8 _null_ _null_ n 0));
DESCR("subtract integer from inet value");

/* inetmi(inet, inet) => int8 */ 
DATA(insert OID = 2633 ( inetmi  PGNSP PGUID 12 f f t f i 2 20 f "869 869" _null_ _null_ _null_ inetmi _null_ _null_ n 0));
DESCR("subtract inet values");

/* "numeric"(text) => "numeric" */ 
DATA(insert OID = 1686 ( numeric  PGNSP PGUID 12 f f t f i 1 1700 f "25" _null_ _null_ _null_ text_numeric _null_ _null_ n 0));
DESCR("(internal)");

/* text("numeric") => text */ 
DATA(insert OID = 1688 ( text  PGNSP PGUID 12 f f t f i 1 25 f "1700" _null_ _null_ _null_ numeric_text _null_ _null_ n 0));
DESCR("(internal)");

/* time_mi_time("time", "time") => "interval" */ 
DATA(insert OID = 1690 ( time_mi_time  PGNSP PGUID 12 f f t f i 2 1186 f "1083 1083" _null_ _null_ _null_ time_mi_time _null_ _null_ n 0));
DESCR("minus");

/* boolle(bool, bool) => bool */ 
DATA(insert OID = 1691 ( boolle  PGNSP PGUID 12 f f t f i 2 16 f "16 16" _null_ _null_ _null_ boolle _null_ _null_ n 0));
DESCR("less-than-or-equal");

/* boolge(bool, bool) => bool */ 
DATA(insert OID = 1692 ( boolge  PGNSP PGUID 12 f f t f i 2 16 f "16 16" _null_ _null_ _null_ boolge _null_ _null_ n 0));
DESCR("greater-than-or-equal");

/* btboolcmp(bool, bool) => int4 */ 
DATA(insert OID = 1693 ( btboolcmp  PGNSP PGUID 12 f f t f i 2 23 f "16 16" _null_ _null_ _null_ btboolcmp _null_ _null_ n 0));
DESCR("btree less-equal-greater");

/* timetz_hash(timetz) => int4 */ 
DATA(insert OID = 1696 ( timetz_hash  PGNSP PGUID 12 f f t f i 1 23 f "1266" _null_ _null_ _null_ timetz_hash _null_ _null_ n 0));
DESCR("hash");

/* interval_hash("interval") => int4 */ 
DATA(insert OID = 1697 ( interval_hash  PGNSP PGUID 12 f f t f i 1 23 f "1186" _null_ _null_ _null_ interval_hash _null_ _null_ n 0));
DESCR("hash");


/* OID's 1700 - 1799 NUMERIC data type  */
/* numeric_in(cstring, oid, int4) => "numeric" */ 
DATA(insert OID = 1701 ( numeric_in  PGNSP PGUID 12 f f t f i 3 1700 f "2275 26 23" _null_ _null_ _null_ numeric_in _null_ _null_ n 0));
DESCR("I/O");

/* numeric_out("numeric") => cstring */ 
DATA(insert OID = 1702 ( numeric_out  PGNSP PGUID 12 f f t f i 1 2275 f "1700" _null_ _null_ _null_ numeric_out _null_ _null_ n 0));
DESCR("I/O");

/* "numeric"("numeric", int4) => "numeric" */ 
DATA(insert OID = 1703 ( numeric  PGNSP PGUID 12 f f t f i 2 1700 f "1700 23" _null_ _null_ _null_ numeric _null_ _null_ n 0));
DESCR("adjust numeric to typmod precision/scale");

/* numeric_abs("numeric") => "numeric" */ 
DATA(insert OID = 1704 ( numeric_abs  PGNSP PGUID 12 f f t f i 1 1700 f "1700" _null_ _null_ _null_ numeric_abs _null_ _null_ n 0));
DESCR("absolute value");

/* abs("numeric") => "numeric" */ 
DATA(insert OID = 1705 ( abs  PGNSP PGUID 12 f f t f i 1 1700 f "1700" _null_ _null_ _null_ numeric_abs _null_ _null_ n 0));
DESCR("absolute value");

/* sign("numeric") => "numeric" */ 
DATA(insert OID = 1706 ( sign  PGNSP PGUID 12 f f t f i 1 1700 f "1700" _null_ _null_ _null_ numeric_sign _null_ _null_ n 0));
DESCR("sign of value");

/* round("numeric", int4) => "numeric" */ 
DATA(insert OID = 1707 ( round  PGNSP PGUID 12 f f t f i 2 1700 f "1700 23" _null_ _null_ _null_ numeric_round _null_ _null_ n 0));
DESCR("value rounded to 'scale'");

/* round("numeric") => pg_catalog."numeric" */ 
DATA(insert OID = 1708 ( round  PGNSP PGUID 14 f f t f i 1 1700 f "1700" _null_ _null_ _null_ "select pg_catalog.round($1,0)" _null_ _null_ c 0));
DESCR("value rounded to 'scale' of zero");

/* trunc("numeric", int4) => "numeric" */ 
DATA(insert OID = 1709 ( trunc  PGNSP PGUID 12 f f t f i 2 1700 f "1700 23" _null_ _null_ _null_ numeric_trunc _null_ _null_ n 0));
DESCR("value truncated to 'scale'");

/* trunc("numeric") => pg_catalog."numeric" */ 
DATA(insert OID = 1710 ( trunc  PGNSP PGUID 14 f f t f i 1 1700 f "1700" _null_ _null_ _null_ "select pg_catalog.trunc($1,0)" _null_ _null_ c 0));
DESCR("value truncated to 'scale' of zero");

/* ceil("numeric") => "numeric" */ 
DATA(insert OID = 1711 ( ceil  PGNSP PGUID 12 f f t f i 1 1700 f "1700" _null_ _null_ _null_ numeric_ceil _null_ _null_ n 0));
DESCR("smallest integer >= value");

/* ceiling("numeric") => "numeric" */ 
DATA(insert OID = 2167 ( ceiling  PGNSP PGUID 12 f f t f i 1 1700 f "1700" _null_ _null_ _null_ numeric_ceil _null_ _null_ n 0));
DESCR("smallest integer >= value");

/* floor("numeric") => "numeric" */ 
DATA(insert OID = 1712 ( floor  PGNSP PGUID 12 f f t f i 1 1700 f "1700" _null_ _null_ _null_ numeric_floor _null_ _null_ n 0));
DESCR("largest integer <= value");

/* numeric_eq("numeric", "numeric") => bool */ 
DATA(insert OID = 1718 ( numeric_eq  PGNSP PGUID 12 f f t f i 2 16 f "1700 1700" _null_ _null_ _null_ numeric_eq _null_ _null_ n 0));
DESCR("equal");

/* numeric_ne("numeric", "numeric") => bool */ 
DATA(insert OID = 1719 ( numeric_ne  PGNSP PGUID 12 f f t f i 2 16 f "1700 1700" _null_ _null_ _null_ numeric_ne _null_ _null_ n 0));
DESCR("not equal");

/* numeric_gt("numeric", "numeric") => bool */ 
DATA(insert OID = 1720 ( numeric_gt  PGNSP PGUID 12 f f t f i 2 16 f "1700 1700" _null_ _null_ _null_ numeric_gt _null_ _null_ n 0));
DESCR("greater-than");

/* numeric_ge("numeric", "numeric") => bool */ 
DATA(insert OID = 1721 ( numeric_ge  PGNSP PGUID 12 f f t f i 2 16 f "1700 1700" _null_ _null_ _null_ numeric_ge _null_ _null_ n 0));
DESCR("greater-than-or-equal");

/* numeric_lt("numeric", "numeric") => bool */ 
DATA(insert OID = 1722 ( numeric_lt  PGNSP PGUID 12 f f t f i 2 16 f "1700 1700" _null_ _null_ _null_ numeric_lt _null_ _null_ n 0));
DESCR("less-than");

/* numeric_le("numeric", "numeric") => bool */ 
DATA(insert OID = 1723 ( numeric_le  PGNSP PGUID 12 f f t f i 2 16 f "1700 1700" _null_ _null_ _null_ numeric_le _null_ _null_ n 0));
DESCR("less-than-or-equal");

/* numeric_add("numeric", "numeric") => "numeric" */ 
DATA(insert OID = 1724 ( numeric_add  PGNSP PGUID 12 f f t f i 2 1700 f "1700 1700" _null_ _null_ _null_ numeric_add _null_ _null_ n 0));
DESCR("add");

/* numeric_sub("numeric", "numeric") => "numeric" */ 
DATA(insert OID = 1725 ( numeric_sub  PGNSP PGUID 12 f f t f i 2 1700 f "1700 1700" _null_ _null_ _null_ numeric_sub _null_ _null_ n 0));
DESCR("subtract");

/* numeric_mul("numeric", "numeric") => "numeric" */ 
DATA(insert OID = 1726 ( numeric_mul  PGNSP PGUID 12 f f t f i 2 1700 f "1700 1700" _null_ _null_ _null_ numeric_mul _null_ _null_ n 0));
DESCR("multiply");

/* numeric_div("numeric", "numeric") => "numeric" */ 
DATA(insert OID = 1727 ( numeric_div  PGNSP PGUID 12 f f t f i 2 1700 f "1700 1700" _null_ _null_ _null_ numeric_div _null_ _null_ n 0));
DESCR("divide");

/* mod("numeric", "numeric") => "numeric" */ 
DATA(insert OID = 1728 ( mod  PGNSP PGUID 12 f f t f i 2 1700 f "1700 1700" _null_ _null_ _null_ numeric_mod _null_ _null_ n 0));
DESCR("modulus");

/* numeric_mod("numeric", "numeric") => "numeric" */ 
DATA(insert OID = 1729 ( numeric_mod  PGNSP PGUID 12 f f t f i 2 1700 f "1700 1700" _null_ _null_ _null_ numeric_mod _null_ _null_ n 0));
DESCR("modulus");

/* sqrt("numeric") => "numeric" */ 
DATA(insert OID = 1730 ( sqrt  PGNSP PGUID 12 f f t f i 1 1700 f "1700" _null_ _null_ _null_ numeric_sqrt _null_ _null_ n 0));
DESCR("square root");

/* numeric_sqrt("numeric") => "numeric" */ 
DATA(insert OID = 1731 ( numeric_sqrt  PGNSP PGUID 12 f f t f i 1 1700 f "1700" _null_ _null_ _null_ numeric_sqrt _null_ _null_ n 0));
DESCR("square root");

/* exp("numeric") => "numeric" */ 
DATA(insert OID = 1732 ( exp  PGNSP PGUID 12 f f t f i 1 1700 f "1700" _null_ _null_ _null_ numeric_exp _null_ _null_ n 0));
DESCR("e raised to the power of n");

/* numeric_exp("numeric") => "numeric" */ 
DATA(insert OID = 1733 ( numeric_exp  PGNSP PGUID 12 f f t f i 1 1700 f "1700" _null_ _null_ _null_ numeric_exp _null_ _null_ n 0));
DESCR("e raised to the power of n");

/* ln("numeric") => "numeric" */ 
DATA(insert OID = 1734 ( ln  PGNSP PGUID 12 f f t f i 1 1700 f "1700" _null_ _null_ _null_ numeric_ln _null_ _null_ n 0));
DESCR("natural logarithm of n");

/* numeric_ln("numeric") => "numeric" */ 
DATA(insert OID = 1735 ( numeric_ln  PGNSP PGUID 12 f f t f i 1 1700 f "1700" _null_ _null_ _null_ numeric_ln _null_ _null_ n 0));
DESCR("natural logarithm of n");

/* "log"("numeric", "numeric") => "numeric" */ 
DATA(insert OID = 1736 ( log  PGNSP PGUID 12 f f t f i 2 1700 f "1700 1700" _null_ _null_ _null_ numeric_log _null_ _null_ n 0));
DESCR("logarithm base m of n");

/* numeric_log("numeric", "numeric") => "numeric" */ 
DATA(insert OID = 1737 ( numeric_log  PGNSP PGUID 12 f f t f i 2 1700 f "1700 1700" _null_ _null_ _null_ numeric_log _null_ _null_ n 0));
DESCR("logarithm base m of n");

/* pow("numeric", "numeric") => "numeric" */ 
DATA(insert OID = 1738 ( pow  PGNSP PGUID 12 f f t f i 2 1700 f "1700 1700" _null_ _null_ _null_ numeric_power _null_ _null_ n 0));
DESCR("m raised to the power of n");

/* power("numeric", "numeric") => "numeric" */ 
DATA(insert OID = 2169 ( power  PGNSP PGUID 12 f f t f i 2 1700 f "1700 1700" _null_ _null_ _null_ numeric_power _null_ _null_ n 0));
DESCR("m raised to the power of n");

/* numeric_power("numeric", "numeric") => "numeric" */ 
DATA(insert OID = 1739 ( numeric_power  PGNSP PGUID 12 f f t f i 2 1700 f "1700 1700" _null_ _null_ _null_ numeric_power _null_ _null_ n 0));
DESCR("m raised to the power of n");

/* "numeric"(int4) => "numeric" */ 
DATA(insert OID = 1740 ( numeric  PGNSP PGUID 12 f f t f i 1 1700 f "23" _null_ _null_ _null_ int4_numeric _null_ _null_ n 0));
DESCR("(internal)");

/* "log"("numeric") => pg_catalog."numeric" */ 
DATA(insert OID = 1741 ( log  PGNSP PGUID 14 f f t f i 1 1700 f "1700" _null_ _null_ _null_ "select pg_catalog.log(10, $1)" _null_ _null_ c 0));
DESCR("logarithm base 10 of n");

/* "numeric"(float4) => "numeric" */ 
DATA(insert OID = 1742 ( numeric  PGNSP PGUID 12 f f t f i 1 1700 f "700" _null_ _null_ _null_ float4_numeric _null_ _null_ n 0));
DESCR("(internal)");

/* "numeric"(float8) => "numeric" */ 
DATA(insert OID = 1743 ( numeric  PGNSP PGUID 12 f f t f i 1 1700 f "701" _null_ _null_ _null_ float8_numeric _null_ _null_ n 0));
DESCR("(internal)");

/* int4("numeric") => int4 */ 
DATA(insert OID = 1744 ( int4  PGNSP PGUID 12 f f t f i 1 23 f "1700" _null_ _null_ _null_ numeric_int4 _null_ _null_ n 0));
DESCR("(internal)");

/* float4("numeric") => float4 */ 
DATA(insert OID = 1745 ( float4  PGNSP PGUID 12 f f t f i 1 700 f "1700" _null_ _null_ _null_ numeric_float4 _null_ _null_ n 0));
DESCR("(internal)");

/* float8("numeric") => float8 */ 
DATA(insert OID = 1746 ( float8  PGNSP PGUID 12 f f t f i 1 701 f "1700" _null_ _null_ _null_ numeric_float8 _null_ _null_ n 0));
DESCR("(internal)");

/* width_bucket("numeric", "numeric", "numeric", int4) => int4 */ 
DATA(insert OID = 2170 ( width_bucket  PGNSP PGUID 12 f f t f i 4 23 f "1700 1700 1700 23" _null_ _null_ _null_ width_bucket_numeric _null_ _null_ n 0));
DESCR("bucket number of operand in equidepth histogram");

/* time_pl_interval("time", "interval") => "time" */ 
DATA(insert OID = 1747 ( time_pl_interval  PGNSP PGUID 12 f f t f i 2 1083 f "1083 1186" _null_ _null_ _null_ time_pl_interval _null_ _null_ n 0));
DESCR("plus");

/* time_mi_interval("time", "interval") => "time" */ 
DATA(insert OID = 1748 ( time_mi_interval  PGNSP PGUID 12 f f t f i 2 1083 f "1083 1186" _null_ _null_ _null_ time_mi_interval _null_ _null_ n 0));
DESCR("minus");

/* timetz_pl_interval(timetz, "interval") => timetz */ 
DATA(insert OID = 1749 ( timetz_pl_interval  PGNSP PGUID 12 f f t f i 2 1266 f "1266 1186" _null_ _null_ _null_ timetz_pl_interval _null_ _null_ n 0));
DESCR("plus");

/* timetz_mi_interval(timetz, "interval") => timetz */ 
DATA(insert OID = 1750 ( timetz_mi_interval  PGNSP PGUID 12 f f t f i 2 1266 f "1266 1186" _null_ _null_ _null_ timetz_mi_interval _null_ _null_ n 0));
DESCR("minus");

/* numeric_inc("numeric") => "numeric" */ 
DATA(insert OID = 1764 ( numeric_inc  PGNSP PGUID 12 f f t f i 1 1700 f "1700" _null_ _null_ _null_ numeric_inc _null_ _null_ n 0));
DESCR("increment by one");

/* numeric_dec("numeric") => "numeric" */ 
DATA(insert OID = 1004 ( numeric_dec  PGNSP PGUID 12 f f t f i 1 1700 f "1700" _null_ _null_ _null_ numeric_dec _null_ _null_ n 0));
DESCR("increment by one");

/* numeric_smaller("numeric", "numeric") => "numeric" */ 
DATA(insert OID = 1766 ( numeric_smaller  PGNSP PGUID 12 f f t f i 2 1700 f "1700 1700" _null_ _null_ _null_ numeric_smaller _null_ _null_ n 0));
DESCR("smaller of two numbers");

/* numeric_larger("numeric", "numeric") => "numeric" */ 
DATA(insert OID = 1767 ( numeric_larger  PGNSP PGUID 12 f f t f i 2 1700 f "1700 1700" _null_ _null_ _null_ numeric_larger _null_ _null_ n 0));
DESCR("larger of two numbers");

/* numeric_cmp("numeric", "numeric") => int4 */ 
DATA(insert OID = 1769 ( numeric_cmp  PGNSP PGUID 12 f f t f i 2 23 f "1700 1700" _null_ _null_ _null_ numeric_cmp _null_ _null_ n 0));
DESCR("compare two numbers");

/* numeric_uminus("numeric") => "numeric" */ 
DATA(insert OID = 1771 ( numeric_uminus  PGNSP PGUID 12 f f t f i 1 1700 f "1700" _null_ _null_ _null_ numeric_uminus _null_ _null_ n 0));
DESCR("negate");

/* int8("numeric") => int8 */ 
DATA(insert OID = 1779 ( int8  PGNSP PGUID 12 f f t f i 1 20 f "1700" _null_ _null_ _null_ numeric_int8 _null_ _null_ n 0));
DESCR("(internal)");

/* "numeric"(int8) => "numeric" */ 
DATA(insert OID = 1781 ( numeric  PGNSP PGUID 12 f f t f i 1 1700 f "20" _null_ _null_ _null_ int8_numeric _null_ _null_ n 0));
DESCR("(internal)");

/* "numeric"(int2) => "numeric" */ 
DATA(insert OID = 1782 ( numeric  PGNSP PGUID 12 f f t f i 1 1700 f "21" _null_ _null_ _null_ int2_numeric _null_ _null_ n 0));
DESCR("(internal)");

/* int2("numeric") => int2 */ 
DATA(insert OID = 1783 ( int2  PGNSP PGUID 12 f f t f i 1 21 f "1700" _null_ _null_ _null_ numeric_int2 _null_ _null_ n 0));
DESCR("(internal)");


/* formatting */
/* to_char(timestamptz, text) => text */ 
DATA(insert OID = 1770 ( to_char  PGNSP PGUID 12 f f t f s 2 25 f "1184 25" _null_ _null_ _null_ timestamptz_to_char _null_ _null_ n 0));
DESCR("format timestamp with time zone to text");

/* to_char("numeric", text) => text */ 
DATA(insert OID = 1772 ( to_char  PGNSP PGUID 12 f f t f s 2 25 f "1700 25" _null_ _null_ _null_ numeric_to_char _null_ _null_ n 0));
DESCR("format numeric to text");

/* to_char(int4, text) => text */ 
DATA(insert OID = 1773 ( to_char  PGNSP PGUID 12 f f t f s 2 25 f "23 25" _null_ _null_ _null_ int4_to_char _null_ _null_ n 0));
DESCR("format int4 to text");

/* to_char(int8, text) => text */ 
DATA(insert OID = 1774 ( to_char  PGNSP PGUID 12 f f t f s 2 25 f "20 25" _null_ _null_ _null_ int8_to_char _null_ _null_ n 0));
DESCR("format int8 to text");

/* to_char(float4, text) => text */ 
DATA(insert OID = 1775 ( to_char  PGNSP PGUID 12 f f t f s 2 25 f "700 25" _null_ _null_ _null_ float4_to_char _null_ _null_ n 0));
DESCR("format float4 to text");

/* to_char(float8, text) => text */ 
DATA(insert OID = 1776 ( to_char  PGNSP PGUID 12 f f t f s 2 25 f "701 25" _null_ _null_ _null_ float8_to_char _null_ _null_ n 0));
DESCR("format float8 to text");

/* to_number(text, text) => "numeric" */ 
DATA(insert OID = 1777 ( to_number  PGNSP PGUID 12 f f t f s 2 1700 f "25 25" _null_ _null_ _null_ numeric_to_number _null_ _null_ n 0));
DESCR("convert text to numeric");

/* to_timestamp(text, text) => timestamptz */ 
DATA(insert OID = 1778 ( to_timestamp  PGNSP PGUID 12 f f t f s 2 1184 f "25 25" _null_ _null_ _null_ to_timestamp _null_ _null_ n 0));
DESCR("convert text to timestamp with time zone");

/* to_date(text, text) => date */ 
DATA(insert OID = 1780 ( to_date  PGNSP PGUID 12 f f t f s 2 1082 f "25 25" _null_ _null_ _null_ to_date _null_ _null_ n 0));
DESCR("convert text to date");

/* to_char("interval", text) => text */ 
DATA(insert OID = 1768 ( to_char  PGNSP PGUID 12 f f t f s 2 25 f "1186 25" _null_ _null_ _null_ interval_to_char _null_ _null_ n 0));
DESCR("format interval to text");

/* quote_ident(text) => text */ 
DATA(insert OID = 1282 ( quote_ident  PGNSP PGUID 12 f f t f i 1 25 f "25" _null_ _null_ _null_ quote_ident _null_ _null_ n 0));
DESCR("quote an identifier for usage in a querystring");

/* quote_literal(text) => text */ 
DATA(insert OID = 1283 ( quote_literal  PGNSP PGUID 12 f f t f i 1 25 f "25" _null_ _null_ _null_ quote_literal _null_ _null_ n 0));
DESCR("quote a literal for usage in a querystring");

/* oidin(cstring) => oid */ 
DATA(insert OID = 1798 ( oidin  PGNSP PGUID 12 f f t f i 1 26 f "2275" _null_ _null_ _null_ oidin _null_ _null_ n 0));
DESCR("I/O");

/* oidout(oid) => cstring */ 
DATA(insert OID = 1799 ( oidout  PGNSP PGUID 12 f f t f i 1 2275 f "26" _null_ _null_ _null_ oidout _null_ _null_ n 0));
DESCR("I/O");

/* bit_length(bytea) => pg_catalog.int4 */ 
DATA(insert OID = 1810 ( bit_length  PGNSP PGUID 14 f f t f i 1 23 f "17" _null_ _null_ _null_ "select pg_catalog.octet_length($1) * 8" _null_ _null_ c 0));
DESCR("length in bits");

/* bit_length(text) => pg_catalog.int4 */ 
DATA(insert OID = 1811 ( bit_length  PGNSP PGUID 14 f f t f i 1 23 f "25" _null_ _null_ _null_ "select pg_catalog.octet_length($1) * 8" _null_ _null_ c 0));
DESCR("length in bits");

/* bit_length("bit") => pg_catalog.int4 */ 
DATA(insert OID = 1812 ( bit_length  PGNSP PGUID 14 f f t f i 1 23 f "1560" _null_ _null_ _null_ "select pg_catalog.length($1)" _null_ _null_ c 0));
DESCR("length in bits");


/* Selectivity estimators for LIKE and related operators  */
/* iclikesel(internal, oid, internal, int4) => float8 */ 
DATA(insert OID = 1814 ( iclikesel  PGNSP PGUID 12 f f t f s 4 701 f "2281 26 2281 23" _null_ _null_ _null_ iclikesel _null_ _null_ n 0));
DESCR("restriction selectivity of ILIKE");

/* icnlikesel(internal, oid, internal, int4) => float8 */ 
DATA(insert OID = 1815 ( icnlikesel  PGNSP PGUID 12 f f t f s 4 701 f "2281 26 2281 23" _null_ _null_ _null_ icnlikesel _null_ _null_ n 0));
DESCR("restriction selectivity of NOT ILIKE");

/* iclikejoinsel(internal, oid, internal, int2) => float8 */ 
DATA(insert OID = 1816 ( iclikejoinsel  PGNSP PGUID 12 f f t f s 4 701 f "2281 26 2281 21" _null_ _null_ _null_ iclikejoinsel _null_ _null_ n 0));
DESCR("join selectivity of ILIKE");

/* icnlikejoinsel(internal, oid, internal, int2) => float8 */ 
DATA(insert OID = 1817 ( icnlikejoinsel  PGNSP PGUID 12 f f t f s 4 701 f "2281 26 2281 21" _null_ _null_ _null_ icnlikejoinsel _null_ _null_ n 0));
DESCR("join selectivity of NOT ILIKE");

/* regexeqsel(internal, oid, internal, int4) => float8 */ 
DATA(insert OID = 1818 ( regexeqsel  PGNSP PGUID 12 f f t f s 4 701 f "2281 26 2281 23" _null_ _null_ _null_ regexeqsel _null_ _null_ n 0));
DESCR("restriction selectivity of regex match");

/* likesel(internal, oid, internal, int4) => float8 */ 
DATA(insert OID = 1819 ( likesel  PGNSP PGUID 12 f f t f s 4 701 f "2281 26 2281 23" _null_ _null_ _null_ likesel _null_ _null_ n 0));
DESCR("restriction selectivity of LIKE");

/* icregexeqsel(internal, oid, internal, int4) => float8 */ 
DATA(insert OID = 1820 ( icregexeqsel  PGNSP PGUID 12 f f t f s 4 701 f "2281 26 2281 23" _null_ _null_ _null_ icregexeqsel _null_ _null_ n 0));
DESCR("restriction selectivity of case-insensitive regex match");

/* regexnesel(internal, oid, internal, int4) => float8 */ 
DATA(insert OID = 1821 ( regexnesel  PGNSP PGUID 12 f f t f s 4 701 f "2281 26 2281 23" _null_ _null_ _null_ regexnesel _null_ _null_ n 0));
DESCR("restriction selectivity of regex non-match");

/* nlikesel(internal, oid, internal, int4) => float8 */ 
DATA(insert OID = 1822 ( nlikesel  PGNSP PGUID 12 f f t f s 4 701 f "2281 26 2281 23" _null_ _null_ _null_ nlikesel _null_ _null_ n 0));
DESCR("restriction selectivity of NOT LIKE");

/* icregexnesel(internal, oid, internal, int4) => float8 */ 
DATA(insert OID = 1823 ( icregexnesel  PGNSP PGUID 12 f f t f s 4 701 f "2281 26 2281 23" _null_ _null_ _null_ icregexnesel _null_ _null_ n 0));
DESCR("restriction selectivity of case-insensitive regex non-match");

/* regexeqjoinsel(internal, oid, internal, int2) => float8 */ 
DATA(insert OID = 1824 ( regexeqjoinsel  PGNSP PGUID 12 f f t f s 4 701 f "2281 26 2281 21" _null_ _null_ _null_ regexeqjoinsel _null_ _null_ n 0));
DESCR("join selectivity of regex match");

/* likejoinsel(internal, oid, internal, int2) => float8 */ 
DATA(insert OID = 1825 ( likejoinsel  PGNSP PGUID 12 f f t f s 4 701 f "2281 26 2281 21" _null_ _null_ _null_ likejoinsel _null_ _null_ n 0));
DESCR("join selectivity of LIKE");

/* icregexeqjoinsel(internal, oid, internal, int2) => float8 */ 
DATA(insert OID = 1826 ( icregexeqjoinsel  PGNSP PGUID 12 f f t f s 4 701 f "2281 26 2281 21" _null_ _null_ _null_ icregexeqjoinsel _null_ _null_ n 0));
DESCR("join selectivity of case-insensitive regex match");

/* regexnejoinsel(internal, oid, internal, int2) => float8 */ 
DATA(insert OID = 1827 ( regexnejoinsel  PGNSP PGUID 12 f f t f s 4 701 f "2281 26 2281 21" _null_ _null_ _null_ regexnejoinsel _null_ _null_ n 0));
DESCR("join selectivity of regex non-match");

/* nlikejoinsel(internal, oid, internal, int2) => float8 */ 
DATA(insert OID = 1828 ( nlikejoinsel  PGNSP PGUID 12 f f t f s 4 701 f "2281 26 2281 21" _null_ _null_ _null_ nlikejoinsel _null_ _null_ n 0));
DESCR("join selectivity of NOT LIKE");

/* icregexnejoinsel(internal, oid, internal, int2) => float8 */ 
DATA(insert OID = 1829 ( icregexnejoinsel  PGNSP PGUID 12 f f t f s 4 701 f "2281 26 2281 21" _null_ _null_ _null_ icregexnejoinsel _null_ _null_ n 0));
DESCR("join selectivity of case-insensitive regex non-match");


/* Sequences and time series */
/* interval_bound(numeric, numeric) => numeric */ 
DATA(insert OID = 2991 ( interval_bound  PGNSP PGUID 12 f f t f i 2 1700 f "1700 1700" _null_ _null_ _null_ numeric_interval_bound _null_ _null_ n 0));
DESCR("boundary of the interval containing the given value");

/* interval_bound(numeric, numeric, int4) => numeric */ 
DATA(insert OID = 2992 ( interval_bound  PGNSP PGUID 12 f f f f i 3 1700 f "1700 1700 23" _null_ _null_ _null_ numeric_interval_bound_shift _null_ _null_ n 0));
DESCR("boundary of the interval containing the given value");

/* interval_bound(numeric, numeric, int4, numeric) => numeric */ 
DATA(insert OID = 2993 ( interval_bound  PGNSP PGUID 12 f f f f i 4 1700 f "1700 1700 23 1700" _null_ _null_ _null_ numeric_interval_bound_shift_rbound _null_ _null_ n 0));
DESCR("boundary of the interval containing the given value");

/* interval_bound(timestamp, "interval") => timestamp */ 
DATA(insert OID = 2994 ( interval_bound  PGNSP PGUID 12 f f t f i 2 1114 f "1114 1186" _null_ _null_ _null_ timestamp_interval_bound _null_ _null_ n 0));
DESCR("boundary of the interval containing the given value");

/* interval_bound(timestamp, "interval", int4) => timestamp */ 
DATA(insert OID = 2995 ( interval_bound  PGNSP PGUID 12 f f f f i 3 1114 f "1114 1186 23" _null_ _null_ _null_ timestamp_interval_bound_shift _null_ _null_ n 0));
DESCR("boundary of the interval containing the given value");

/* interval_bound(timestamp, "interval", int4, timestamp) => timestamp */ 
DATA(insert OID = 2996 ( interval_bound  PGNSP PGUID 12 f f f f i 4 1114 f "1114 1186 23 1114" _null_ _null_ _null_ timestamp_interval_bound_shift_reg _null_ _null_ n 0));
DESCR("boundary of the interval containing the given value");

/* interval_bound(timestamptz, "interval") => timestamptz */ 
DATA(insert OID = 2997 ( interval_bound  PGNSP PGUID 12 f f t f i 2 1184 f "1184 1186" _null_ _null_ _null_ timestamptz_interval_bound _null_ _null_ n 0));
DESCR("boundary of the interval containing the given value");

/* interval_bound(timestamptz, "interval", int4) => timestamptz */ 
DATA(insert OID = 2998 ( interval_bound  PGNSP PGUID 12 f f f f i 3 1184 f "1184 1186 23" _null_ _null_ _null_ timestamptz_interval_bound_shift _null_ _null_ n 0));
DESCR("boundary of the interval containing the given value");

/* interval_bound(timestamptz, "interval", int4, timestamptz) => timestamptz */ 
DATA(insert OID = 2999 ( interval_bound  PGNSP PGUID 12 f f f f i 4 1184 f "1184 1186 23 1184" _null_ _null_ _null_ timestamptz_interval_bound_shift_reg _null_ _null_ n 0));
DESCR("boundary of the interval containing the given value");


/* Aggregate-related functions  */
/* float8_avg(bytea) => float8 */ 
DATA(insert OID = 1830 ( float8_avg  PGNSP PGUID 12 f f t f i 1 701 f "17" _null_ _null_ _null_ float8_avg _null_ _null_ n 0));
DESCR("AVG aggregate final function");

/* float8_var_pop(_float8) => float8 */ 
DATA(insert OID = 2512 ( float8_var_pop  PGNSP PGUID 12 f f t f i 1 701 f "1022" _null_ _null_ _null_ float8_var_pop _null_ _null_ n 0));
DESCR("VAR_POP aggregate final function");

/* float8_var_samp(_float8) => float8 */ 
DATA(insert OID = 1831 ( float8_var_samp  PGNSP PGUID 12 f f t f i 1 701 f "1022" _null_ _null_ _null_ float8_var_samp _null_ _null_ n 0));
DESCR("VAR_SAMP aggregate final function");

/* float8_stddev_pop(_float8) => float8 */ 
DATA(insert OID = 2513 ( float8_stddev_pop  PGNSP PGUID 12 f f t f i 1 701 f "1022" _null_ _null_ _null_ float8_stddev_pop _null_ _null_ n 0));
DESCR("STDDEV_POP aggregate final function");

/* float8_stddev_samp(_float8) => float8 */ 
DATA(insert OID = 1832 ( float8_stddev_samp  PGNSP PGUID 12 f f t f i 1 701 f "1022" _null_ _null_ _null_ float8_stddev_samp _null_ _null_ n 0));
DESCR("STDDEV_SAMP aggregate final function");

/* numeric_accum(_numeric, "numeric") => _numeric */ 
DATA(insert OID = 1833 ( numeric_accum  PGNSP PGUID 12 f f t f i 2 1231 f "1231 1700" _null_ _null_ _null_ numeric_accum _null_ _null_ n 0));
DESCR("aggregate transition function");

/* numeric_avg_accum(bytea, "numeric") => bytea */ 
DATA(insert OID = 3102 ( numeric_avg_accum  PGNSP PGUID 12 f f t f i 2 17 f "17 1700" _null_ _null_ _null_ numeric_avg_accum _null_ _null_ n 0));
DESCR("aggregate transition function");

/* numeric_decum(_numeric, "numeric") => _numeric */ 
DATA(insert OID = 7309 ( numeric_decum  PGNSP PGUID 12 f f t f i 2 1231 f "1231 1700" _null_ _null_ _null_ numeric_decum _null_ _null_ n 0));
DESCR("aggregate inverse transition function");

/* numeric_avg_decum(bytea, "numeric") => bytea */ 
DATA(insert OID = 3103 ( numeric_avg_decum  PGNSP PGUID 12 f f t f i 2 17 f "17 1700" _null_ _null_ _null_ numeric_avg_decum _null_ _null_ n 0));
DESCR("aggregate inverse transition function");

/* int2_accum(_numeric, int2) => _numeric */ 
DATA(insert OID = 1834 ( int2_accum  PGNSP PGUID 12 f f t f i 2 1231 f "1231 21" _null_ _null_ _null_ int2_accum _null_ _null_ n 0));
DESCR("aggregate transition function");

/* int4_accum(_numeric, int4) => _numeric */ 
DATA(insert OID = 1835 ( int4_accum  PGNSP PGUID 12 f f t f i 2 1231 f "1231 23" _null_ _null_ _null_ int4_accum _null_ _null_ n 0));
DESCR("aggregate transition function");

/* int8_accum(_numeric, int8) => _numeric */ 
DATA(insert OID = 1836 ( int8_accum  PGNSP PGUID 12 f f t f i 2 1231 f "1231 20" _null_ _null_ _null_ int8_accum _null_ _null_ n 0));
DESCR("aggregate transition function");

/* int2_decum(_numeric, int2) => _numeric */ 
DATA(insert OID = 7306 ( int2_decum  PGNSP PGUID 12 f f t f i 2 1231 f "1231 21" _null_ _null_ _null_ int2_decum _null_ _null_ n 0));
DESCR("aggregate inverse transition function");

/* int4_decum(_numeric, int4) => _numeric */ 
DATA(insert OID = 7307 ( int4_decum  PGNSP PGUID 12 f f t f i 2 1231 f "1231 23" _null_ _null_ _null_ int4_decum _null_ _null_ n 0));
DESCR("aggregate inverse transition function");

/* int8_decum(_numeric, int8) => _numeric */ 
DATA(insert OID = 7308 ( int8_decum  PGNSP PGUID 12 f f t f i 2 1231 f "1231 20" _null_ _null_ _null_ int8_decum _null_ _null_ n 0));
DESCR("aggregate inverse transition function");

/* numeric_avg(bytea) => "numeric" */ 
DATA(insert OID = 1837 ( numeric_avg  PGNSP PGUID 12 f f t f i 1 1700 f "17" _null_ _null_ _null_ numeric_avg _null_ _null_ n 0));
DESCR("AVG aggregate final function");

/* numeric_var_pop(_numeric) => "numeric" */ 
DATA(insert OID = 2514 ( numeric_var_pop  PGNSP PGUID 12 f f t f i 1 1700 f "1231" _null_ _null_ _null_ numeric_var_pop _null_ _null_ n 0));
DESCR("VAR_POP aggregate final function");

/* numeric_var_samp(_numeric) => "numeric" */ 
DATA(insert OID = 1838 ( numeric_var_samp  PGNSP PGUID 12 f f t f i 1 1700 f "1231" _null_ _null_ _null_ numeric_var_samp _null_ _null_ n 0));
DESCR("VAR_SAMP aggregate final function");

/* numeric_stddev_pop(_numeric) => "numeric" */ 
DATA(insert OID = 2596 ( numeric_stddev_pop  PGNSP PGUID 12 f f t f i 1 1700 f "1231" _null_ _null_ _null_ numeric_stddev_pop _null_ _null_ n 0));
DESCR("STDDEV_POP aggregate final function");

/* numeric_stddev_samp(_numeric) => "numeric" */ 
DATA(insert OID = 1839 ( numeric_stddev_samp  PGNSP PGUID 12 f f t f i 1 1700 f "1231" _null_ _null_ _null_ numeric_stddev_samp _null_ _null_ n 0));
DESCR("STDDEV_SAMP aggregate final function");

/* int2_sum(int8, int2) => int8 */ 
DATA(insert OID = 1840 ( int2_sum  PGNSP PGUID 12 f f f f i 2 20 f "20 21" _null_ _null_ _null_ int2_sum _null_ _null_ n 0));
DESCR("SUM(int2) transition function");

/* int4_sum(int8, int4) => int8 */ 
DATA(insert OID = 1841 ( int4_sum  PGNSP PGUID 12 f f f f i 2 20 f "20 23" _null_ _null_ _null_ int4_sum _null_ _null_ n 0));
DESCR("SUM(int4) transition function");

/* int8_sum("numeric", int8) => "numeric" */ 
DATA(insert OID = 1842 ( int8_sum  PGNSP PGUID 12 f f f f i 2 1700 f "1700 20" _null_ _null_ _null_ int8_sum _null_ _null_ n 0));
DESCR("SUM(int8) transition function");

/* int2_invsum(int8, int2) => int8 */ 
DATA(insert OID = 7008 ( int2_invsum  PGNSP PGUID 12 f f f f i 2 20 f "20 21" _null_ _null_ _null_ int2_invsum _null_ _null_ n 0));
DESCR("SUM(int2) inverse transition function");

/* int4_invsum(int8, int4) => int8 */ 
DATA(insert OID = 7009 ( int4_invsum  PGNSP PGUID 12 f f f f i 2 20 f "20 23" _null_ _null_ _null_ int4_invsum _null_ _null_ n 0));
DESCR("SUM(int4) inverse transition function");

/* int8_invsum("numeric", int8) => "numeric" */ 
DATA(insert OID = 7010 ( int8_invsum  PGNSP PGUID 12 f f f f i 2 1700 f "1700 20" _null_ _null_ _null_ int8_invsum _null_ _null_ n 0));
DESCR("SUM(int8) inverse transition function");

/* interval_accum(_interval, "interval") => _interval */ 
DATA(insert OID = 1843 ( interval_accum  PGNSP PGUID 12 f f t f i 2 1187 f "1187 1186" _null_ _null_ _null_ interval_accum _null_ _null_ n 0));
DESCR("aggregate transition function");

/* interval_decum(_interval, "interval") => _interval */ 
DATA(insert OID = 6038 ( interval_decum  PGNSP PGUID 12 f f t f i 2 1187 f "1187 1186" _null_ _null_ _null_ interval_decum _null_ _null_ n 0));
DESCR("aggregate inverse transition function");

/* interval_avg(_interval) => "interval" */ 
DATA(insert OID = 1844 ( interval_avg  PGNSP PGUID 12 f f t f i 1 1186 f "1187" _null_ _null_ _null_ interval_avg _null_ _null_ n 0));
DESCR("AVG aggregate final function");

/* int2_avg_accum(bytea, int2) => bytea */ 
DATA(insert OID = 1962 ( int2_avg_accum  PGNSP PGUID 12 f f t f i 2 17 f "17 21" _null_ _null_ _null_ int2_avg_accum _null_ _null_ n 0));
DESCR("AVG(int2) transition function");

/* int4_avg_accum(bytea, int4) => bytea */ 
DATA(insert OID = 1963 ( int4_avg_accum  PGNSP PGUID 12 f f t f i 2 17 f "17 23" _null_ _null_ _null_ int4_avg_accum _null_ _null_ n 0));
DESCR("AVG(int4) transition function");

/* int8_avg_accum(bytea, int8) => bytea */ 
DATA(insert OID = 3100 ( int8_avg_accum  PGNSP PGUID 12 f f t f i 2 17 f "17 20" _null_ _null_ _null_ int8_avg_accum _null_ _null_ n 0));
DESCR("AVG(int8) transition function");

/* int2_avg_decum(bytea, int2) => bytea */ 
DATA(insert OID = 6019 ( int2_avg_decum  PGNSP PGUID 12 f f t f i 2 17 f "17 21" _null_ _null_ _null_ int2_avg_decum _null_ _null_ n 0));
DESCR("AVG(int2) transition function");

/* int4_avg_decum(bytea, int4) => bytea */ 
DATA(insert OID = 6020 ( int4_avg_decum  PGNSP PGUID 12 f f t f i 2 17 f "17 23" _null_ _null_ _null_ int4_avg_decum _null_ _null_ n 0));
DESCR("AVG(int4) transition function");

/* int8_avg_decum(bytea, int8) => bytea */ 
DATA(insert OID = 3101 ( int8_avg_decum  PGNSP PGUID 12 f f t f i 2 17 f "17 20" _null_ _null_ _null_ int8_avg_decum _null_ _null_ n 0));
DESCR("AVG(int8) transition function");

/* int8_avg(bytea) => "numeric" */ 
DATA(insert OID = 1964 ( int8_avg  PGNSP PGUID 12 f f t f i 1 1700 f "17" _null_ _null_ _null_ int8_avg _null_ _null_ n 0));
DESCR("AVG(int) aggregate final function");

/* int8inc_float8_float8(int8, float8, float8) => int8 */ 
DATA(insert OID = 2805 ( int8inc_float8_float8  PGNSP PGUID 12 f f t f i 3 20 f "20 701 701" _null_ _null_ _null_ int8inc_float8_float8 _null_ _null_ n 0));
DESCR("REGR_COUNT(double, double) transition function");

/* float8_regr_accum(_float8, float8, float8) => _float8 */ 
DATA(insert OID = 2806 ( float8_regr_accum  PGNSP PGUID 12 f f t f i 3 1022 f "1022 701 701" _null_ _null_ _null_ float8_regr_accum _null_ _null_ n 0));
DESCR("REGR_...(double, double) transition function");

/* float8_regr_sxx(_float8) => float8 */ 
DATA(insert OID = 2807 ( float8_regr_sxx  PGNSP PGUID 12 f f t f i 1 701 f "1022" _null_ _null_ _null_ float8_regr_sxx _null_ _null_ n 0));
DESCR("REGR_SXX(double, double) aggregate final function");

/* float8_regr_syy(_float8) => float8 */ 
DATA(insert OID = 2808 ( float8_regr_syy  PGNSP PGUID 12 f f t f i 1 701 f "1022" _null_ _null_ _null_ float8_regr_syy _null_ _null_ n 0));
DESCR("REGR_SYY(double, double) aggregate final function");

/* float8_regr_sxy(_float8) => float8 */ 
DATA(insert OID = 2809 ( float8_regr_sxy  PGNSP PGUID 12 f f t f i 1 701 f "1022" _null_ _null_ _null_ float8_regr_sxy _null_ _null_ n 0));
DESCR("REGR_SXY(double, double) aggregate final function");

/* float8_regr_avgx(_float8) => float8 */ 
DATA(insert OID = 2810 ( float8_regr_avgx  PGNSP PGUID 12 f f t f i 1 701 f "1022" _null_ _null_ _null_ float8_regr_avgx _null_ _null_ n 0));
DESCR("REGR_AVGX(double, double) aggregate final function");

/* float8_regr_avgy(_float8) => float8 */ 
DATA(insert OID = 2811 ( float8_regr_avgy  PGNSP PGUID 12 f f t f i 1 701 f "1022" _null_ _null_ _null_ float8_regr_avgy _null_ _null_ n 0));
DESCR("REGR_AVGY(double, double) aggregate final function");

/* float8_regr_r2(_float8) => float8 */ 
DATA(insert OID = 2812 ( float8_regr_r2  PGNSP PGUID 12 f f t f i 1 701 f "1022" _null_ _null_ _null_ float8_regr_r2 _null_ _null_ n 0));
DESCR("REGR_R2(double, double) aggregate final function");

/* float8_regr_slope(_float8) => float8 */ 
DATA(insert OID = 2813 ( float8_regr_slope  PGNSP PGUID 12 f f t f i 1 701 f "1022" _null_ _null_ _null_ float8_regr_slope _null_ _null_ n 0));
DESCR("REGR_SLOPE(double, double) aggregate final function");

/* float8_regr_intercept(_float8) => float8 */ 
DATA(insert OID = 2814 ( float8_regr_intercept  PGNSP PGUID 12 f f t f i 1 701 f "1022" _null_ _null_ _null_ float8_regr_intercept _null_ _null_ n 0));
DESCR("REGR_INTERCEPT(double, double) aggregate final function");

/* float8_covar_pop(_float8) => float8 */ 
DATA(insert OID = 2815 ( float8_covar_pop  PGNSP PGUID 12 f f t f i 1 701 f "1022" _null_ _null_ _null_ float8_covar_pop _null_ _null_ n 0));
DESCR("COVAR_POP(double, double) aggregate final function");

/* float8_covar_samp(_float8) => float8 */ 
DATA(insert OID = 2816 ( float8_covar_samp  PGNSP PGUID 12 f f t f i 1 701 f "1022" _null_ _null_ _null_ float8_covar_samp _null_ _null_ n 0));
DESCR("COVAR_SAMP(double, double) aggregate final function");

/* float8_corr(_float8) => float8 */ 
DATA(insert OID = 2817 ( float8_corr  PGNSP PGUID 12 f f t f i 1 701 f "1022" _null_ _null_ _null_ float8_corr _null_ _null_ n 0));
DESCR("CORR(double, double) aggregate final function");

/* pg_partition_oid_transfn(internal, oid, record) => internal */ 
DATA(insert OID = 2911 ( pg_partition_oid_transfn  PGNSP PGUID 12 f f f f i 3 2281 f "2281 26 2249" _null_ _null_ _null_ pg_partition_oid_transfn _null_ _null_ n 0));
DESCR("pg_partition_oid transition function");

/* pg_partition_oid_finalfn(internal) => _oid */ 
DATA(insert OID = 2912 ( pg_partition_oid_finalfn  PGNSP PGUID 12 f f f f i 1 1028 f "2281" _null_ _null_ _null_ pg_partition_oid_finalfn _null_ _null_ n 0));
DESCR("pg_partition_oid final function");

/* pg_partition_oid(oid, record) => _oid */ 
DATA(insert OID = 2913 ( pg_partition_oid  PGNSP PGUID 12 t f f f i 2 1028 f "26 2249" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));


/* To ASCII conversion  */
/* to_ascii(text) => text */ 
DATA(insert OID = 1845 ( to_ascii  PGNSP PGUID 12 f f t f i 1 25 f "25" _null_ _null_ _null_ to_ascii_default _null_ _null_ n 0));
DESCR("encode text from DB encoding to ASCII text");

/* to_ascii(text, int4) => text */ 
DATA(insert OID = 1846 ( to_ascii  PGNSP PGUID 12 f f t f i 2 25 f "25 23" _null_ _null_ _null_ to_ascii_enc _null_ _null_ n 0));
DESCR("encode text from encoding to ASCII text");

/* to_ascii(text, name) => text */ 
DATA(insert OID = 1847 ( to_ascii  PGNSP PGUID 12 f f t f i 2 25 f "25 19" _null_ _null_ _null_ to_ascii_encname _null_ _null_ n 0));
DESCR("encode text from encoding to ASCII text");

/* interval_pl_time("interval", "time") => pg_catalog."time" */ 
DATA(insert OID = 1848 ( interval_pl_time  PGNSP PGUID 14 f f t f i 2 1083 f "1186 1083" _null_ _null_ _null_ "select $2 + $1" _null_ _null_ c 0));
DESCR("plus");

/* int28eq(int2, int8) => bool */ 
DATA(insert OID = 1850 ( int28eq  PGNSP PGUID 12 f f t f i 2 16 f "21 20" _null_ _null_ _null_ int28eq _null_ _null_ n 0));
DESCR("equal");

/* int28ne(int2, int8) => bool */ 
DATA(insert OID = 1851 ( int28ne  PGNSP PGUID 12 f f t f i 2 16 f "21 20" _null_ _null_ _null_ int28ne _null_ _null_ n 0));
DESCR("not equal");

/* int28lt(int2, int8) => bool */ 
DATA(insert OID = 1852 ( int28lt  PGNSP PGUID 12 f f t f i 2 16 f "21 20" _null_ _null_ _null_ int28lt _null_ _null_ n 0));
DESCR("less-than");

/* int28gt(int2, int8) => bool */ 
DATA(insert OID = 1853 ( int28gt  PGNSP PGUID 12 f f t f i 2 16 f "21 20" _null_ _null_ _null_ int28gt _null_ _null_ n 0));
DESCR("greater-than");

/* int28le(int2, int8) => bool */ 
DATA(insert OID = 1854 ( int28le  PGNSP PGUID 12 f f t f i 2 16 f "21 20" _null_ _null_ _null_ int28le _null_ _null_ n 0));
DESCR("less-than-or-equal");

/* int28ge(int2, int8) => bool */ 
DATA(insert OID = 1855 ( int28ge  PGNSP PGUID 12 f f t f i 2 16 f "21 20" _null_ _null_ _null_ int28ge _null_ _null_ n 0));
DESCR("greater-than-or-equal");

/* int82eq(int8, int2) => bool */ 
DATA(insert OID = 1856 ( int82eq  PGNSP PGUID 12 f f t f i 2 16 f "20 21" _null_ _null_ _null_ int82eq _null_ _null_ n 0));
DESCR("equal");

/* int82ne(int8, int2) => bool */ 
DATA(insert OID = 1857 ( int82ne  PGNSP PGUID 12 f f t f i 2 16 f "20 21" _null_ _null_ _null_ int82ne _null_ _null_ n 0));
DESCR("not equal");

/* int82lt(int8, int2) => bool */ 
DATA(insert OID = 1858 ( int82lt  PGNSP PGUID 12 f f t f i 2 16 f "20 21" _null_ _null_ _null_ int82lt _null_ _null_ n 0));
DESCR("less-than");

/* int82gt(int8, int2) => bool */ 
DATA(insert OID = 1859 ( int82gt  PGNSP PGUID 12 f f t f i 2 16 f "20 21" _null_ _null_ _null_ int82gt _null_ _null_ n 0));
DESCR("greater-than");

/* int82le(int8, int2) => bool */ 
DATA(insert OID = 1860 ( int82le  PGNSP PGUID 12 f f t f i 2 16 f "20 21" _null_ _null_ _null_ int82le _null_ _null_ n 0));
DESCR("less-than-or-equal");

/* int82ge(int8, int2) => bool */ 
DATA(insert OID = 1861 ( int82ge  PGNSP PGUID 12 f f t f i 2 16 f "20 21" _null_ _null_ _null_ int82ge _null_ _null_ n 0));
DESCR("greater-than-or-equal");

/* int2and(int2, int2) => int2 */ 
DATA(insert OID = 1892 ( int2and  PGNSP PGUID 12 f f t f i 2 21 f "21 21" _null_ _null_ _null_ int2and _null_ _null_ n 0));
DESCR("bitwise and");

/* int2or(int2, int2) => int2 */ 
DATA(insert OID = 1893 ( int2or  PGNSP PGUID 12 f f t f i 2 21 f "21 21" _null_ _null_ _null_ int2or _null_ _null_ n 0));
DESCR("bitwise or");

/* int2xor(int2, int2) => int2 */ 
DATA(insert OID = 1894 ( int2xor  PGNSP PGUID 12 f f t f i 2 21 f "21 21" _null_ _null_ _null_ int2xor _null_ _null_ n 0));
DESCR("bitwise xor");

/* int2not(int2) => int2 */ 
DATA(insert OID = 1895 ( int2not  PGNSP PGUID 12 f f t f i 1 21 f "21" _null_ _null_ _null_ int2not _null_ _null_ n 0));
DESCR("bitwise not");

/* int2shl(int2, int4) => int2 */ 
DATA(insert OID = 1896 ( int2shl  PGNSP PGUID 12 f f t f i 2 21 f "21 23" _null_ _null_ _null_ int2shl _null_ _null_ n 0));
DESCR("bitwise shift left");

/* int2shr(int2, int4) => int2 */ 
DATA(insert OID = 1897 ( int2shr  PGNSP PGUID 12 f f t f i 2 21 f "21 23" _null_ _null_ _null_ int2shr _null_ _null_ n 0));
DESCR("bitwise shift right");

/* int4and(int4, int4) => int4 */ 
DATA(insert OID = 1898 ( int4and  PGNSP PGUID 12 f f t f i 2 23 f "23 23" _null_ _null_ _null_ int4and _null_ _null_ n 0));
DESCR("bitwise and");

/* int4or(int4, int4) => int4 */ 
DATA(insert OID = 1899 ( int4or  PGNSP PGUID 12 f f t f i 2 23 f "23 23" _null_ _null_ _null_ int4or _null_ _null_ n 0));
DESCR("bitwise or");

/* int4xor(int4, int4) => int4 */ 
DATA(insert OID = 1900 ( int4xor  PGNSP PGUID 12 f f t f i 2 23 f "23 23" _null_ _null_ _null_ int4xor _null_ _null_ n 0));
DESCR("bitwise xor");

/* int4not(int4) => int4 */ 
DATA(insert OID = 1901 ( int4not  PGNSP PGUID 12 f f t f i 1 23 f "23" _null_ _null_ _null_ int4not _null_ _null_ n 0));
DESCR("bitwise not");

/* int4shl(int4, int4) => int4 */ 
DATA(insert OID = 1902 ( int4shl  PGNSP PGUID 12 f f t f i 2 23 f "23 23" _null_ _null_ _null_ int4shl _null_ _null_ n 0));
DESCR("bitwise shift left");

/* int4shr(int4, int4) => int4 */ 
DATA(insert OID = 1903 ( int4shr  PGNSP PGUID 12 f f t f i 2 23 f "23 23" _null_ _null_ _null_ int4shr _null_ _null_ n 0));
DESCR("bitwise shift right");

/* int8and(int8, int8) => int8 */ 
DATA(insert OID = 1904 ( int8and  PGNSP PGUID 12 f f t f i 2 20 f "20 20" _null_ _null_ _null_ int8and _null_ _null_ n 0));
DESCR("bitwise and");

/* int8or(int8, int8) => int8 */ 
DATA(insert OID = 1905 ( int8or  PGNSP PGUID 12 f f t f i 2 20 f "20 20" _null_ _null_ _null_ int8or _null_ _null_ n 0));
DESCR("bitwise or");

/* int8xor(int8, int8) => int8 */ 
DATA(insert OID = 1906 ( int8xor  PGNSP PGUID 12 f f t f i 2 20 f "20 20" _null_ _null_ _null_ int8xor _null_ _null_ n 0));
DESCR("bitwise xor");

/* int8not(int8) => int8 */ 
DATA(insert OID = 1907 ( int8not  PGNSP PGUID 12 f f t f i 1 20 f "20" _null_ _null_ _null_ int8not _null_ _null_ n 0));
DESCR("bitwise not");

/* int8shl(int8, int4) => int8 */ 
DATA(insert OID = 1908 ( int8shl  PGNSP PGUID 12 f f t f i 2 20 f "20 23" _null_ _null_ _null_ int8shl _null_ _null_ n 0));
DESCR("bitwise shift left");

/* int8shr(int8, int4) => int8 */ 
DATA(insert OID = 1909 ( int8shr  PGNSP PGUID 12 f f t f i 2 20 f "20 23" _null_ _null_ _null_ int8shr _null_ _null_ n 0));
DESCR("bitwise shift right");

/* int8up(int8) => int8 */ 
DATA(insert OID = 1910 ( int8up  PGNSP PGUID 12 f f t f i 1 20 f "20" _null_ _null_ _null_ int8up _null_ _null_ n 0));
DESCR("unary plus");

/* int2up(int2) => int2 */ 
DATA(insert OID = 1911 ( int2up  PGNSP PGUID 12 f f t f i 1 21 f "21" _null_ _null_ _null_ int2up _null_ _null_ n 0));
DESCR("unary plus");

/* int4up(int4) => int4 */ 
DATA(insert OID = 1912 ( int4up  PGNSP PGUID 12 f f t f i 1 23 f "23" _null_ _null_ _null_ int4up _null_ _null_ n 0));
DESCR("unary plus");

/* float4up(float4) => float4 */ 
DATA(insert OID = 1913 ( float4up  PGNSP PGUID 12 f f t f i 1 700 f "700" _null_ _null_ _null_ float4up _null_ _null_ n 0));
DESCR("unary plus");

/* float8up(float8) => float8 */ 
DATA(insert OID = 1914 ( float8up  PGNSP PGUID 12 f f t f i 1 701 f "701" _null_ _null_ _null_ float8up _null_ _null_ n 0));
DESCR("unary plus");

/* numeric_uplus("numeric") => "numeric" */ 
DATA(insert OID = 1915 ( numeric_uplus  PGNSP PGUID 12 f f t f i 1 1700 f "1700" _null_ _null_ _null_ numeric_uplus _null_ _null_ n 0));
DESCR("unary plus");

/* has_table_privilege(name, text, text) => bool */ 
DATA(insert OID = 1922 ( has_table_privilege  PGNSP PGUID 12 f f t f s 3 16 f "19 25 25" _null_ _null_ _null_ has_table_privilege_name_name _null_ _null_ n 0));
DESCR("user privilege on relation by username, rel name");

/* has_table_privilege(name, oid, text) => bool */ 
DATA(insert OID = 1923 ( has_table_privilege  PGNSP PGUID 12 f f t f s 3 16 f "19 26 25" _null_ _null_ _null_ has_table_privilege_name_id _null_ _null_ n 0));
DESCR("user privilege on relation by username, rel oid");

/* has_table_privilege(oid, text, text) => bool */ 
DATA(insert OID = 1924 ( has_table_privilege  PGNSP PGUID 12 f f t f s 3 16 f "26 25 25" _null_ _null_ _null_ has_table_privilege_id_name _null_ _null_ n 0));
DESCR("user privilege on relation by user oid, rel name");

/* has_table_privilege(oid, oid, text) => bool */ 
DATA(insert OID = 1925 ( has_table_privilege  PGNSP PGUID 12 f f t f s 3 16 f "26 26 25" _null_ _null_ _null_ has_table_privilege_id_id _null_ _null_ n 0));
DESCR("user privilege on relation by user oid, rel oid");

/* has_table_privilege(text, text) => bool */ 
DATA(insert OID = 1926 ( has_table_privilege  PGNSP PGUID 12 f f t f s 2 16 f "25 25" _null_ _null_ _null_ has_table_privilege_name _null_ _null_ n 0));
DESCR("current user privilege on relation by rel name");

/* has_table_privilege(oid, text) => bool */ 
DATA(insert OID = 1927 ( has_table_privilege  PGNSP PGUID 12 f f t f s 2 16 f "26 25" _null_ _null_ _null_ has_table_privilege_id _null_ _null_ n 0));
DESCR("current user privilege on relation by rel oid");

/* pg_stat_get_numscans(oid) => int8 */ 
DATA(insert OID = 1928 ( pg_stat_get_numscans  PGNSP PGUID 12 f f t f s 1 20 f "26" _null_ _null_ _null_ pg_stat_get_numscans _null_ _null_ n 0));
DESCR("Statistics: Number of scans done for table/index");

/* pg_stat_get_tuples_returned(oid) => int8 */ 
DATA(insert OID = 1929 ( pg_stat_get_tuples_returned  PGNSP PGUID 12 f f t f s 1 20 f "26" _null_ _null_ _null_ pg_stat_get_tuples_returned _null_ _null_ n 0));
DESCR("Statistics: Number of tuples read by seqscan");

/* pg_stat_get_tuples_fetched(oid) => int8 */ 
DATA(insert OID = 1930 ( pg_stat_get_tuples_fetched  PGNSP PGUID 12 f f t f s 1 20 f "26" _null_ _null_ _null_ pg_stat_get_tuples_fetched _null_ _null_ n 0));
DESCR("Statistics: Number of tuples fetched by idxscan");

/* pg_stat_get_tuples_inserted(oid) => int8 */ 
DATA(insert OID = 1931 ( pg_stat_get_tuples_inserted  PGNSP PGUID 12 f f t f s 1 20 f "26" _null_ _null_ _null_ pg_stat_get_tuples_inserted _null_ _null_ n 0));
DESCR("Statistics: Number of tuples inserted");

/* pg_stat_get_tuples_updated(oid) => int8 */ 
DATA(insert OID = 1932 ( pg_stat_get_tuples_updated  PGNSP PGUID 12 f f t f s 1 20 f "26" _null_ _null_ _null_ pg_stat_get_tuples_updated _null_ _null_ n 0));
DESCR("Statistics: Number of tuples updated");

/* pg_stat_get_tuples_deleted(oid) => int8 */ 
DATA(insert OID = 1933 ( pg_stat_get_tuples_deleted  PGNSP PGUID 12 f f t f s 1 20 f "26" _null_ _null_ _null_ pg_stat_get_tuples_deleted _null_ _null_ n 0));
DESCR("Statistics: Number of tuples deleted");

/* pg_stat_get_blocks_fetched(oid) => int8 */ 
DATA(insert OID = 1934 ( pg_stat_get_blocks_fetched  PGNSP PGUID 12 f f t f s 1 20 f "26" _null_ _null_ _null_ pg_stat_get_blocks_fetched _null_ _null_ n 0));
DESCR("Statistics: Number of blocks fetched");

/* pg_stat_get_blocks_hit(oid) => int8 */ 
DATA(insert OID = 1935 ( pg_stat_get_blocks_hit  PGNSP PGUID 12 f f t f s 1 20 f "26" _null_ _null_ _null_ pg_stat_get_blocks_hit _null_ _null_ n 0));
DESCR("Statistics: Number of blocks found in cache");

/* pg_stat_get_last_vacuum_time(oid) => timestamptz */ 
DATA(insert OID = 2781 ( pg_stat_get_last_vacuum_time  PGNSP PGUID 12 f f t f s 1 1184 f "26" _null_ _null_ _null_ pg_stat_get_last_vacuum_time _null_ _null_ n 0));
DESCR("Statistics: Last manual vacuum time for a table");

/* pg_stat_get_last_autovacuum_time(oid) => timestamptz */ 
DATA(insert OID = 2782 ( pg_stat_get_last_autovacuum_time  PGNSP PGUID 12 f f t f s 1 1184 f "26" _null_ _null_ _null_ pg_stat_get_last_autovacuum_time _null_ _null_ n 0));
DESCR("Statistics: Last auto vacuum time for a table");

/* pg_stat_get_last_analyze_time(oid) => timestamptz */ 
DATA(insert OID = 2783 ( pg_stat_get_last_analyze_time  PGNSP PGUID 12 f f t f s 1 1184 f "26" _null_ _null_ _null_ pg_stat_get_last_analyze_time _null_ _null_ n 0));
DESCR("Statistics: Last manual analyze time for a table");

/* pg_stat_get_last_autoanalyze_time(oid) => timestamptz */ 
DATA(insert OID = 2784 ( pg_stat_get_last_autoanalyze_time  PGNSP PGUID 12 f f t f s 1 1184 f "26" _null_ _null_ _null_ pg_stat_get_last_autoanalyze_time _null_ _null_ n 0));
DESCR("Statistics: Last auto analyze time for a table");

/* pg_stat_get_backend_idset() => SETOF int4 */ 
DATA(insert OID = 1936 ( pg_stat_get_backend_idset  PGNSP PGUID 12 f f t t s 0 23 f "" _null_ _null_ _null_ pg_stat_get_backend_idset _null_ _null_ n 0));
DESCR("Statistics: Currently active backend IDs");

/* pg_backend_pid() => int4 */ 
DATA(insert OID = 2026 ( pg_backend_pid  PGNSP PGUID 12 f f t f s 0 23 f "" _null_ _null_ _null_ pg_backend_pid _null_ _null_ n 0));
DESCR("Statistics: Current backend PID");

/* pg_stat_reset() => bool */ 
DATA(insert OID = 2274 ( pg_stat_reset  PGNSP PGUID 12 f f f f v 0 16 f "" _null_ _null_ _null_ pg_stat_reset _null_ _null_ n 0));
DESCR("Statistics: Reset collected statistics");

/* pg_stat_get_backend_pid(int4) => int4 */ 
DATA(insert OID = 1937 ( pg_stat_get_backend_pid  PGNSP PGUID 12 f f t f s 1 23 f "23" _null_ _null_ _null_ pg_stat_get_backend_pid _null_ _null_ n 0));
DESCR("Statistics: PID of backend");

/* pg_stat_get_backend_dbid(int4) => oid */ 
DATA(insert OID = 1938 ( pg_stat_get_backend_dbid  PGNSP PGUID 12 f f t f s 1 26 f "23" _null_ _null_ _null_ pg_stat_get_backend_dbid _null_ _null_ n 0));
DESCR("Statistics: Database ID of backend");

/* pg_stat_get_backend_userid(int4) => oid */ 
DATA(insert OID = 1939 ( pg_stat_get_backend_userid  PGNSP PGUID 12 f f t f s 1 26 f "23" _null_ _null_ _null_ pg_stat_get_backend_userid _null_ _null_ n 0));
DESCR("Statistics: User ID of backend");

/* pg_stat_get_backend_activity(int4) => text */ 
DATA(insert OID = 1940 ( pg_stat_get_backend_activity  PGNSP PGUID 12 f f t f s 1 25 f "23" _null_ _null_ _null_ pg_stat_get_backend_activity _null_ _null_ n 0));
DESCR("Statistics: Current query of backend");

/* pg_stat_get_backend_waiting(int4) => bool */ 
DATA(insert OID = 2853 ( pg_stat_get_backend_waiting  PGNSP PGUID 12 f f t f s 1 16 f "23" _null_ _null_ _null_ pg_stat_get_backend_waiting _null_ _null_ n 0));
DESCR("Statistics: Is backend currently waiting for a lock");

/* pg_stat_get_backend_activity_start(int4) => timestamptz */ 
DATA(insert OID = 2094 ( pg_stat_get_backend_activity_start  PGNSP PGUID 12 f f t f s 1 1184 f "23" _null_ _null_ _null_ pg_stat_get_backend_activity_start _null_ _null_ n 0));
DESCR("Statistics: Start time for current query of backend");

/* pg_stat_get_backend_start(int4) => timestamptz */ 
DATA(insert OID = 1391 ( pg_stat_get_backend_start  PGNSP PGUID 12 f f t f s 1 1184 f "23" _null_ _null_ _null_ pg_stat_get_backend_start _null_ _null_ n 0));
DESCR("Statistics: Start time for current backend session");

/* pg_stat_get_backend_client_addr(int4) => inet */ 
DATA(insert OID = 1392 ( pg_stat_get_backend_client_addr  PGNSP PGUID 12 f f t f s 1 869 f "23" _null_ _null_ _null_ pg_stat_get_backend_client_addr _null_ _null_ n 0));
DESCR("Statistics: Address of client connected to backend");

/* pg_stat_get_backend_client_port(int4) => int4 */ 
DATA(insert OID = 1393 ( pg_stat_get_backend_client_port  PGNSP PGUID 12 f f t f s 1 23 f "23" _null_ _null_ _null_ pg_stat_get_backend_client_port _null_ _null_ n 0));
DESCR("Statistics: Port number of client connected to backend");

/* pg_stat_get_backend_waiting_reason(int4) => text */ 
DATA(insert OID = 2972 ( pg_stat_get_backend_waiting_reason  PGNSP PGUID 12 f f t f s 1 25 f "23" _null_ _null_ _null_ pg_stat_get_backend_waiting_reason _null_ _null_ n 0));
DESCR("Statistics: Reason backend is waiting for");

/* pg_stat_get_db_numbackends(oid) => int4 */ 
DATA(insert OID = 1941 ( pg_stat_get_db_numbackends  PGNSP PGUID 12 f f t f s 1 23 f "26" _null_ _null_ _null_ pg_stat_get_db_numbackends _null_ _null_ n 0));
DESCR("Statistics: Number of backends in database");

/* pg_stat_get_db_xact_commit(oid) => int8 */ 
DATA(insert OID = 1942 ( pg_stat_get_db_xact_commit  PGNSP PGUID 12 f f t f s 1 20 f "26" _null_ _null_ _null_ pg_stat_get_db_xact_commit _null_ _null_ n 0));
DESCR("Statistics: Transactions committed");

/* pg_stat_get_db_xact_rollback(oid) => int8 */ 
DATA(insert OID = 1943 ( pg_stat_get_db_xact_rollback  PGNSP PGUID 12 f f t f s 1 20 f "26" _null_ _null_ _null_ pg_stat_get_db_xact_rollback _null_ _null_ n 0));
DESCR("Statistics: Transactions rolled back");

/* pg_stat_get_db_blocks_fetched(oid) => int8 */ 
DATA(insert OID = 1944 ( pg_stat_get_db_blocks_fetched  PGNSP PGUID 12 f f t f s 1 20 f "26" _null_ _null_ _null_ pg_stat_get_db_blocks_fetched _null_ _null_ n 0));
DESCR("Statistics: Blocks fetched for database");

/* pg_stat_get_db_blocks_hit(oid) => int8 */ 
DATA(insert OID = 1945 ( pg_stat_get_db_blocks_hit  PGNSP PGUID 12 f f t f s 1 20 f "26" _null_ _null_ _null_ pg_stat_get_db_blocks_hit _null_ _null_ n 0));
DESCR("Statistics: Blocks found in cache for database");

/* pg_stat_get_queue_num_exec(oid) => int8 */ 
DATA(insert OID = 6031 ( pg_stat_get_queue_num_exec  PGNSP PGUID 12 f f t f s 1 20 f "26" _null_ _null_ _null_ pg_stat_get_queue_num_exec _null_ _null_ n 0));
DESCR("Statistics: Number of queries that executed in queue");

/* pg_stat_get_queue_num_wait(oid) => int8 */ 
DATA(insert OID = 6032 ( pg_stat_get_queue_num_wait  PGNSP PGUID 12 f f t f s 1 20 f "26" _null_ _null_ _null_ pg_stat_get_queue_num_wait _null_ _null_ n 0));
DESCR("Statistics: Number of queries that waited in queue");

/* pg_stat_get_queue_elapsed_exec(oid) => int8 */ 
DATA(insert OID = 6033 ( pg_stat_get_queue_elapsed_exec  PGNSP PGUID 12 f f t f s 1 20 f "26" _null_ _null_ _null_ pg_stat_get_queue_elapsed_exec _null_ _null_ n 0));
DESCR("Statistics:  Elapsed seconds for queries that executed in queue");

/* pg_stat_get_queue_elapsed_wait(oid) => int8 */ 
DATA(insert OID = 6034 ( pg_stat_get_queue_elapsed_wait  PGNSP PGUID 12 f f t f s 1 20 f "26" _null_ _null_ _null_ pg_stat_get_queue_elapsed_wait _null_ _null_ n 0));
DESCR("Statistics:  Elapsed seconds for queries that waited in queue");

/* pg_stat_get_backend_session_id(int4) => int4 */ 
DATA(insert OID = 6039 ( pg_stat_get_backend_session_id  PGNSP PGUID 12 f f t f s 1 23 f "23" _null_ _null_ _null_ pg_stat_get_backend_session_id _null_ _null_ n 0));
DESCR("Statistics: Greenplum session id of backend");

/* pg_renice_session(int4, int4) => int4 */ 
DATA(insert OID = 6042 ( pg_renice_session  PGNSP PGUID 12 f f t f v 2 23 f "23 23" _null_ _null_ _null_ pg_renice_session _null_ _null_ n 0));
DESCR("change priority of all the backends for a given session id");

/* pg_stat_get_activity(IN pid int4, OUT datid oid, OUT procpid int4, OUT usesysid oid, OUT application_name text, OUT current_query text, OUT waiting bool, OUT xact_start timestamptz, OUT query_start timestamptz, OUT backend_start timestamptz, OUT client_addr inet, OUT client_port int4, OUT sess_id int4, OUT waiting_reason text) => SETOF pg_catalog.record */ 
DATA(insert OID = 6071 ( pg_stat_get_activity  PGNSP PGUID 12 f f f t v 1 2249 f "23" "{23,26,23,26,25,25,16,1184,1184,1184,869,23,23,25}" "{i,o,o,o,o,o,o,o,o,o,o,o,o,o}" "{pid,datid,procpid,usesysid,application_name,current_query,waiting,xact_start,query_start,backend_start,client_addr,client_port,sess_id,waiting_reason}" pg_stat_get_activity _null_ _null_ n 0));
DESCR("statistics: information about currently active backends");

/* pg_stat_get_wal_senders(OUT pid int4, OUT state text, OUT sent_location text, OUT write_location text, OUT flush_location text, OUT replay_location text, OUT sync_priority int4, OUT sync_state text) => SETOF pg_catalog.record */ 
DATA(insert OID = 3099 ( pg_stat_get_wal_senders  PGNSP PGUID 12 f f f t s 0 2249 f "" "{23,25,25,25,25,25,23,25}" "{o,o,o,o,o,o,o,o}" "{pid,state,sent_location,write_location,flush_location,replay_location,sync_priority,sync_state}" pg_stat_get_wal_senders _null_ _null_ n 0));
DESCR("statistics: information about currently active replication");

/* encode(bytea, text) => text */ 
DATA(insert OID = 1946 ( encode  PGNSP PGUID 12 f f t f i 2 25 f "17 25" _null_ _null_ _null_ binary_encode _null_ _null_ n 0));
DESCR("Convert bytea value into some ascii-only text string");

/* "decode"(text, text) => bytea */ 
DATA(insert OID = 1947 ( decode  PGNSP PGUID 12 f f t f i 2 17 f "25 25" _null_ _null_ _null_ binary_decode _null_ _null_ n 0));
DESCR("Convert ascii-encoded text string into bytea value");

/* byteaeq(bytea, bytea) => bool */ 
DATA(insert OID = 1948 ( byteaeq  PGNSP PGUID 12 f f t f i 2 16 f "17 17" _null_ _null_ _null_ byteaeq _null_ _null_ n 0));
DESCR("equal");

/* bytealt(bytea, bytea) => bool */ 
DATA(insert OID = 1949 ( bytealt  PGNSP PGUID 12 f f t f i 2 16 f "17 17" _null_ _null_ _null_ bytealt _null_ _null_ n 0));
DESCR("less-than");

/* byteale(bytea, bytea) => bool */ 
DATA(insert OID = 1950 ( byteale  PGNSP PGUID 12 f f t f i 2 16 f "17 17" _null_ _null_ _null_ byteale _null_ _null_ n 0));
DESCR("less-than-or-equal");

/* byteagt(bytea, bytea) => bool */ 
DATA(insert OID = 1951 ( byteagt  PGNSP PGUID 12 f f t f i 2 16 f "17 17" _null_ _null_ _null_ byteagt _null_ _null_ n 0));
DESCR("greater-than");

/* byteage(bytea, bytea) => bool */ 
DATA(insert OID = 1952 ( byteage  PGNSP PGUID 12 f f t f i 2 16 f "17 17" _null_ _null_ _null_ byteage _null_ _null_ n 0));
DESCR("greater-than-or-equal");

/* byteane(bytea, bytea) => bool */ 
DATA(insert OID = 1953 ( byteane  PGNSP PGUID 12 f f t f i 2 16 f "17 17" _null_ _null_ _null_ byteane _null_ _null_ n 0));
DESCR("not equal");

/* byteacmp(bytea, bytea) => int4 */ 
DATA(insert OID = 1954 ( byteacmp  PGNSP PGUID 12 f f t f i 2 23 f "17 17" _null_ _null_ _null_ byteacmp _null_ _null_ n 0));
DESCR("less-equal-greater");

/* "timestamp"("timestamp", int4) => "timestamp" */ 
DATA(insert OID = 1961 ( timestamp  PGNSP PGUID 12 f f t f i 2 1114 f "1114 23" _null_ _null_ _null_ timestamp_scale _null_ _null_ n 0));
DESCR("adjust timestamp precision");

/* oidlarger(oid, oid) => oid */ 
DATA(insert OID = 1965 ( oidlarger  PGNSP PGUID 12 f f t f i 2 26 f "26 26" _null_ _null_ _null_ oidlarger _null_ _null_ n 0));
DESCR("larger of two");

/* oidsmaller(oid, oid) => oid */ 
DATA(insert OID = 1966 ( oidsmaller  PGNSP PGUID 12 f f t f i 2 26 f "26 26" _null_ _null_ _null_ oidsmaller _null_ _null_ n 0));
DESCR("smaller of two");

/* timestamptz(timestamptz, int4) => timestamptz */ 
DATA(insert OID = 1967 ( timestamptz  PGNSP PGUID 12 f f t f i 2 1184 f "1184 23" _null_ _null_ _null_ timestamptz_scale _null_ _null_ n 0));
DESCR("adjust timestamptz precision");

/* "time"("time", int4) => "time" */ 
DATA(insert OID = 1968 ( time  PGNSP PGUID 12 f f t f i 2 1083 f "1083 23" _null_ _null_ _null_ time_scale _null_ _null_ n 0));
DESCR("adjust time precision");

/* timetz(timetz, int4) => timetz */ 
DATA(insert OID = 1969 ( timetz  PGNSP PGUID 12 f f t f i 2 1266 f "1266 23" _null_ _null_ _null_ timetz_scale _null_ _null_ n 0));
DESCR("adjust time with time zone precision");

/* bytealike(bytea, bytea) => bool */ 
DATA(insert OID = 2005 ( bytealike  PGNSP PGUID 12 f f t f i 2 16 f "17 17" _null_ _null_ _null_ bytealike _null_ _null_ n 0));
DESCR("matches LIKE expression");

/* byteanlike(bytea, bytea) => bool */ 
DATA(insert OID = 2006 ( byteanlike  PGNSP PGUID 12 f f t f i 2 16 f "17 17" _null_ _null_ _null_ byteanlike _null_ _null_ n 0));
DESCR("does not match LIKE expression");

/* "like"(bytea, bytea) => bool */ 
DATA(insert OID = 2007 ( like  PGNSP PGUID 12 f f t f i 2 16 f "17 17" _null_ _null_ _null_ bytealike _null_ _null_ n 0));
DESCR("matches LIKE expression");

/* notlike(bytea, bytea) => bool */ 
DATA(insert OID = 2008 ( notlike  PGNSP PGUID 12 f f t f i 2 16 f "17 17" _null_ _null_ _null_ byteanlike _null_ _null_ n 0));
DESCR("does not match LIKE expression");

/* like_escape(bytea, bytea) => bytea */ 
DATA(insert OID = 2009 ( like_escape  PGNSP PGUID 12 f f t f i 2 17 f "17 17" _null_ _null_ _null_ like_escape_bytea _null_ _null_ n 0));
DESCR("convert LIKE pattern to use backslash escapes");

/* length(bytea) => int4 */ 
DATA(insert OID = 2010 ( length  PGNSP PGUID 12 f f t f i 1 23 f "17" _null_ _null_ _null_ byteaoctetlen _null_ _null_ n 0));
DESCR("octet length");

/* byteacat(bytea, bytea) => bytea */ 
DATA(insert OID = 2011 ( byteacat  PGNSP PGUID 12 f f t f i 2 17 f "17 17" _null_ _null_ _null_ byteacat _null_ _null_ n 0));
DESCR("concatenate");

/* "substring"(bytea, int4, int4) => bytea */ 
DATA(insert OID = 2012 ( substring  PGNSP PGUID 12 f f t f i 3 17 f "17 23 23" _null_ _null_ _null_ bytea_substr _null_ _null_ n 0));
DESCR("return portion of string");

/* "substring"(bytea, int4) => bytea */ 
DATA(insert OID = 2013 ( substring  PGNSP PGUID 12 f f t f i 2 17 f "17 23" _null_ _null_ _null_ bytea_substr_no_len _null_ _null_ n 0));
DESCR("return portion of string");

/* substr(bytea, int4, int4) => bytea */ 
DATA(insert OID = 2085 ( substr  PGNSP PGUID 12 f f t f i 3 17 f "17 23 23" _null_ _null_ _null_ bytea_substr _null_ _null_ n 0));
DESCR("return portion of string");

/* substr(bytea, int4) => bytea */ 
DATA(insert OID = 2086 ( substr  PGNSP PGUID 12 f f t f i 2 17 f "17 23" _null_ _null_ _null_ bytea_substr_no_len _null_ _null_ n 0));
DESCR("return portion of string");

/* "position"(bytea, bytea) => int4 */ 
DATA(insert OID = 2014 ( position  PGNSP PGUID 12 f f t f i 2 23 f "17 17" _null_ _null_ _null_ byteapos _null_ _null_ n 0));
DESCR("return position of substring");

/* btrim(bytea, bytea) => bytea */ 
DATA(insert OID = 2015 ( btrim  PGNSP PGUID 12 f f t f i 2 17 f "17 17" _null_ _null_ _null_ byteatrim _null_ _null_ n 0));
DESCR("trim both ends of string");

/* "time"(timestamptz) => "time" */ 
DATA(insert OID = 2019 ( time  PGNSP PGUID 12 f f t f s 1 1083 f "1184" _null_ _null_ _null_ timestamptz_time _null_ _null_ n 0));
DESCR("convert timestamptz to time");

/* date_trunc(text, "timestamp") => "timestamp" */ 
DATA(insert OID = 2020 ( date_trunc  PGNSP PGUID 12 f f t f i 2 1114 f "25 1114" _null_ _null_ _null_ timestamp_trunc _null_ _null_ n 0));
DESCR("truncate timestamp to specified units");

/* date_part(text, "timestamp") => float8 */ 
DATA(insert OID = 2021 ( date_part  PGNSP PGUID 12 f f t f i 2 701 f "25 1114" _null_ _null_ _null_ timestamp_part _null_ _null_ n 0));
DESCR("extract field from timestamp");

/* "timestamp"(text) => "timestamp" */ 
DATA(insert OID = 2022 ( timestamp  PGNSP PGUID 12 f f t f s 1 1114 f "25" _null_ _null_ _null_ text_timestamp _null_ _null_ n 0));
DESCR("convert text to timestamp");

/* "timestamp"(abstime) => "timestamp" */ 
DATA(insert OID = 2023 ( timestamp  PGNSP PGUID 12 f f t f s 1 1114 f "702" _null_ _null_ _null_ abstime_timestamp _null_ _null_ n 0));
DESCR("convert abstime to timestamp");

/* "timestamp"(date) => "timestamp" */ 
DATA(insert OID = 2024 ( timestamp  PGNSP PGUID 12 f f t f i 1 1114 f "1082" _null_ _null_ _null_ date_timestamp _null_ _null_ n 0));
DESCR("convert date to timestamp");

/* "timestamp"(date, "time") => "timestamp" */ 
DATA(insert OID = 2025 ( timestamp  PGNSP PGUID 12 f f t f i 2 1114 f "1082 1083" _null_ _null_ _null_ datetime_timestamp _null_ _null_ n 0));
DESCR("convert date and time to timestamp");

/* "timestamp"(timestamptz) => "timestamp" */ 
DATA(insert OID = 2027 ( timestamp  PGNSP PGUID 12 f f t f s 1 1114 f "1184" _null_ _null_ _null_ timestamptz_timestamp _null_ _null_ n 0));
DESCR("convert timestamp with time zone to timestamp");

/* timestamptz("timestamp") => timestamptz */ 
DATA(insert OID = 2028 ( timestamptz  PGNSP PGUID 12 f f t f s 1 1184 f "1114" _null_ _null_ _null_ timestamp_timestamptz _null_ _null_ n 0));
DESCR("convert timestamp to timestamp with time zone");

/* date("timestamp") => date */ 
DATA(insert OID = 2029 ( date  PGNSP PGUID 12 f f t f i 1 1082 f "1114" _null_ _null_ _null_ timestamp_date _null_ _null_ n 0));
DESCR("convert timestamp to date");

/* abstime("timestamp") => abstime */ 
DATA(insert OID = 2030 ( abstime  PGNSP PGUID 12 f f t f s 1 702 f "1114" _null_ _null_ _null_ timestamp_abstime _null_ _null_ n 0));
DESCR("convert timestamp to abstime");

/* timestamp_mi("timestamp", "timestamp") => "interval" */ 
DATA(insert OID = 2031 ( timestamp_mi  PGNSP PGUID 12 f f t f i 2 1186 f "1114 1114" _null_ _null_ _null_ timestamp_mi _null_ _null_ n 0));
DESCR("subtract");

/* timestamp_pl_interval("timestamp", "interval") => "timestamp" */ 
DATA(insert OID = 2032 ( timestamp_pl_interval  PGNSP PGUID 12 f f t f i 2 1114 f "1114 1186" _null_ _null_ _null_ timestamp_pl_interval _null_ _null_ n 0));
DESCR("plus");

/* timestamp_mi_interval("timestamp", "interval") => "timestamp" */ 
DATA(insert OID = 2033 ( timestamp_mi_interval  PGNSP PGUID 12 f f t f i 2 1114 f "1114 1186" _null_ _null_ _null_ timestamp_mi_interval _null_ _null_ n 0));
DESCR("minus");

/* text("timestamp") => text */ 
DATA(insert OID = 2034 ( text  PGNSP PGUID 12 f f t f s 1 25 f "1114" _null_ _null_ _null_ timestamp_text _null_ _null_ n 0));
DESCR("convert timestamp to text");

/* timestamp_smaller("timestamp", "timestamp") => "timestamp" */ 
DATA(insert OID = 2035 ( timestamp_smaller  PGNSP PGUID 12 f f t f i 2 1114 f "1114 1114" _null_ _null_ _null_ timestamp_smaller _null_ _null_ n 0));
DESCR("smaller of two");

/* timestamp_larger("timestamp", "timestamp") => "timestamp" */ 
DATA(insert OID = 2036 ( timestamp_larger  PGNSP PGUID 12 f f t f i 2 1114 f "1114 1114" _null_ _null_ _null_ timestamp_larger _null_ _null_ n 0));
DESCR("larger of two");

/* timezone(text, timetz) => timetz */ 
DATA(insert OID = 2037 ( timezone  PGNSP PGUID 12 f f t f v 2 1266 f "25 1266" _null_ _null_ _null_ timetz_zone _null_ _null_ n 0));
DESCR("adjust time with time zone to new zone");

/* timezone("interval", timetz) => timetz */ 
DATA(insert OID = 2038 ( timezone  PGNSP PGUID 12 f f t f i 2 1266 f "1186 1266" _null_ _null_ _null_ timetz_izone _null_ _null_ n 0));
DESCR("adjust time with time zone to new zone");

/* "overlaps"("timestamp", "timestamp", "timestamp", "timestamp") => bool */ 
DATA(insert OID = 2041 ( overlaps  PGNSP PGUID 12 f f f f i 4 16 f "1114 1114 1114 1114" _null_ _null_ _null_ overlaps_timestamp _null_ _null_ n 0));
DESCR("SQL92 interval comparison");

/* "overlaps"("timestamp", "interval", "timestamp", "interval") => pg_catalog.bool */ 
DATA(insert OID = 2042 ( overlaps  PGNSP PGUID 14 f f f f i 4 16 f "1114 1186 1114 1186" _null_ _null_ _null_ "select ($1, ($1 + $2)) overlaps ($3, ($3 + $4))" _null_ _null_ c 0));
DESCR("SQL92 interval comparison");

/* "overlaps"("timestamp", "timestamp", "timestamp", "interval") => pg_catalog.bool */ 
DATA(insert OID = 2043 ( overlaps  PGNSP PGUID 14 f f f f i 4 16 f "1114 1114 1114 1186" _null_ _null_ _null_ "select ($1, $2) overlaps ($3, ($3 + $4))" _null_ _null_ c 0));
DESCR("SQL92 interval comparison");

/* "overlaps"("timestamp", "interval", "timestamp", "timestamp") => pg_catalog.bool */ 
DATA(insert OID = 2044 ( overlaps  PGNSP PGUID 14 f f f f i 4 16 f "1114 1186 1114 1114" _null_ _null_ _null_ "select ($1, ($1 + $2)) overlaps ($3, $4)" _null_ _null_ c 0));
DESCR("SQL92 interval comparison");

/* timestamp_cmp("timestamp", "timestamp") => int4 */ 
DATA(insert OID = 2045 ( timestamp_cmp  PGNSP PGUID 12 f f t f i 2 23 f "1114 1114" _null_ _null_ _null_ timestamp_cmp _null_ _null_ n 0));
DESCR("less-equal-greater");

/* "time"(timetz) => "time" */ 
DATA(insert OID = 2046 ( time  PGNSP PGUID 12 f f t f i 1 1083 f "1266" _null_ _null_ _null_ timetz_time _null_ _null_ n 0));
DESCR("convert time with time zone to time");

/* timetz("time") => timetz */ 
DATA(insert OID = 2047 ( timetz  PGNSP PGUID 12 f f t f s 1 1266 f "1083" _null_ _null_ _null_ time_timetz _null_ _null_ n 0));
DESCR("convert time to timetz");

/* isfinite("timestamp") => bool */ 
DATA(insert OID = 2048 ( isfinite  PGNSP PGUID 12 f f t f i 1 16 f "1114" _null_ _null_ _null_ timestamp_finite _null_ _null_ n 0));
DESCR("finite timestamp?");

/* to_char("timestamp", text) => text */ 
DATA(insert OID = 2049 ( to_char  PGNSP PGUID 12 f f t f s 2 25 f "1114 25" _null_ _null_ _null_ timestamp_to_char _null_ _null_ n 0));
DESCR("format timestamp to text");

/* timestamp_eq("timestamp", "timestamp") => bool */ 
DATA(insert OID = 2052 ( timestamp_eq  PGNSP PGUID 12 f f t f i 2 16 f "1114 1114" _null_ _null_ _null_ timestamp_eq _null_ _null_ n 0));
DESCR("equal");

/* timestamp_ne("timestamp", "timestamp") => bool */ 
DATA(insert OID = 2053 ( timestamp_ne  PGNSP PGUID 12 f f t f i 2 16 f "1114 1114" _null_ _null_ _null_ timestamp_ne _null_ _null_ n 0));
DESCR("not equal");

/* timestamp_lt("timestamp", "timestamp") => bool */ 
DATA(insert OID = 2054 ( timestamp_lt  PGNSP PGUID 12 f f t f i 2 16 f "1114 1114" _null_ _null_ _null_ timestamp_lt _null_ _null_ n 0));
DESCR("less-than");

/* timestamp_le("timestamp", "timestamp") => bool */ 
DATA(insert OID = 2055 ( timestamp_le  PGNSP PGUID 12 f f t f i 2 16 f "1114 1114" _null_ _null_ _null_ timestamp_le _null_ _null_ n 0));
DESCR("less-than-or-equal");

/* timestamp_ge("timestamp", "timestamp") => bool */ 
DATA(insert OID = 2056 ( timestamp_ge  PGNSP PGUID 12 f f t f i 2 16 f "1114 1114" _null_ _null_ _null_ timestamp_ge _null_ _null_ n 0));
DESCR("greater-than-or-equal");

/* timestamp_gt("timestamp", "timestamp") => bool */ 
DATA(insert OID = 2057 ( timestamp_gt  PGNSP PGUID 12 f f t f i 2 16 f "1114 1114" _null_ _null_ _null_ timestamp_gt _null_ _null_ n 0));
DESCR("greater-than");

/* age("timestamp", "timestamp") => "interval" */ 
DATA(insert OID = 2058 ( age  PGNSP PGUID 12 f f t f i 2 1186 f "1114 1114" _null_ _null_ _null_ timestamp_age _null_ _null_ n 0));
DESCR("date difference preserving months and years");

/* age("timestamp") => pg_catalog."interval" */ 
DATA(insert OID = 2059 ( age  PGNSP PGUID 14 f f t f s 1 1186 f "1114" _null_ _null_ _null_ "select pg_catalog.age(cast(current_date as timestamp without time zone), $1)" _null_ _null_ c 0));
DESCR("date difference from today preserving months and years");

/* timezone(text, "timestamp") => timestamptz */ 
DATA(insert OID = 2069 ( timezone  PGNSP PGUID 12 f f t f i 2 1184 f "25 1114" _null_ _null_ _null_ timestamp_zone _null_ _null_ n 0));
DESCR("adjust timestamp to new time zone");

/* timezone("interval", "timestamp") => timestamptz */ 
DATA(insert OID = 2070 ( timezone  PGNSP PGUID 12 f f t f i 2 1184 f "1186 1114" _null_ _null_ _null_ timestamp_izone _null_ _null_ n 0));
DESCR("adjust timestamp to new time zone");

/* date_pl_interval(date, "interval") => "timestamp" */ 
DATA(insert OID = 2071 ( date_pl_interval  PGNSP PGUID 12 f f t f i 2 1114 f "1082 1186" _null_ _null_ _null_ date_pl_interval _null_ _null_ n 0));
DESCR("add");

/* date_mi_interval(date, "interval") => "timestamp" */ 
DATA(insert OID = 2072 ( date_mi_interval  PGNSP PGUID 12 f f t f i 2 1114 f "1082 1186" _null_ _null_ _null_ date_mi_interval _null_ _null_ n 0));
DESCR("subtract");

/* "substring"(text, text) => text */ 
DATA(insert OID = 2073 ( substring  PGNSP PGUID 12 f f t f i 2 25 f "25 25" _null_ _null_ _null_ textregexsubstr _null_ _null_ n 0));
DESCR("extracts text matching regular expression");

/* "substring"(text, text, text) => pg_catalog.text */ 
DATA(insert OID = 2074 ( substring  PGNSP PGUID 14 f f t f i 3 25 f "25 25 25" _null_ _null_ _null_ "select pg_catalog.substring($1, pg_catalog.similar_escape($2, $3))" _null_ _null_ c 0));
DESCR("extracts text matching SQL99 regular expression");

/* "bit"(int8, int4) => "bit" */ 
DATA(insert OID = 2075 ( bit  PGNSP PGUID 12 f f t f i 2 1560 f "20 23" _null_ _null_ _null_ bitfromint8 _null_ _null_ n 0));
DESCR("int8 to bitstring");

/* int8("bit") => int8 */ 
DATA(insert OID = 2076 ( int8  PGNSP PGUID 12 f f t f i 1 20 f "1560" _null_ _null_ _null_ bittoint8 _null_ _null_ n 0));
DESCR("bitstring to int8");

/* current_setting(text) => text */ 
DATA(insert OID = 2077 ( current_setting  PGNSP PGUID 12 f f t f s 1 25 f "25" _null_ _null_ _null_ show_config_by_name _null_ _null_ n 0));
DESCR("SHOW X as a function");

/* set_config(text, text, bool) => text */ 
DATA(insert OID = 2078 ( set_config  PGNSP PGUID 12 f f f f v 3 25 f "25 25 16" _null_ _null_ _null_ set_config_by_name _null_ _null_ n 0));
DESCR("SET X as a function");

/* pg_show_all_settings() => SETOF record */ 
DATA(insert OID = 2084 ( pg_show_all_settings  PGNSP PGUID 12 f f t t s 0 2249 f "" _null_ _null_ _null_ show_all_settings _null_ _null_ n 0));
DESCR("SHOW ALL as a function");

/* pg_lock_status() => SETOF record */ 
DATA(insert OID = 1371 ( pg_lock_status  PGNSP PGUID 12 f f t t v 0 2249 f "" _null_ _null_ _null_ pg_lock_status _null_ _null_ r 0));
DESCR("view system lock information");

/* pg_prepared_xact() => SETOF record */ 
DATA(insert OID = 1065 ( pg_prepared_xact  PGNSP PGUID 12 f f t t v 0 2249 f "" _null_ _null_ _null_ pg_prepared_xact _null_ _null_ n 0));
DESCR("view two-phase transactions");

/* pg_table_is_visible(oid) => bool */ 
DATA(insert OID = 2079 ( pg_table_is_visible  PGNSP PGUID 12 f f t f s 1 16 f "26" _null_ _null_ _null_ pg_table_is_visible _null_ _null_ n 0));
DESCR("is table visible in search path?");

/* pg_type_is_visible(oid) => bool */ 
DATA(insert OID = 2080 ( pg_type_is_visible  PGNSP PGUID 12 f f t f s 1 16 f "26" _null_ _null_ _null_ pg_type_is_visible _null_ _null_ n 0));
DESCR("is type visible in search path?");

/* pg_function_is_visible(oid) => bool */ 
DATA(insert OID = 2081 ( pg_function_is_visible  PGNSP PGUID 12 f f t f s 1 16 f "26" _null_ _null_ _null_ pg_function_is_visible _null_ _null_ n 0));
DESCR("is function visible in search path?");

/* pg_operator_is_visible(oid) => bool */ 
DATA(insert OID = 2082 ( pg_operator_is_visible  PGNSP PGUID 12 f f t f s 1 16 f "26" _null_ _null_ _null_ pg_operator_is_visible _null_ _null_ n 0));
DESCR("is operator visible in search path?");

/* pg_opclass_is_visible(oid) => bool */ 
DATA(insert OID = 2083 ( pg_opclass_is_visible  PGNSP PGUID 12 f f t f s 1 16 f "26" _null_ _null_ _null_ pg_opclass_is_visible _null_ _null_ n 0));
DESCR("is opclass visible in search path?");

/* pg_conversion_is_visible(oid) => bool */ 
DATA(insert OID = 2093 ( pg_conversion_is_visible  PGNSP PGUID 12 f f t f s 1 16 f "26" _null_ _null_ _null_ pg_conversion_is_visible _null_ _null_ n 0));
DESCR("is conversion visible in search path?");

/* pg_my_temp_schema() => oid */ 
DATA(insert OID = 2854 ( pg_my_temp_schema  PGNSP PGUID 12 f f t f s 0 26 f "" _null_ _null_ _null_ pg_my_temp_schema _null_ _null_ n 0));
DESCR("get OID of current session's temp schema, if any");

/* pg_is_other_temp_schema(oid) => bool */ 
DATA(insert OID = 2855 ( pg_is_other_temp_schema  PGNSP PGUID 12 f f t f s 1 16 f "26" _null_ _null_ _null_ pg_is_other_temp_schema _null_ _null_ n 0));
DESCR("is schema another session's temp schema?");

/* pg_cancel_backend(int4) => bool */ 
DATA(insert OID = 2171 ( pg_cancel_backend  PGNSP PGUID 12 f f t f v 1 16 f "23" _null_ _null_ _null_ pg_cancel_backend _null_ _null_ n 0));
DESCR("Cancel a server process' current query");

/* pg_terminate_backend(int4) => bool */ 
DATA(insert OID = 2878 ( pg_terminate_backend  PGNSP PGUID 12 f f t f v 1 16 f "23" _null_ _null_ _null_ pg_terminate_backend _null_ _null_ n 0));
DESCR("terminate a server process");

/* pg_start_backup(text) => text */ 
DATA(insert OID = 2172 ( pg_start_backup  PGNSP PGUID 12 f f t f v 1 25 f "25" _null_ _null_ _null_ pg_start_backup _null_ _null_ n 0));
DESCR("Prepare for taking an online backup");

/* pg_stop_backup() => text */ 
DATA(insert OID = 2173 ( pg_stop_backup  PGNSP PGUID 12 f f t f v 0 25 f "" _null_ _null_ _null_ pg_stop_backup _null_ _null_ n 0));
DESCR("Finish taking an online backup");

/* pg_switch_xlog() => text */ 
DATA(insert OID = 2848 ( pg_switch_xlog  PGNSP PGUID 12 f f t f v 0 25 f "" _null_ _null_ _null_ pg_switch_xlog _null_ _null_ n 0));
DESCR("Switch to new xlog file");

/* pg_current_xlog_location() => text */ 
DATA(insert OID = 2849 ( pg_current_xlog_location  PGNSP PGUID 12 f f t f v 0 25 f "" _null_ _null_ _null_ pg_current_xlog_location _null_ _null_ n 0));
DESCR("current xlog write location");

/* pg_current_xlog_insert_location() => text */ 
DATA(insert OID = 2852 ( pg_current_xlog_insert_location  PGNSP PGUID 12 f f t f v 0 25 f "" _null_ _null_ _null_ pg_current_xlog_insert_location _null_ _null_ n 0));
DESCR("current xlog insert location");

/* pg_xlogfile_name_offset(IN wal_location text, OUT file_name text, OUT file_offset int4) => pg_catalog.record */ 
DATA(insert OID = 2850 ( pg_xlogfile_name_offset  PGNSP PGUID 12 f f t f i 1 2249 f "25" "{25,25,23}" "{i,o,o}" "{wal_location,file_name,file_offset}" pg_xlogfile_name_offset _null_ _null_ n 0));
DESCR("xlog filename and byte offset, given an xlog location");

/* pg_xlogfile_name(text) => text */ 
DATA(insert OID = 2851 ( pg_xlogfile_name  PGNSP PGUID 12 f f t f i 1 25 f "25" _null_ _null_ _null_ pg_xlogfile_name _null_ _null_ n 0));
DESCR("xlog filename, given an xlog location");

/* pg_reload_conf() => bool */ 
DATA(insert OID = 2621 ( pg_reload_conf  PGNSP PGUID 12 f f t f v 0 16 f "" _null_ _null_ _null_ pg_reload_conf _null_ _null_ n 0));
DESCR("Reload configuration files");

/* pg_rotate_logfile() => bool */ 
DATA(insert OID = 2622 ( pg_rotate_logfile  PGNSP PGUID 12 f f t f v 0 16 f "" _null_ _null_ _null_ pg_rotate_logfile _null_ _null_ n 0));
DESCR("Rotate log file");

/* pg_stat_file(IN filename text, OUT size int8, OUT access timestamptz, OUT modification timestamptz, OUT change timestamptz, OUT creation timestamptz, OUT isdir bool) => pg_catalog.record */ 
DATA(insert OID = 2623 ( pg_stat_file  PGNSP PGUID 12 f f t f v 1 2249 f "25" "{25,20,1184,1184,1184,1184,16}" "{i,o,o,o,o,o,o}" "{filename,size,access,modification,change,creation,isdir}" pg_stat_file _null_ _null_ n 0));
DESCR("Return file information");

/* pg_read_file(text, int8, int8) => text */ 
DATA(insert OID = 2624 ( pg_read_file  PGNSP PGUID 12 f f t f v 3 25 f "25 20 20" _null_ _null_ _null_ pg_read_file _null_ _null_ n 0));
DESCR("Read text from a file");

/* pg_ls_dir(text) => SETOF text */ 
DATA(insert OID = 2625 ( pg_ls_dir  PGNSP PGUID 12 f f t t v 1 25 f "25" _null_ _null_ _null_ pg_ls_dir _null_ _null_ n 0));
DESCR("List all files in a directory");

/* pg_sleep(float8) => void */ 
DATA(insert OID = 2626 ( pg_sleep  PGNSP PGUID 12 f f t f v 1 2278 f "701" _null_ _null_ _null_ pg_sleep _null_ _null_ n 0));
DESCR("Sleep for the specified time in seconds");

/* pg_resqueue_status() => SETOF record */ 
DATA(insert OID = 6030 ( pg_resqueue_status  PGNSP PGUID 12 f f t t v 0 2249 f "" _null_ _null_ _null_ pg_resqueue_status _null_ _null_ n 0));
DESCR("Return resource queue information");

/* pg_resqueue_status_kv() => SETOF record */ 
DATA(insert OID = 6069 ( pg_resqueue_status_kv  PGNSP PGUID 12 f f t t v 0 2249 f "" _null_ _null_ _null_ pg_resqueue_status_kv _null_ _null_ n 0));
DESCR("Return resource queue information");

/* pg_file_read(text, int8, int8) => text */ 
DATA(insert OID = 6045 ( pg_file_read  PGNSP PGUID 12 f f t f v 3 25 f "25 20 20" _null_ _null_ _null_ pg_read_file _null_ _null_ n 0));
DESCR("Read text from a file");

/* pg_logfile_rotate() => bool */ 
DATA(insert OID = 6046 ( pg_logfile_rotate  PGNSP PGUID 12 f f t f v 0 16 f "" _null_ _null_ _null_ pg_rotate_logfile _null_ _null_ n 0));
DESCR("Rotate log file");

/* pg_file_write(text, text, bool) => int8 */ 
DATA(insert OID = 6047 ( pg_file_write  PGNSP PGUID 12 f f t f v 3 20 f "25 25 16" _null_ _null_ _null_ pg_file_write _null_ _null_ n 0));
DESCR("Write text to a file");

/* pg_file_rename(text, text, text) => bool */ 
DATA(insert OID = 6048 ( pg_file_rename  PGNSP PGUID 12 f f f f v 3 16 f "25 25 25" _null_ _null_ _null_ pg_file_rename _null_ _null_ n 0));
DESCR("Rename a file");

/* pg_file_unlink(text) => bool */ 
DATA(insert OID = 6049 ( pg_file_unlink  PGNSP PGUID 12 f f t f v 1 16 f "25" _null_ _null_ _null_ pg_file_unlink _null_ _null_ n 0));
DESCR("Delete (unlink) a file");

/* pg_logdir_ls() => SETOF record */ 
DATA(insert OID = 6050 ( pg_logdir_ls  PGNSP PGUID 12 f f t t v 0 2249 f "" _null_ _null_ _null_ pg_logdir_ls _null_ _null_ n 0));
DESCR("ls the log dir");

/* pg_file_length(text) => int8 */ 
DATA(insert OID = 6051 ( pg_file_length  PGNSP PGUID 12 f f t f v 1 20 f "25" _null_ _null_ _null_ pg_file_length _null_ _null_ n 0));
DESCR("Get the length of a file (via stat)");

/* text(bool) => text */ 
DATA(insert OID = 2971 ( text  PGNSP PGUID 12 f f t f i 1 25 f "16" _null_ _null_ _null_ booltext _null_ _null_ n 0));
DESCR("convert boolean to text");


/* Aggregates (moved here from pg_aggregate for 7.3)  */
/* avg(int8) => "numeric" */ 
DATA(insert OID = 2100 ( avg  PGNSP PGUID 12 t f f f i 1 1700 f "20" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* avg(int4) => "numeric" */ 
DATA(insert OID = 2101 ( avg  PGNSP PGUID 12 t f f f i 1 1700 f "23" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* avg(int2) => "numeric" */ 
DATA(insert OID = 2102 ( avg  PGNSP PGUID 12 t f f f i 1 1700 f "21" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* avg("numeric") => "numeric" */ 
DATA(insert OID = 2103 ( avg  PGNSP PGUID 12 t f f f i 1 1700 f "1700" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* avg(float4) => float8 */ 
DATA(insert OID = 2104 ( avg  PGNSP PGUID 12 t f f f i 1 701 f "700" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* avg(float8) => float8 */ 
DATA(insert OID = 2105 ( avg  PGNSP PGUID 12 t f f f i 1 701 f "701" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* avg("interval") => "interval" */ 
DATA(insert OID = 2106 ( avg  PGNSP PGUID 12 t f f f i 1 1186 f "1186" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));


#define SUM_OID_MIN 2107
/* sum(int8) => "numeric" */ 
DATA(insert OID = 2107 ( sum  PGNSP PGUID 12 t f f f i 1 1700 f "20" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* sum(int4) => int8 */ 
DATA(insert OID = 2108 ( sum  PGNSP PGUID 12 t f f f i 1 20 f "23" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* sum(int2) => int8 */ 
DATA(insert OID = 2109 ( sum  PGNSP PGUID 12 t f f f i 1 20 f "21" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* sum(float4) => float4 */ 
DATA(insert OID = 2110 ( sum  PGNSP PGUID 12 t f f f i 1 700 f "700" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* sum(float8) => float8 */ 
DATA(insert OID = 2111 ( sum  PGNSP PGUID 12 t f f f i 1 701 f "701" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* sum(money) => money */ 
DATA(insert OID = 2112 ( sum  PGNSP PGUID 12 t f f f i 1 790 f "790" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* sum("interval") => "interval" */ 
DATA(insert OID = 2113 ( sum  PGNSP PGUID 12 t f f f i 1 1186 f "1186" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* sum("numeric") => "numeric" */ 
DATA(insert OID = 2114 ( sum  PGNSP PGUID 12 t f f f i 1 1700 f "1700" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));


#define SUM_OID_MAX 2114
/* max(int8) => int8 */ 
DATA(insert OID = 2115 ( max  PGNSP PGUID 12 t f f f i 1 20 f "20" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* max(int4) => int4 */ 
DATA(insert OID = 2116 ( max  PGNSP PGUID 12 t f f f i 1 23 f "23" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* max(int2) => int2 */ 
DATA(insert OID = 2117 ( max  PGNSP PGUID 12 t f f f i 1 21 f "21" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* max(oid) => oid */ 
DATA(insert OID = 2118 ( max  PGNSP PGUID 12 t f f f i 1 26 f "26" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* max(float4) => float4 */ 
DATA(insert OID = 2119 ( max  PGNSP PGUID 12 t f f f i 1 700 f "700" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* max(float8) => float8 */ 
DATA(insert OID = 2120 ( max  PGNSP PGUID 12 t f f f i 1 701 f "701" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* max(abstime) => abstime */ 
DATA(insert OID = 2121 ( max  PGNSP PGUID 12 t f f f i 1 702 f "702" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* max(date) => date */ 
DATA(insert OID = 2122 ( max  PGNSP PGUID 12 t f f f i 1 1082 f "1082" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* max("time") => "time" */ 
DATA(insert OID = 2123 ( max  PGNSP PGUID 12 t f f f i 1 1083 f "1083" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* max(timetz) => timetz */ 
DATA(insert OID = 2124 ( max  PGNSP PGUID 12 t f f f i 1 1266 f "1266" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* max(money) => money */ 
DATA(insert OID = 2125 ( max  PGNSP PGUID 12 t f f f i 1 790 f "790" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* max("timestamp") => "timestamp" */ 
DATA(insert OID = 2126 ( max  PGNSP PGUID 12 t f f f i 1 1114 f "1114" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* max(timestamptz) => timestamptz */ 
DATA(insert OID = 2127 ( max  PGNSP PGUID 12 t f f f i 1 1184 f "1184" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* max("interval") => "interval" */ 
DATA(insert OID = 2128 ( max  PGNSP PGUID 12 t f f f i 1 1186 f "1186" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* max(text) => text */ 
DATA(insert OID = 2129 ( max  PGNSP PGUID 12 t f f f i 1 25 f "25" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* max("numeric") => "numeric" */ 
DATA(insert OID = 2130 ( max  PGNSP PGUID 12 t f f f i 1 1700 f "1700" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* max(anyarray) => anyarray */ 
DATA(insert OID = 2050 ( max  PGNSP PGUID 12 t f f f i 1 2277 f "2277" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* max(bpchar) => bpchar */ 
DATA(insert OID = 2244 ( max  PGNSP PGUID 12 t f f f i 1 1042 f "1042" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* max(tid) => tid */ 
DATA(insert OID = 2797 ( max  PGNSP PGUID 12 t f f f i 1 27 f "27" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* max(gpxlogloc) => gpxlogloc */ 
DATA(insert OID = 3332 ( max  PGNSP PGUID 12 t f f f i 1 3310 f "3310" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* min(int8) => int8 */ 
DATA(insert OID = 2131 ( min  PGNSP PGUID 12 t f f f i 1 20 f "20" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* min(int4) => int4 */ 
DATA(insert OID = 2132 ( min  PGNSP PGUID 12 t f f f i 1 23 f "23" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* min(int2) => int2 */ 
DATA(insert OID = 2133 ( min  PGNSP PGUID 12 t f f f i 1 21 f "21" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* min(oid) => oid */ 
DATA(insert OID = 2134 ( min  PGNSP PGUID 12 t f f f i 1 26 f "26" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* min(float4) => float4 */ 
DATA(insert OID = 2135 ( min  PGNSP PGUID 12 t f f f i 1 700 f "700" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* min(float8) => float8 */ 
DATA(insert OID = 2136 ( min  PGNSP PGUID 12 t f f f i 1 701 f "701" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* min(abstime) => abstime */ 
DATA(insert OID = 2137 ( min  PGNSP PGUID 12 t f f f i 1 702 f "702" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* min(date) => date */ 
DATA(insert OID = 2138 ( min  PGNSP PGUID 12 t f f f i 1 1082 f "1082" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* min("time") => "time" */ 
DATA(insert OID = 2139 ( min  PGNSP PGUID 12 t f f f i 1 1083 f "1083" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* min(timetz) => timetz */ 
DATA(insert OID = 2140 ( min  PGNSP PGUID 12 t f f f i 1 1266 f "1266" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* min(money) => money */ 
DATA(insert OID = 2141 ( min  PGNSP PGUID 12 t f f f i 1 790 f "790" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* min("timestamp") => "timestamp" */ 
DATA(insert OID = 2142 ( min  PGNSP PGUID 12 t f f f i 1 1114 f "1114" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* min(timestamptz) => timestamptz */ 
DATA(insert OID = 2143 ( min  PGNSP PGUID 12 t f f f i 1 1184 f "1184" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* min("interval") => "interval" */ 
DATA(insert OID = 2144 ( min  PGNSP PGUID 12 t f f f i 1 1186 f "1186" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* min(text) => text */ 
DATA(insert OID = 2145 ( min  PGNSP PGUID 12 t f f f i 1 25 f "25" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* min("numeric") => "numeric" */ 
DATA(insert OID = 2146 ( min  PGNSP PGUID 12 t f f f i 1 1700 f "1700" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* min(anyarray) => anyarray */ 
DATA(insert OID = 2051 ( min  PGNSP PGUID 12 t f f f i 1 2277 f "2277" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* min(bpchar) => bpchar */ 
DATA(insert OID = 2245 ( min  PGNSP PGUID 12 t f f f i 1 1042 f "1042" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* min(tid) => tid */ 
DATA(insert OID = 2798 ( min  PGNSP PGUID 12 t f f f i 1 27 f "27" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* min(gpxlogloc) => gpxlogloc */ 
DATA(insert OID = 3333 ( min  PGNSP PGUID 12 t f f f i 1 3310 f "3310" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));


/* count has two forms: count(any) and count(*)  */
/* count("any") => int8 */ 
DATA(insert OID = 2147 ( count  PGNSP PGUID 12 t f f f i 1 20 f "2276" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));


#define COUNT_ANY_OID 2147
/* count() => int8 */ 
DATA(insert OID = 2803 ( count  PGNSP PGUID 12 t f f f i 0 20 f "" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));


#define COUNT_STAR_OID  2803
/* var_pop(int8) => "numeric" */ 
DATA(insert OID = 2718 ( var_pop  PGNSP PGUID 12 t f f f i 1 1700 f "20" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* var_pop(int4) => "numeric" */ 
DATA(insert OID = 2719 ( var_pop  PGNSP PGUID 12 t f f f i 1 1700 f "23" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* var_pop(int2) => "numeric" */ 
DATA(insert OID = 2720 ( var_pop  PGNSP PGUID 12 t f f f i 1 1700 f "21" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* var_pop(float4) => float8 */ 
DATA(insert OID = 2721 ( var_pop  PGNSP PGUID 12 t f f f i 1 701 f "700" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* var_pop(float8) => float8 */ 
DATA(insert OID = 2722 ( var_pop  PGNSP PGUID 12 t f f f i 1 701 f "701" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* var_pop("numeric") => "numeric" */ 
DATA(insert OID = 2723 ( var_pop  PGNSP PGUID 12 t f f f i 1 1700 f "1700" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* var_samp(int8) => "numeric" */ 
DATA(insert OID = 2641 ( var_samp  PGNSP PGUID 12 t f f f i 1 1700 f "20" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* var_samp(int4) => "numeric" */ 
DATA(insert OID = 2642 ( var_samp  PGNSP PGUID 12 t f f f i 1 1700 f "23" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* var_samp(int2) => "numeric" */ 
DATA(insert OID = 2643 ( var_samp  PGNSP PGUID 12 t f f f i 1 1700 f "21" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* var_samp(float4) => float8 */ 
DATA(insert OID = 2644 ( var_samp  PGNSP PGUID 12 t f f f i 1 701 f "700" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* var_samp(float8) => float8 */ 
DATA(insert OID = 2645 ( var_samp  PGNSP PGUID 12 t f f f i 1 701 f "701" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* var_samp("numeric") => "numeric" */ 
DATA(insert OID = 2646 ( var_samp  PGNSP PGUID 12 t f f f i 1 1700 f "1700" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* variance(int8) => "numeric" */ 
DATA(insert OID = 2148 ( variance  PGNSP PGUID 12 t f f f i 1 1700 f "20" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* variance(int4) => "numeric" */ 
DATA(insert OID = 2149 ( variance  PGNSP PGUID 12 t f f f i 1 1700 f "23" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* variance(int2) => "numeric" */ 
DATA(insert OID = 2150 ( variance  PGNSP PGUID 12 t f f f i 1 1700 f "21" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* variance(float4) => float8 */ 
DATA(insert OID = 2151 ( variance  PGNSP PGUID 12 t f f f i 1 701 f "700" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* variance(float8) => float8 */ 
DATA(insert OID = 2152 ( variance  PGNSP PGUID 12 t f f f i 1 701 f "701" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* variance("numeric") => "numeric" */ 
DATA(insert OID = 2153 ( variance  PGNSP PGUID 12 t f f f i 1 1700 f "1700" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* stddev_pop(int8) => "numeric" */ 
DATA(insert OID = 2724 ( stddev_pop  PGNSP PGUID 12 t f f f i 1 1700 f "20" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* stddev_pop(int4) => "numeric" */ 
DATA(insert OID = 2725 ( stddev_pop  PGNSP PGUID 12 t f f f i 1 1700 f "23" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* stddev_pop(int2) => "numeric" */ 
DATA(insert OID = 2726 ( stddev_pop  PGNSP PGUID 12 t f f f i 1 1700 f "21" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* stddev_pop(float4) => float8 */ 
DATA(insert OID = 2727 ( stddev_pop  PGNSP PGUID 12 t f f f i 1 701 f "700" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* stddev_pop(float8) => float8 */ 
DATA(insert OID = 2728 ( stddev_pop  PGNSP PGUID 12 t f f f i 1 701 f "701" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* stddev_pop("numeric") => "numeric" */ 
DATA(insert OID = 2729 ( stddev_pop  PGNSP PGUID 12 t f f f i 1 1700 f "1700" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* stddev_samp(int8) => "numeric" */ 
DATA(insert OID = 2712 ( stddev_samp  PGNSP PGUID 12 t f f f i 1 1700 f "20" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* stddev_samp(int4) => "numeric" */ 
DATA(insert OID = 2713 ( stddev_samp  PGNSP PGUID 12 t f f f i 1 1700 f "23" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* stddev_samp(int2) => "numeric" */ 
DATA(insert OID = 2714 ( stddev_samp  PGNSP PGUID 12 t f f f i 1 1700 f "21" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* stddev_samp(float4) => float8 */ 
DATA(insert OID = 2715 ( stddev_samp  PGNSP PGUID 12 t f f f i 1 701 f "700" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* stddev_samp(float8) => float8 */ 
DATA(insert OID = 2716 ( stddev_samp  PGNSP PGUID 12 t f f f i 1 701 f "701" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* stddev_samp("numeric") => "numeric" */ 
DATA(insert OID = 2717 ( stddev_samp  PGNSP PGUID 12 t f f f i 1 1700 f "1700" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* stddev(int8) => "numeric" */ 
DATA(insert OID = 2154 ( stddev  PGNSP PGUID 12 t f f f i 1 1700 f "20" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* stddev(int4) => "numeric" */ 
DATA(insert OID = 2155 ( stddev  PGNSP PGUID 12 t f f f i 1 1700 f "23" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* stddev(int2) => "numeric" */ 
DATA(insert OID = 2156 ( stddev  PGNSP PGUID 12 t f f f i 1 1700 f "21" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* stddev(float4) => float8 */ 
DATA(insert OID = 2157 ( stddev  PGNSP PGUID 12 t f f f i 1 701 f "700" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* stddev(float8) => float8 */ 
DATA(insert OID = 2158 ( stddev  PGNSP PGUID 12 t f f f i 1 701 f "701" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* stddev("numeric") => "numeric" */ 
DATA(insert OID = 2159 ( stddev  PGNSP PGUID 12 t f f f i 1 1700 f "1700" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));


/* MPP Aggregate -- array_sum -- special for prospective customer.  */
/* array_sum(_int4) => _int4 */ 
DATA(insert OID = 6013 ( array_sum  PGNSP PGUID 12 t f f f i 1 1007 f "1007" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* regr_count(float8, float8) => int8 */ 
DATA(insert OID = 2818 ( regr_count  PGNSP PGUID 12 t f f f i 2 20 f "701 701" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* regr_sxx(float8, float8) => float8 */ 
DATA(insert OID = 2819 ( regr_sxx  PGNSP PGUID 12 t f f f i 2 701 f "701 701" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* regr_syy(float8, float8) => float8 */ 
DATA(insert OID = 2820 ( regr_syy  PGNSP PGUID 12 t f f f i 2 701 f "701 701" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* regr_sxy(float8, float8) => float8 */ 
DATA(insert OID = 2821 ( regr_sxy  PGNSP PGUID 12 t f f f i 2 701 f "701 701" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* regr_avgx(float8, float8) => float8 */ 
DATA(insert OID = 2822 ( regr_avgx  PGNSP PGUID 12 t f f f i 2 701 f "701 701" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* regr_avgy(float8, float8) => float8 */ 
DATA(insert OID = 2823 ( regr_avgy  PGNSP PGUID 12 t f f f i 2 701 f "701 701" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* regr_r2(float8, float8) => float8 */ 
DATA(insert OID = 2824 ( regr_r2  PGNSP PGUID 12 t f f f i 2 701 f "701 701" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* regr_slope(float8, float8) => float8 */ 
DATA(insert OID = 2825 ( regr_slope  PGNSP PGUID 12 t f f f i 2 701 f "701 701" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* regr_intercept(float8, float8) => float8 */ 
DATA(insert OID = 2826 ( regr_intercept  PGNSP PGUID 12 t f f f i 2 701 f "701 701" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* covar_pop(float8, float8) => float8 */ 
DATA(insert OID = 2827 ( covar_pop  PGNSP PGUID 12 t f f f i 2 701 f "701 701" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* covar_samp(float8, float8) => float8 */ 
DATA(insert OID = 2828 ( covar_samp  PGNSP PGUID 12 t f f f i 2 701 f "701 701" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* corr(float8, float8) => float8 */ 
DATA(insert OID = 2829 ( corr  PGNSP PGUID 12 t f f f i 2 701 f "701 701" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* text_pattern_lt(text, text) => bool */ 
DATA(insert OID = 2160 ( text_pattern_lt  PGNSP PGUID 12 f f t f i 2 16 f "25 25" _null_ _null_ _null_ text_pattern_lt _null_ _null_ n 0));

/* text_pattern_le(text, text) => bool */ 
DATA(insert OID = 2161 ( text_pattern_le  PGNSP PGUID 12 f f t f i 2 16 f "25 25" _null_ _null_ _null_ text_pattern_le _null_ _null_ n 0));

/* text_pattern_eq(text, text) => bool */ 
DATA(insert OID = 2162 ( text_pattern_eq  PGNSP PGUID 12 f f t f i 2 16 f "25 25" _null_ _null_ _null_ texteq _null_ _null_ n 0));

/* text_pattern_ge(text, text) => bool */ 
DATA(insert OID = 2163 ( text_pattern_ge  PGNSP PGUID 12 f f t f i 2 16 f "25 25" _null_ _null_ _null_ text_pattern_ge _null_ _null_ n 0));

/* text_pattern_gt(text, text) => bool */ 
DATA(insert OID = 2164 ( text_pattern_gt  PGNSP PGUID 12 f f t f i 2 16 f "25 25" _null_ _null_ _null_ text_pattern_gt _null_ _null_ n 0));

/* text_pattern_ne(text, text) => bool */ 
DATA(insert OID = 2165 ( text_pattern_ne  PGNSP PGUID 12 f f t f i 2 16 f "25 25" _null_ _null_ _null_ textne _null_ _null_ n 0));

/* bttext_pattern_cmp(text, text) => int4 */ 
DATA(insert OID = 2166 ( bttext_pattern_cmp  PGNSP PGUID 12 f f t f i 2 23 f "25 25" _null_ _null_ _null_ bttext_pattern_cmp _null_ _null_ n 0));


/* Window functions (similar to aggregates)  */
/* row_number() => int8 */ 
DATA(insert OID = 7000 ( row_number  PGNSP PGUID 12 f f f f i 0 20 t "" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));


#define ROW_NUMBER_OID 7000
#define ROW_NUMBER_TYPE 20
/* rank() => int8 */ 
DATA(insert OID = 7001 ( rank  PGNSP PGUID 12 f f f f i 0 20 t "" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* dense_rank() => int8 */ 
DATA(insert OID = 7002 ( dense_rank  PGNSP PGUID 12 f f f f i 0 20 t "" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* percent_rank() => float8 */ 
DATA(insert OID = 7003 ( percent_rank  PGNSP PGUID 12 f f f f i 0 701 t "" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* cume_dist() => float8 */ 
DATA(insert OID = 7004 ( cume_dist  PGNSP PGUID 12 f f f f i 0 701 t "" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));


#define CUME_DIST_OID 7004
/* ntile(int4) => int8 */ 
DATA(insert OID = 7005 ( ntile  PGNSP PGUID 12 f f f f i 1 20 t "23" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* ntile(int8) => int8 */ 
DATA(insert OID = 7006 ( ntile  PGNSP PGUID 12 f f f f i 1 20 t "20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* ntile("numeric") => int8 */ 
DATA(insert OID = 7007 ( ntile  PGNSP PGUID 12 f f f f i 1 20 t "1700" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));


/*  XXX for now, window functions with arguments, just cite numeric = 1700, need to overload.  */
/* first_value(bool) => bool */ 
DATA(insert OID = 7017 ( first_value  PGNSP PGUID 12 f f f f i 1 16 t "16" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* first_value("char") => "char" */ 
DATA(insert OID = 7018 ( first_value  PGNSP PGUID 12 f f f f i 1 18 t "18" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* first_value(cidr) => cidr */ 
DATA(insert OID = 7019 ( first_value  PGNSP PGUID 12 f f f f i 1 650 t "650" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* first_value(circle) => circle */ 
DATA(insert OID = 7020 ( first_value  PGNSP PGUID 12 f f f f i 1 718 t "718" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* first_value(float4) => float4 */ 
DATA(insert OID = 7021 ( first_value  PGNSP PGUID 12 f f f f i 1 700 t "700" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* first_value(float8) => float8 */ 
DATA(insert OID = 7022 ( first_value  PGNSP PGUID 12 f f f f i 1 701 t "701" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* first_value(inet) => inet */ 
DATA(insert OID = 7023 ( first_value  PGNSP PGUID 12 f f f f i 1 869 t "869" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* first_value("interval") => "interval" */ 
DATA(insert OID = 7024 ( first_value  PGNSP PGUID 12 f f f f i 1 1186 t "1186" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* first_value(line) => line */ 
DATA(insert OID = 7025 ( first_value  PGNSP PGUID 12 f f f f i 1 628 t "628" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* first_value(lseg) => lseg */ 
DATA(insert OID = 7026 ( first_value  PGNSP PGUID 12 f f f f i 1 601 t "601" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* first_value(macaddr) => macaddr */ 
DATA(insert OID = 7027 ( first_value  PGNSP PGUID 12 f f f f i 1 829 t "829" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* first_value(int2) => int2 */ 
DATA(insert OID = 7028 ( first_value  PGNSP PGUID 12 f f f f i 1 21 t "21" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* first_value(int4) => int4 */ 
DATA(insert OID = 7029 ( first_value  PGNSP PGUID 12 f f f f i 1 23 t "23" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* first_value(int8) => int8 */ 
DATA(insert OID = 7030 ( first_value  PGNSP PGUID 12 f f f f i 1 20 t "20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* first_value(money) => money */ 
DATA(insert OID = 7031 ( first_value  PGNSP PGUID 12 f f f f i 1 790 t "790" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* first_value(name) => name */ 
DATA(insert OID = 7032 ( first_value  PGNSP PGUID 12 f f f f i 1 19 t "19" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* first_value("numeric") => "numeric" */ 
DATA(insert OID = 7033 ( first_value  PGNSP PGUID 12 f f f f i 1 1700 t "1700" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* first_value(oid) => oid */ 
DATA(insert OID = 7034 ( first_value  PGNSP PGUID 12 f f f f i 1 26 t "26" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* first_value(path) => path */ 
DATA(insert OID = 7035 ( first_value  PGNSP PGUID 12 f f f f i 1 602 t "602" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* first_value(point) => point */ 
DATA(insert OID = 7036 ( first_value  PGNSP PGUID 12 f f f f i 1 600 t "600" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* first_value(polygon) => polygon */ 
DATA(insert OID = 7037 ( first_value  PGNSP PGUID 12 f f f f i 1 604 t "604" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* first_value(reltime) => reltime */ 
DATA(insert OID = 7038 ( first_value  PGNSP PGUID 12 f f f f i 1 703 t "703" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* first_value(text) => text */ 
DATA(insert OID = 7039 ( first_value  PGNSP PGUID 12 f f f f i 1 25 t "25" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* first_value(tid) => tid */ 
DATA(insert OID = 7040 ( first_value  PGNSP PGUID 12 f f f f i 1 27 t "27" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* first_value("time") => "time" */ 
DATA(insert OID = 7041 ( first_value  PGNSP PGUID 12 f f f f i 1 1083 t "1083" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* first_value("timestamp") => "timestamp" */ 
DATA(insert OID = 7042 ( first_value  PGNSP PGUID 12 f f f f i 1 1114 t "1114" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* first_value(timestamptz) => timestamptz */ 
DATA(insert OID = 7043 ( first_value  PGNSP PGUID 12 f f f f i 1 1184 t "1184" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* first_value(timetz) => timetz */ 
DATA(insert OID = 7044 ( first_value  PGNSP PGUID 12 f f f f i 1 1266 t "1266" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* first_value(varbit) => varbit */ 
DATA(insert OID = 7045 ( first_value  PGNSP PGUID 12 f f f f i 1 1562 t "1562" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* first_value("varchar") => "varchar" */ 
DATA(insert OID = 7046 ( first_value  PGNSP PGUID 12 f f f f i 1 1043 t "1043" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* first_value(xid) => xid */ 
DATA(insert OID = 7047 ( first_value  PGNSP PGUID 12 f f f f i 1 28 t "28" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* first_value(bytea) => bytea */ 
DATA(insert OID = 7232 ( first_value  PGNSP PGUID 12 f f f f i 1 17 t "17" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* first_value("bit") => "bit" */ 
DATA(insert OID = 7256 ( first_value  PGNSP PGUID 12 f f f f i 1 1560 t "1560" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* first_value(box) => box */ 
DATA(insert OID = 7272 ( first_value  PGNSP PGUID 12 f f f f i 1 603 t "603" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* first_value(anyarray) => anyarray */ 
DATA(insert OID = 7288 ( first_value  PGNSP PGUID 12 f f f f i 1 2277 t "2277" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* last_value(int4) => int4 */ 
DATA(insert OID = 7012 ( last_value  PGNSP PGUID 12 f f f f i 1 23 t "23" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* last_value(int2) => int2 */ 
DATA(insert OID = 7013 ( last_value  PGNSP PGUID 12 f f f f i 1 21 t "21" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* last_value(int8) => int8 */ 
DATA(insert OID = 7014 ( last_value  PGNSP PGUID 12 f f f f i 1 20 t "20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* last_value("numeric") => "numeric" */ 
DATA(insert OID = 7015 ( last_value  PGNSP PGUID 12 f f f f i 1 1700 t "1700" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* last_value(text) => text */ 
DATA(insert OID = 7016 ( last_value  PGNSP PGUID 12 f f f f i 1 25 t "25" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* last_value(bool) => bool */ 
DATA(insert OID = 7063 ( last_value  PGNSP PGUID 12 f f f f i 1 16 t "16" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* last_value("char") => "char" */ 
DATA(insert OID = 7072 ( last_value  PGNSP PGUID 12 f f f f i 1 18 t "18" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* last_value(cidr) => cidr */ 
DATA(insert OID = 7073 ( last_value  PGNSP PGUID 12 f f f f i 1 650 t "650" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* last_value(circle) => circle */ 
DATA(insert OID = 7048 ( last_value  PGNSP PGUID 12 f f f f i 1 718 t "718" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* last_value(float4) => float4 */ 
DATA(insert OID = 7049 ( last_value  PGNSP PGUID 12 f f f f i 1 700 t "700" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* last_value(float8) => float8 */ 
DATA(insert OID = 7050 ( last_value  PGNSP PGUID 12 f f f f i 1 701 t "701" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* last_value(inet) => inet */ 
DATA(insert OID = 7051 ( last_value  PGNSP PGUID 12 f f f f i 1 869 t "869" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* last_value("interval") => "interval" */ 
DATA(insert OID = 7052 ( last_value  PGNSP PGUID 12 f f f f i 1 1186 t "1186" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* last_value(line) => line */ 
DATA(insert OID = 7053 ( last_value  PGNSP PGUID 12 f f f f i 1 628 t "628" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* last_value(lseg) => lseg */ 
DATA(insert OID = 7054 ( last_value  PGNSP PGUID 12 f f f f i 1 601 t "601" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* last_value(macaddr) => macaddr */ 
DATA(insert OID = 7055 ( last_value  PGNSP PGUID 12 f f f f i 1 829 t "829" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* last_value(money) => money */ 
DATA(insert OID = 7056 ( last_value  PGNSP PGUID 12 f f f f i 1 790 t "790" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* last_value(name) => name */ 
DATA(insert OID = 7057 ( last_value  PGNSP PGUID 12 f f f f i 1 19 t "19" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* last_value(oid) => oid */ 
DATA(insert OID = 7058 ( last_value  PGNSP PGUID 12 f f f f i 1 26 t "26" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* last_value(path) => path */ 
DATA(insert OID = 7059 ( last_value  PGNSP PGUID 12 f f f f i 1 602 t "602" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* last_value(point) => point */ 
DATA(insert OID = 7060 ( last_value  PGNSP PGUID 12 f f f f i 1 600 t "600" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* last_value(polygon) => polygon */ 
DATA(insert OID = 7061 ( last_value  PGNSP PGUID 12 f f f f i 1 604 t "604" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* last_value(reltime) => reltime */ 
DATA(insert OID = 7062 ( last_value  PGNSP PGUID 12 f f f f i 1 703 t "703" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* last_value(tid) => tid */ 
DATA(insert OID = 7064 ( last_value  PGNSP PGUID 12 f f f f i 1 27 t "27" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* last_value("time") => "time" */ 
DATA(insert OID = 7065 ( last_value  PGNSP PGUID 12 f f f f i 1 1083 t "1083" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* last_value("timestamp") => "timestamp" */ 
DATA(insert OID = 7066 ( last_value  PGNSP PGUID 12 f f f f i 1 1114 t "1114" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* last_value(timestamptz) => timestamptz */ 
DATA(insert OID = 7067 ( last_value  PGNSP PGUID 12 f f f f i 1 1184 t "1184" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* last_value(timetz) => timetz */ 
DATA(insert OID = 7068 ( last_value  PGNSP PGUID 12 f f f f i 1 1266 t "1266" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* last_value(varbit) => varbit */ 
DATA(insert OID = 7069 ( last_value  PGNSP PGUID 12 f f f f i 1 1562 t "1562" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* last_value("varchar") => "varchar" */ 
DATA(insert OID = 7070 ( last_value  PGNSP PGUID 12 f f f f i 1 1043 t "1043" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* last_value(xid) => xid */ 
DATA(insert OID = 7071 ( last_value  PGNSP PGUID 12 f f f f i 1 28 t "28" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* last_value(bytea) => bytea */ 
DATA(insert OID = 7238 ( last_value  PGNSP PGUID 12 f f f f i 1 17 t "17" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* last_value("bit") => "bit" */ 
DATA(insert OID = 7258 ( last_value  PGNSP PGUID 12 f f f f i 1 1560 t "1560" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* last_value(box) => box */ 
DATA(insert OID = 7274 ( last_value  PGNSP PGUID 12 f f f f i 1 603 t "603" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* last_value(anyarray) => anyarray */ 
DATA(insert OID = 7290 ( last_value  PGNSP PGUID 12 f f f f i 1 2277 t "2277" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(bool, int8, bool) => bool */ 
DATA(insert OID = 7675 ( lag  PGNSP PGUID 12 f f f f i 3 16 t "16 20 16" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(bool, int8) => bool */ 
DATA(insert OID = 7491 ( lag  PGNSP PGUID 12 f f f f i 2 16 t "16 20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(bool) => bool */ 
DATA(insert OID = 7493 ( lag  PGNSP PGUID 12 f f f f i 1 16 t "16" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag("char", int8, "char") => "char" */ 
DATA(insert OID = 7495 ( lag  PGNSP PGUID 12 f f f f i 3 18 t "18 20 18" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag("char", int8) => "char" */ 
DATA(insert OID = 7497 ( lag  PGNSP PGUID 12 f f f f i 2 18 t "18 20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag("char") => "char" */ 
DATA(insert OID = 7499 ( lag  PGNSP PGUID 12 f f f f i 1 18 t "18" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(cidr, int8, cidr) => cidr */ 
DATA(insert OID = 7501 ( lag  PGNSP PGUID 12 f f f f i 3 650 t "650 20 650" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(cidr, int8) => cidr */ 
DATA(insert OID = 7503 ( lag  PGNSP PGUID 12 f f f f i 2 650 t "650 20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(cidr) => cidr */ 
DATA(insert OID = 7505 ( lag  PGNSP PGUID 12 f f f f i 1 650 t "650" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(circle, int8, circle) => circle */ 
DATA(insert OID = 7507 ( lag  PGNSP PGUID 12 f f f f i 3 718 t "718 20 718" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(circle, int8) => circle */ 
DATA(insert OID = 7509 ( lag  PGNSP PGUID 12 f f f f i 2 718 t "718 20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(circle) => circle */ 
DATA(insert OID = 7511 ( lag  PGNSP PGUID 12 f f f f i 1 718 t "718" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(float4, int8, float4) => float4 */ 
DATA(insert OID = 7513 ( lag  PGNSP PGUID 12 f f f f i 3 700 t "700 20 700" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(float4, int8) => float4 */ 
DATA(insert OID = 7515 ( lag  PGNSP PGUID 12 f f f f i 2 700 t "700 20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(float4) => float4 */ 
DATA(insert OID = 7517 ( lag  PGNSP PGUID 12 f f f f i 1 700 t "700" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(float8, int8, float8) => float8 */ 
DATA(insert OID = 7519 ( lag  PGNSP PGUID 12 f f f f i 3 701 t "701 20 701" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(float8, int8) => float8 */ 
DATA(insert OID = 7521 ( lag  PGNSP PGUID 12 f f f f i 2 701 t "701 20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(float8) => float8 */ 
DATA(insert OID = 7523 ( lag  PGNSP PGUID 12 f f f f i 1 701 t "701" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(inet, int8, inet) => inet */ 
DATA(insert OID = 7525 ( lag  PGNSP PGUID 12 f f f f i 3 869 t "869 20 869" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(inet, int8) => inet */ 
DATA(insert OID = 7527 ( lag  PGNSP PGUID 12 f f f f i 2 869 t "869 20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(inet) => inet */ 
DATA(insert OID = 7529 ( lag  PGNSP PGUID 12 f f f f i 1 869 t "869" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag("interval", int8, "interval") => "interval" */ 
DATA(insert OID = 7531 ( lag  PGNSP PGUID 12 f f f f i 3 1186 t "1186 20 1186" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag("interval", int8) => "interval" */ 
DATA(insert OID = 7533 ( lag  PGNSP PGUID 12 f f f f i 2 1186 t "1186 20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag("interval") => "interval" */ 
DATA(insert OID = 7535 ( lag  PGNSP PGUID 12 f f f f i 1 1186 t "1186" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(line, int8, line) => line */ 
DATA(insert OID = 7537 ( lag  PGNSP PGUID 12 f f f f i 3 628 t "628 20 628" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(line, int8) => line */ 
DATA(insert OID = 7539 ( lag  PGNSP PGUID 12 f f f f i 2 628 t "628 20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(line) => line */ 
DATA(insert OID = 7541 ( lag  PGNSP PGUID 12 f f f f i 1 628 t "628" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(lseg, int8, lseg) => lseg */ 
DATA(insert OID = 7543 ( lag  PGNSP PGUID 12 f f f f i 3 601 t "601 20 601" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(lseg, int8) => lseg */ 
DATA(insert OID = 7545 ( lag  PGNSP PGUID 12 f f f f i 2 601 t "601 20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(lseg) => lseg */ 
DATA(insert OID = 7547 ( lag  PGNSP PGUID 12 f f f f i 1 601 t "601" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(macaddr, int8, macaddr) => macaddr */ 
DATA(insert OID = 7549 ( lag  PGNSP PGUID 12 f f f f i 3 829 t "829 20 829" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(macaddr, int8) => macaddr */ 
DATA(insert OID = 7551 ( lag  PGNSP PGUID 12 f f f f i 2 829 t "829 20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(macaddr) => macaddr */ 
DATA(insert OID = 7553 ( lag  PGNSP PGUID 12 f f f f i 1 829 t "829" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(int2, int8, int2) => int2 */ 
DATA(insert OID = 7555 ( lag  PGNSP PGUID 12 f f f f i 3 21 t "21 20 21" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(int2, int8) => int2 */ 
DATA(insert OID = 7557 ( lag  PGNSP PGUID 12 f f f f i 2 21 t "21 20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(int2) => int2 */ 
DATA(insert OID = 7559 ( lag  PGNSP PGUID 12 f f f f i 1 21 t "21" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(int4, int8, int4) => int4 */ 
DATA(insert OID = 7561 ( lag  PGNSP PGUID 12 f f f f i 3 23 t "23 20 23" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(int4, int8) => int4 */ 
DATA(insert OID = 7563 ( lag  PGNSP PGUID 12 f f f f i 2 23 t "23 20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(int4) => int4 */ 
DATA(insert OID = 7565 ( lag  PGNSP PGUID 12 f f f f i 1 23 t "23" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(int8, int8, int8) => int8 */ 
DATA(insert OID = 7567 ( lag  PGNSP PGUID 12 f f f f i 3 20 t "20 20 20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(int8, int8) => int8 */ 
DATA(insert OID = 7569 ( lag  PGNSP PGUID 12 f f f f i 2 20 t "20 20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(int8) => int8 */ 
DATA(insert OID = 7571 ( lag  PGNSP PGUID 12 f f f f i 1 20 t "20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(money, int8, money) => money */ 
DATA(insert OID = 7573 ( lag  PGNSP PGUID 12 f f f f i 3 790 t "790 20 790" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(money, int8) => money */ 
DATA(insert OID = 7575 ( lag  PGNSP PGUID 12 f f f f i 2 790 t "790 20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(money) => money */ 
DATA(insert OID = 7577 ( lag  PGNSP PGUID 12 f f f f i 1 790 t "790" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(name, int8, name) => name */ 
DATA(insert OID = 7579 ( lag  PGNSP PGUID 12 f f f f i 3 19 t "19 20 19" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(name, int8) => name */ 
DATA(insert OID = 7581 ( lag  PGNSP PGUID 12 f f f f i 2 19 t "19 20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(name) => name */ 
DATA(insert OID = 7583 ( lag  PGNSP PGUID 12 f f f f i 1 19 t "19" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag("numeric", int8, "numeric") => "numeric" */ 
DATA(insert OID = 7585 ( lag  PGNSP PGUID 12 f f f f i 3 1700 t "1700 20 1700" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag("numeric", int8) => "numeric" */ 
DATA(insert OID = 7587 ( lag  PGNSP PGUID 12 f f f f i 2 1700 t "1700 20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag("numeric") => "numeric" */ 
DATA(insert OID = 7589 ( lag  PGNSP PGUID 12 f f f f i 1 1700 t "1700" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(oid, int8, oid) => oid */ 
DATA(insert OID = 7591 ( lag  PGNSP PGUID 12 f f f f i 3 26 t "26 20 26" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(oid, int8) => oid */ 
DATA(insert OID = 7593 ( lag  PGNSP PGUID 12 f f f f i 2 26 t "26 20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(oid) => oid */ 
DATA(insert OID = 7595 ( lag  PGNSP PGUID 12 f f f f i 1 26 t "26" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(path, int8, path) => path */ 
DATA(insert OID = 7597 ( lag  PGNSP PGUID 12 f f f f i 3 602 t "602 20 602" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(path, int8) => path */ 
DATA(insert OID = 7599 ( lag  PGNSP PGUID 12 f f f f i 2 602 t "602 20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(path) => path */ 
DATA(insert OID = 7601 ( lag  PGNSP PGUID 12 f f f f i 1 602 t "602" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(point, int8, point) => point */ 
DATA(insert OID = 7603 ( lag  PGNSP PGUID 12 f f f f i 3 600 t "600 20 600" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(point, int8) => point */ 
DATA(insert OID = 7605 ( lag  PGNSP PGUID 12 f f f f i 2 600 t "600 20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(point) => point */ 
DATA(insert OID = 7607 ( lag  PGNSP PGUID 12 f f f f i 1 600 t "600" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(polygon, int8, polygon) => polygon */ 
DATA(insert OID = 7609 ( lag  PGNSP PGUID 12 f f f f i 3 604 t "604 20 604" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(polygon, int8) => polygon */ 
DATA(insert OID = 7611 ( lag  PGNSP PGUID 12 f f f f i 2 604 t "604 20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(polygon) => polygon */ 
DATA(insert OID = 7613 ( lag  PGNSP PGUID 12 f f f f i 1 604 t "604" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(reltime, int8, reltime) => reltime */ 
DATA(insert OID = 7615 ( lag  PGNSP PGUID 12 f f f f i 3 703 t "703 20 703" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(reltime, int8) => reltime */ 
DATA(insert OID = 7617 ( lag  PGNSP PGUID 12 f f f f i 2 703 t "703 20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(reltime) => reltime */ 
DATA(insert OID = 7619 ( lag  PGNSP PGUID 12 f f f f i 1 703 t "703" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(text, int8, text) => text */ 
DATA(insert OID = 7621 ( lag  PGNSP PGUID 12 f f f f i 3 25 t "25 20 25" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(text, int8) => text */ 
DATA(insert OID = 7623 ( lag  PGNSP PGUID 12 f f f f i 2 25 t "25 20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(text) => text */ 
DATA(insert OID = 7625 ( lag  PGNSP PGUID 12 f f f f i 1 25 t "25" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(tid, int8, tid) => tid */ 
DATA(insert OID = 7627 ( lag  PGNSP PGUID 12 f f f f i 3 27 t "27 20 27" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(tid, int8) => tid */ 
DATA(insert OID = 7629 ( lag  PGNSP PGUID 12 f f f f i 2 27 t "27 20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(tid) => tid */ 
DATA(insert OID = 7631 ( lag  PGNSP PGUID 12 f f f f i 1 27 t "27" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag("time", int8, "time") => "time" */ 
DATA(insert OID = 7633 ( lag  PGNSP PGUID 12 f f f f i 3 1083 t "1083 20 1083" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag("time", int8) => "time" */ 
DATA(insert OID = 7635 ( lag  PGNSP PGUID 12 f f f f i 2 1083 t "1083 20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag("time") => "time" */ 
DATA(insert OID = 7637 ( lag  PGNSP PGUID 12 f f f f i 1 1083 t "1083" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag("timestamp", int8, "timestamp") => "timestamp" */ 
DATA(insert OID = 7639 ( lag  PGNSP PGUID 12 f f f f i 3 1114 t "1114 20 1114" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag("timestamp", int8) => "timestamp" */ 
DATA(insert OID = 7641 ( lag  PGNSP PGUID 12 f f f f i 2 1114 t "1114 20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag("timestamp") => "timestamp" */ 
DATA(insert OID = 7643 ( lag  PGNSP PGUID 12 f f f f i 1 1114 t "1114" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(timestamptz, int8, timestamptz) => timestamptz */ 
DATA(insert OID = 7645 ( lag  PGNSP PGUID 12 f f f f i 3 1184 t "1184 20 1184" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(timestamptz, int8) => timestamptz */ 
DATA(insert OID = 7647 ( lag  PGNSP PGUID 12 f f f f i 2 1184 t "1184 20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(timestamptz) => timestamptz */ 
DATA(insert OID = 7649 ( lag  PGNSP PGUID 12 f f f f i 1 1184 t "1184" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(timetz, int8, timetz) => timetz */ 
DATA(insert OID = 7651 ( lag  PGNSP PGUID 12 f f f f i 3 1266 t "1266 20 1266" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(timetz, int8) => timetz */ 
DATA(insert OID = 7653 ( lag  PGNSP PGUID 12 f f f f i 2 1266 t "1266 20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(timetz) => timetz */ 
DATA(insert OID = 7655 ( lag  PGNSP PGUID 12 f f f f i 1 1266 t "1266" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(varbit, int8, varbit) => varbit */ 
DATA(insert OID = 7657 ( lag  PGNSP PGUID 12 f f f f i 3 1562 t "1562 20 1562" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(varbit, int8) => varbit */ 
DATA(insert OID = 7659 ( lag  PGNSP PGUID 12 f f f f i 2 1562 t "1562 20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(varbit) => varbit */ 
DATA(insert OID = 7661 ( lag  PGNSP PGUID 12 f f f f i 1 1562 t "1562" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag("varchar", int8, "varchar") => "varchar" */ 
DATA(insert OID = 7663 ( lag  PGNSP PGUID 12 f f f f i 3 1043 t "1043 20 1043" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag("varchar", int8) => "varchar" */ 
DATA(insert OID = 7665 ( lag  PGNSP PGUID 12 f f f f i 2 1043 t "1043 20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag("varchar") => "varchar" */ 
DATA(insert OID = 7667 ( lag  PGNSP PGUID 12 f f f f i 1 1043 t "1043" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(xid, int8, xid) => xid */ 
DATA(insert OID = 7669 ( lag  PGNSP PGUID 12 f f f f i 3 28 t "28 20 28" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(xid, int8) => xid */ 
DATA(insert OID = 7671 ( lag  PGNSP PGUID 12 f f f f i 2 28 t "28 20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(xid) => xid */ 
DATA(insert OID = 7673 ( lag  PGNSP PGUID 12 f f f f i 1 28 t "28" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(anyarray, int8, anyarray) => anyarray */ 
DATA(insert OID = 7211 ( lag  PGNSP PGUID 12 f f f f i 3 2277 t "2277 20 2277" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(anyarray, int8) => anyarray */ 
DATA(insert OID = 7212 ( lag  PGNSP PGUID 12 f f f f i 2 2277 t "2277 20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(anyarray) => anyarray */ 
DATA(insert OID = 7213 ( lag  PGNSP PGUID 12 f f f f i 1 2277 t "2277" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(bytea) => bytea */ 
DATA(insert OID = 7226 ( lag  PGNSP PGUID 12 f f f f i 1 17 t "17" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(bytea, int8) => bytea */ 
DATA(insert OID = 7228 ( lag  PGNSP PGUID 12 f f f f i 2 17 t "17 20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(bytea, int8, bytea) => bytea */ 
DATA(insert OID = 7230 ( lag  PGNSP PGUID 12 f f f f i 3 17 t "17 20 17" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag("bit") => "bit" */ 
DATA(insert OID = 7250 ( lag  PGNSP PGUID 12 f f f f i 1 1560 t "1560" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag("bit", int8) => "bit" */ 
DATA(insert OID = 7252 ( lag  PGNSP PGUID 12 f f f f i 2 1560 t "1560 20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag("bit", int8, "bit") => "bit" */ 
DATA(insert OID = 7254 ( lag  PGNSP PGUID 12 f f f f i 3 1560 t "1560 20 1560" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(box) => box */ 
DATA(insert OID = 7266 ( lag  PGNSP PGUID 12 f f f f i 1 603 t "603" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(box, int8) => box */ 
DATA(insert OID = 7268 ( lag  PGNSP PGUID 12 f f f f i 2 603 t "603 20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lag(box, int8, box) => box */ 
DATA(insert OID = 7270 ( lag  PGNSP PGUID 12 f f f f i 3 603 t "603 20 603" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(int4, int8, int4) => int4 */ 
DATA(insert OID = 7011 ( lead  PGNSP PGUID 12 f f f f i 3 23 t "23 20 23" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(int4, int8) => int4 */ 
DATA(insert OID = 7074 ( lead  PGNSP PGUID 12 f f f f i 2 23 t "23 20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(int4) => int4 */ 
DATA(insert OID = 7075 ( lead  PGNSP PGUID 12 f f f f i 1 23 t "23" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(bool, int8, bool) => bool */ 
DATA(insert OID = 7310 ( lead  PGNSP PGUID 12 f f f f i 3 16 t "16 20 16" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(bool, int8) => bool */ 
DATA(insert OID = 7312 ( lead  PGNSP PGUID 12 f f f f i 2 16 t "16 20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(bool) => bool */ 
DATA(insert OID = 7314 ( lead  PGNSP PGUID 12 f f f f i 1 16 t "16" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead("char", int8, "char") => "char" */ 
DATA(insert OID = 7316 ( lead  PGNSP PGUID 12 f f f f i 3 18 t "18 20 18" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead("char", int8) => "char" */ 
DATA(insert OID = 7318 ( lead  PGNSP PGUID 12 f f f f i 2 18 t "18 20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead("char") => "char" */ 
DATA(insert OID = 7320 ( lead  PGNSP PGUID 12 f f f f i 1 18 t "18" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(cidr, int8, cidr) => cidr */ 
DATA(insert OID = 7322 ( lead  PGNSP PGUID 12 f f f f i 3 650 t "650 20 650" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(cidr, int8) => cidr */ 
DATA(insert OID = 7324 ( lead  PGNSP PGUID 12 f f f f i 2 650 t "650 20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(cidr) => cidr */ 
DATA(insert OID = 7326 ( lead  PGNSP PGUID 12 f f f f i 1 650 t "650" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(circle, int8, circle) => circle */ 
DATA(insert OID = 7328 ( lead  PGNSP PGUID 12 f f f f i 3 718 t "718 20 718" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(circle, int8) => circle */ 
DATA(insert OID = 7330 ( lead  PGNSP PGUID 12 f f f f i 2 718 t "718 20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(circle) => circle */ 
DATA(insert OID = 7332 ( lead  PGNSP PGUID 12 f f f f i 1 718 t "718" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(float4, int8, float4) => float4 */ 
DATA(insert OID = 7334 ( lead  PGNSP PGUID 12 f f f f i 3 700 t "700 20 700" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(float4, int8) => float4 */ 
DATA(insert OID = 7336 ( lead  PGNSP PGUID 12 f f f f i 2 700 t "700 20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(float4) => float4 */ 
DATA(insert OID = 7338 ( lead  PGNSP PGUID 12 f f f f i 1 700 t "700" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(float8, int8, float8) => float8 */ 
DATA(insert OID = 7340 ( lead  PGNSP PGUID 12 f f f f i 3 701 t "701 20 701" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(float8, int8) => float8 */ 
DATA(insert OID = 7342 ( lead  PGNSP PGUID 12 f f f f i 2 701 t "701 20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(float8) => float8 */ 
DATA(insert OID = 7344 ( lead  PGNSP PGUID 12 f f f f i 1 701 t "701" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(inet, int8, inet) => inet */ 
DATA(insert OID = 7346 ( lead  PGNSP PGUID 12 f f f f i 3 869 t "869 20 869" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(inet, int8) => inet */ 
DATA(insert OID = 7348 ( lead  PGNSP PGUID 12 f f f f i 2 869 t "869 20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(inet) => inet */ 
DATA(insert OID = 7350 ( lead  PGNSP PGUID 12 f f f f i 1 869 t "869" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead("interval", int8, "interval") => "interval" */ 
DATA(insert OID = 7352 ( lead  PGNSP PGUID 12 f f f f i 3 1186 t "1186 20 1186" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead("interval", int8) => "interval" */ 
DATA(insert OID = 7354 ( lead  PGNSP PGUID 12 f f f f i 2 1186 t "1186 20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead("interval") => "interval" */ 
DATA(insert OID = 7356 ( lead  PGNSP PGUID 12 f f f f i 1 1186 t "1186" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(line, int8, line) => line */ 
DATA(insert OID = 7358 ( lead  PGNSP PGUID 12 f f f f i 3 628 t "628 20 628" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(line, int8) => line */ 
DATA(insert OID = 7360 ( lead  PGNSP PGUID 12 f f f f i 2 628 t "628 20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(line) => line */ 
DATA(insert OID = 7362 ( lead  PGNSP PGUID 12 f f f f i 1 628 t "628" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(lseg, int8, lseg) => lseg */ 
DATA(insert OID = 7364 ( lead  PGNSP PGUID 12 f f f f i 3 601 t "601 20 601" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(lseg, int8) => lseg */ 
DATA(insert OID = 7366 ( lead  PGNSP PGUID 12 f f f f i 2 601 t "601 20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(lseg) => lseg */ 
DATA(insert OID = 7368 ( lead  PGNSP PGUID 12 f f f f i 1 601 t "601" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(macaddr, int8, macaddr) => macaddr */ 
DATA(insert OID = 7370 ( lead  PGNSP PGUID 12 f f f f i 3 829 t "829 20 829" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(macaddr, int8) => macaddr */ 
DATA(insert OID = 7372 ( lead  PGNSP PGUID 12 f f f f i 2 829 t "829 20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(macaddr) => macaddr */ 
DATA(insert OID = 7374 ( lead  PGNSP PGUID 12 f f f f i 1 829 t "829" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(int2, int8, int2) => int2 */ 
DATA(insert OID = 7376 ( lead  PGNSP PGUID 12 f f f f i 3 21 t "21 20 21" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(int2, int8) => int2 */ 
DATA(insert OID = 7378 ( lead  PGNSP PGUID 12 f f f f i 2 21 t "21 20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(int2) => int2 */ 
DATA(insert OID = 7380 ( lead  PGNSP PGUID 12 f f f f i 1 21 t "21" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(int8, int8, int8) => int8 */ 
DATA(insert OID = 7382 ( lead  PGNSP PGUID 12 f f f f i 3 20 t "20 20 20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(int8, int8) => int8 */ 
DATA(insert OID = 7384 ( lead  PGNSP PGUID 12 f f f f i 2 20 t "20 20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(int8) => int8 */ 
DATA(insert OID = 7386 ( lead  PGNSP PGUID 12 f f f f i 1 20 t "20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(money, int8, money) => money */ 
DATA(insert OID = 7388 ( lead  PGNSP PGUID 12 f f f f i 3 790 t "790 20 790" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(money, int8) => money */ 
DATA(insert OID = 7390 ( lead  PGNSP PGUID 12 f f f f i 2 790 t "790 20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(money) => money */ 
DATA(insert OID = 7392 ( lead  PGNSP PGUID 12 f f f f i 1 790 t "790" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(name, int8, name) => name */ 
DATA(insert OID = 7394 ( lead  PGNSP PGUID 12 f f f f i 3 19 t "19 20 19" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(name, int8) => name */ 
DATA(insert OID = 7396 ( lead  PGNSP PGUID 12 f f f f i 2 19 t "19 20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(name) => name */ 
DATA(insert OID = 7398 ( lead  PGNSP PGUID 12 f f f f i 1 19 t "19" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead("numeric", int8, "numeric") => "numeric" */ 
DATA(insert OID = 7400 ( lead  PGNSP PGUID 12 f f f f i 3 1700 t "1700 20 1700" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead("numeric", int8) => "numeric" */ 
DATA(insert OID = 7402 ( lead  PGNSP PGUID 12 f f f f i 2 1700 t "1700 20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead("numeric") => "numeric" */ 
DATA(insert OID = 7404 ( lead  PGNSP PGUID 12 f f f f i 1 1700 t "1700" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(oid, int8, oid) => oid */ 
DATA(insert OID = 7406 ( lead  PGNSP PGUID 12 f f f f i 3 26 t "26 20 26" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(oid, int8) => oid */ 
DATA(insert OID = 7408 ( lead  PGNSP PGUID 12 f f f f i 2 26 t "26 20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(oid) => oid */ 
DATA(insert OID = 7410 ( lead  PGNSP PGUID 12 f f f f i 1 26 t "26" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(path, int8, path) => path */ 
DATA(insert OID = 7412 ( lead  PGNSP PGUID 12 f f f f i 3 602 t "602 20 602" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(path, int8) => path */ 
DATA(insert OID = 7414 ( lead  PGNSP PGUID 12 f f f f i 2 602 t "602 20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(path) => path */ 
DATA(insert OID = 7416 ( lead  PGNSP PGUID 12 f f f f i 1 602 t "602" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(point, int8, point) => point */ 
DATA(insert OID = 7418 ( lead  PGNSP PGUID 12 f f f f i 3 600 t "600 20 600" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(point, int8) => point */ 
DATA(insert OID = 7420 ( lead  PGNSP PGUID 12 f f f f i 2 600 t "600 20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(point) => point */ 
DATA(insert OID = 7422 ( lead  PGNSP PGUID 12 f f f f i 1 600 t "600" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(polygon, int8, polygon) => polygon */ 
DATA(insert OID = 7424 ( lead  PGNSP PGUID 12 f f f f i 3 604 t "604 20 604" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(polygon, int8) => polygon */ 
DATA(insert OID = 7426 ( lead  PGNSP PGUID 12 f f f f i 2 604 t "604 20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(polygon) => polygon */ 
DATA(insert OID = 7428 ( lead  PGNSP PGUID 12 f f f f i 1 604 t "604" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(reltime, int8, reltime) => reltime */ 
DATA(insert OID = 7430 ( lead  PGNSP PGUID 12 f f f f i 3 703 t "703 20 703" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(reltime, int8) => reltime */ 
DATA(insert OID = 7432 ( lead  PGNSP PGUID 12 f f f f i 2 703 t "703 20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(reltime) => reltime */ 
DATA(insert OID = 7434 ( lead  PGNSP PGUID 12 f f f f i 1 703 t "703" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(text, int8, text) => text */ 
DATA(insert OID = 7436 ( lead  PGNSP PGUID 12 f f f f i 3 25 t "25 20 25" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(text, int8) => text */ 
DATA(insert OID = 7438 ( lead  PGNSP PGUID 12 f f f f i 2 25 t "25 20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(text) => text */ 
DATA(insert OID = 7440 ( lead  PGNSP PGUID 12 f f f f i 1 25 t "25" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(tid, int8, tid) => tid */ 
DATA(insert OID = 7442 ( lead  PGNSP PGUID 12 f f f f i 3 27 t "27 20 27" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(tid, int8) => tid */ 
DATA(insert OID = 7444 ( lead  PGNSP PGUID 12 f f f f i 2 27 t "27 20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(tid) => tid */ 
DATA(insert OID = 7446 ( lead  PGNSP PGUID 12 f f f f i 1 27 t "27" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead("time", int8, "time") => "time" */ 
DATA(insert OID = 7448 ( lead  PGNSP PGUID 12 f f f f i 3 1083 t "1083 20 1083" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead("time", int8) => "time" */ 
DATA(insert OID = 7450 ( lead  PGNSP PGUID 12 f f f f i 2 1083 t "1083 20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead("time") => "time" */ 
DATA(insert OID = 7452 ( lead  PGNSP PGUID 12 f f f f i 1 1083 t "1083" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead("timestamp", int8, "timestamp") => "timestamp" */ 
DATA(insert OID = 7454 ( lead  PGNSP PGUID 12 f f f f i 3 1114 t "1114 20 1114" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead("timestamp", int8) => "timestamp" */ 
DATA(insert OID = 7456 ( lead  PGNSP PGUID 12 f f f f i 2 1114 t "1114 20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead("timestamp") => "timestamp" */ 
DATA(insert OID = 7458 ( lead  PGNSP PGUID 12 f f f f i 1 1114 t "1114" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(timestamptz, int8, timestamptz) => timestamptz */ 
DATA(insert OID = 7460 ( lead  PGNSP PGUID 12 f f f f i 3 1184 t "1184 20 1184" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(timestamptz, int8) => timestamptz */ 
DATA(insert OID = 7462 ( lead  PGNSP PGUID 12 f f f f i 2 1184 t "1184 20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(timestamptz) => timestamptz */ 
DATA(insert OID = 7464 ( lead  PGNSP PGUID 12 f f f f i 1 1184 t "1184" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(timetz, int8, timetz) => timetz */ 
DATA(insert OID = 7466 ( lead  PGNSP PGUID 12 f f f f i 3 1266 t "1266 20 1266" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(timetz, int8) => timetz */ 
DATA(insert OID = 7468 ( lead  PGNSP PGUID 12 f f f f i 2 1266 t "1266 20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(timetz) => timetz */ 
DATA(insert OID = 7470 ( lead  PGNSP PGUID 12 f f f f i 1 1266 t "1266" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(varbit, int8, varbit) => varbit */ 
DATA(insert OID = 7472 ( lead  PGNSP PGUID 12 f f f f i 3 1562 t "1562 20 1562" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(varbit, int8) => varbit */ 
DATA(insert OID = 7474 ( lead  PGNSP PGUID 12 f f f f i 2 1562 t "1562 20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(varbit) => varbit */ 
DATA(insert OID = 7476 ( lead  PGNSP PGUID 12 f f f f i 1 1562 t "1562" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead("varchar", int8, "varchar") => "varchar" */ 
DATA(insert OID = 7478 ( lead  PGNSP PGUID 12 f f f f i 3 1043 t "1043 20 1043" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead("varchar", int8) => "varchar" */ 
DATA(insert OID = 7480 ( lead  PGNSP PGUID 12 f f f f i 2 1043 t "1043 20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead("varchar") => "varchar" */ 
DATA(insert OID = 7482 ( lead  PGNSP PGUID 12 f f f f i 1 1043 t "1043" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(xid, int8, xid) => xid */ 
DATA(insert OID = 7484 ( lead  PGNSP PGUID 12 f f f f i 3 28 t "28 20 28" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(xid, int8) => xid */ 
DATA(insert OID = 7486 ( lead  PGNSP PGUID 12 f f f f i 2 28 t "28 20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(xid) => xid */ 
DATA(insert OID = 7488 ( lead  PGNSP PGUID 12 f f f f i 1 28 t "28" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(anyarray, int8, anyarray) => anyarray */ 
DATA(insert OID = 7214 ( lead  PGNSP PGUID 12 f f f f i 3 2277 t "2277 20 2277" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(anyarray, int8) => anyarray */ 
DATA(insert OID = 7215 ( lead  PGNSP PGUID 12 f f f f i 2 2277 t "2277 20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(anyarray) => anyarray */ 
DATA(insert OID = 7216 ( lead  PGNSP PGUID 12 f f f f i 1 2277 t "2277" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(bytea) => bytea */ 
DATA(insert OID = 7220 ( lead  PGNSP PGUID 12 f f f f i 1 17 t "17" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(bytea, int8) => bytea */ 
DATA(insert OID = 7222 ( lead  PGNSP PGUID 12 f f f f i 2 17 t "17 20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(bytea, int8, bytea) => bytea */ 
DATA(insert OID = 7224 ( lead  PGNSP PGUID 12 f f f f i 3 17 t "17 20 17" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead("bit") => "bit" */ 
DATA(insert OID = 7244 ( lead  PGNSP PGUID 12 f f f f i 1 1560 t "1560" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead("bit", int8) => "bit" */ 
DATA(insert OID = 7246 ( lead  PGNSP PGUID 12 f f f f i 2 1560 t "1560 20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead("bit", int8, "bit") => "bit" */ 
DATA(insert OID = 7248 ( lead  PGNSP PGUID 12 f f f f i 3 1560 t "1560 20 1560" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(box) => box */ 
DATA(insert OID = 7260 ( lead  PGNSP PGUID 12 f f f f i 1 603 t "603" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(box, int8) => box */ 
DATA(insert OID = 7262 ( lead  PGNSP PGUID 12 f f f f i 2 603 t "603 20" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));

/* lead(box, int8, box) => box */ 
DATA(insert OID = 7264 ( lead  PGNSP PGUID 12 f f f f i 3 603 t "603 20 603" _null_ _null_ _null_ window_dummy _null_ _null_ n 0));


/* We use the same procedures here as above since the types are binary compatible.  */
/* bpchar_pattern_lt(bpchar, bpchar) => bool */ 
DATA(insert OID = 2174 ( bpchar_pattern_lt  PGNSP PGUID 12 f f t f i 2 16 f "1042 1042" _null_ _null_ _null_ text_pattern_lt _null_ _null_ n 0));

/* bpchar_pattern_le(bpchar, bpchar) => bool */ 
DATA(insert OID = 2175 ( bpchar_pattern_le  PGNSP PGUID 12 f f t f i 2 16 f "1042 1042" _null_ _null_ _null_ text_pattern_le _null_ _null_ n 0));

/* bpchar_pattern_eq(bpchar, bpchar) => bool */ 
DATA(insert OID = 2176 ( bpchar_pattern_eq  PGNSP PGUID 12 f f t f i 2 16 f "1042 1042" _null_ _null_ _null_ texteq _null_ _null_ n 0));

/* bpchar_pattern_ge(bpchar, bpchar) => bool */ 
DATA(insert OID = 2177 ( bpchar_pattern_ge  PGNSP PGUID 12 f f t f i 2 16 f "1042 1042" _null_ _null_ _null_ text_pattern_ge _null_ _null_ n 0));

/* bpchar_pattern_gt(bpchar, bpchar) => bool */ 
DATA(insert OID = 2178 ( bpchar_pattern_gt  PGNSP PGUID 12 f f t f i 2 16 f "1042 1042" _null_ _null_ _null_ text_pattern_gt _null_ _null_ n 0));

/* bpchar_pattern_ne(bpchar, bpchar) => bool */ 
DATA(insert OID = 2179 ( bpchar_pattern_ne  PGNSP PGUID 12 f f t f i 2 16 f "1042 1042" _null_ _null_ _null_ textne _null_ _null_ n 0));

/* btbpchar_pattern_cmp(bpchar, bpchar) => int4 */ 
DATA(insert OID = 2180 ( btbpchar_pattern_cmp  PGNSP PGUID 12 f f t f i 2 23 f "1042 1042" _null_ _null_ _null_ bttext_pattern_cmp _null_ _null_ n 0));

/* name_pattern_lt(name, name) => bool */ 
DATA(insert OID = 2181 ( name_pattern_lt  PGNSP PGUID 12 f f t f i 2 16 f "19 19" _null_ _null_ _null_ name_pattern_lt _null_ _null_ n 0));

/* name_pattern_le(name, name) => bool */ 
DATA(insert OID = 2182 ( name_pattern_le  PGNSP PGUID 12 f f t f i 2 16 f "19 19" _null_ _null_ _null_ name_pattern_le _null_ _null_ n 0));

/* name_pattern_eq(name, name) => bool */ 
DATA(insert OID = 2183 ( name_pattern_eq  PGNSP PGUID 12 f f t f i 2 16 f "19 19" _null_ _null_ _null_ name_pattern_eq _null_ _null_ n 0));

/* name_pattern_ge(name, name) => bool */ 
DATA(insert OID = 2184 ( name_pattern_ge  PGNSP PGUID 12 f f t f i 2 16 f "19 19" _null_ _null_ _null_ name_pattern_ge _null_ _null_ n 0));

/* name_pattern_gt(name, name) => bool */ 
DATA(insert OID = 2185 ( name_pattern_gt  PGNSP PGUID 12 f f t f i 2 16 f "19 19" _null_ _null_ _null_ name_pattern_gt _null_ _null_ n 0));

/* name_pattern_ne(name, name) => bool */ 
DATA(insert OID = 2186 ( name_pattern_ne  PGNSP PGUID 12 f f t f i 2 16 f "19 19" _null_ _null_ _null_ name_pattern_ne _null_ _null_ n 0));

/* btname_pattern_cmp(name, name) => int4 */ 
DATA(insert OID = 2187 ( btname_pattern_cmp  PGNSP PGUID 12 f f t f i 2 23 f "19 19" _null_ _null_ _null_ btname_pattern_cmp _null_ _null_ n 0));

/* btint48cmp(int4, int8) => int4 */ 
DATA(insert OID = 2188 ( btint48cmp  PGNSP PGUID 12 f f t f i 2 23 f "23 20" _null_ _null_ _null_ btint48cmp _null_ _null_ n 0));

/* btint84cmp(int8, int4) => int4 */ 
DATA(insert OID = 2189 ( btint84cmp  PGNSP PGUID 12 f f t f i 2 23 f "20 23" _null_ _null_ _null_ btint84cmp _null_ _null_ n 0));

/* btint24cmp(int2, int4) => int4 */ 
DATA(insert OID = 2190 ( btint24cmp  PGNSP PGUID 12 f f t f i 2 23 f "21 23" _null_ _null_ _null_ btint24cmp _null_ _null_ n 0));

/* btint42cmp(int4, int2) => int4 */ 
DATA(insert OID = 2191 ( btint42cmp  PGNSP PGUID 12 f f t f i 2 23 f "23 21" _null_ _null_ _null_ btint42cmp _null_ _null_ n 0));

/* btint28cmp(int2, int8) => int4 */ 
DATA(insert OID = 2192 ( btint28cmp  PGNSP PGUID 12 f f t f i 2 23 f "21 20" _null_ _null_ _null_ btint28cmp _null_ _null_ n 0));

/* btint82cmp(int8, int2) => int4 */ 
DATA(insert OID = 2193 ( btint82cmp  PGNSP PGUID 12 f f t f i 2 23 f "20 21" _null_ _null_ _null_ btint82cmp _null_ _null_ n 0));

/* btfloat48cmp(float4, float8) => int4 */ 
DATA(insert OID = 2194 ( btfloat48cmp  PGNSP PGUID 12 f f t f i 2 23 f "700 701" _null_ _null_ _null_ btfloat48cmp _null_ _null_ n 0));

/* btfloat84cmp(float8, float4) => int4 */ 
DATA(insert OID = 2195 ( btfloat84cmp  PGNSP PGUID 12 f f t f i 2 23 f "701 700" _null_ _null_ _null_ btfloat84cmp _null_ _null_ n 0));

/* pg_options_to_table(IN options_array _text, OUT option_name text, OUT option_value text) => SETOF pg_catalog.record */ 
DATA(insert OID = 2896 ( pg_options_to_table  PGNSP PGUID 12 f f t t s 1 2249 f "1009" "{1009,25,25}" "{i,o,o}" "{options_array,option_name,option_value}" pg_options_to_table _null_ _null_ n 0));
DESCR("convert generic options array to name/value table");

/* regprocedurein(cstring) => regprocedure */ 
DATA(insert OID = 2212 ( regprocedurein  PGNSP PGUID 12 f f t f s 1 2202 f "2275" _null_ _null_ _null_ regprocedurein _null_ _null_ n 0));
DESCR("I/O");

/* regprocedureout(regprocedure) => cstring */ 
DATA(insert OID = 2213 ( regprocedureout  PGNSP PGUID 12 f f t f s 1 2275 f "2202" _null_ _null_ _null_ regprocedureout _null_ _null_ n 0));
DESCR("I/O");

/* regoperin(cstring) => regoper */ 
DATA(insert OID = 2214 ( regoperin  PGNSP PGUID 12 f f t f s 1 2203 f "2275" _null_ _null_ _null_ regoperin _null_ _null_ n 0));
DESCR("I/O");

/* regoperout(regoper) => cstring */ 
DATA(insert OID = 2215 ( regoperout  PGNSP PGUID 12 f f t f s 1 2275 f "2203" _null_ _null_ _null_ regoperout _null_ _null_ n 0));
DESCR("I/O");

/* regoperatorin(cstring) => regoperator */ 
DATA(insert OID = 2216 ( regoperatorin  PGNSP PGUID 12 f f t f s 1 2204 f "2275" _null_ _null_ _null_ regoperatorin _null_ _null_ n 0));
DESCR("I/O");

/* regoperatorout(regoperator) => cstring */ 
DATA(insert OID = 2217 ( regoperatorout  PGNSP PGUID 12 f f t f s 1 2275 f "2204" _null_ _null_ _null_ regoperatorout _null_ _null_ n 0));
DESCR("I/O");

/* regclassin(cstring) => regclass */ 
DATA(insert OID = 2218 ( regclassin  PGNSP PGUID 12 f f t f s 1 2205 f "2275" _null_ _null_ _null_ regclassin _null_ _null_ n 0));
DESCR("I/O");

/* regclassout(regclass) => cstring */ 
DATA(insert OID = 2219 ( regclassout  PGNSP PGUID 12 f f t f s 1 2275 f "2205" _null_ _null_ _null_ regclassout _null_ _null_ n 0));
DESCR("I/O");

/* regtypein(cstring) => regtype */ 
DATA(insert OID = 2220 ( regtypein  PGNSP PGUID 12 f f t f s 1 2206 f "2275" _null_ _null_ _null_ regtypein _null_ _null_ n 0));
DESCR("I/O");

/* regtypeout(regtype) => cstring */ 
DATA(insert OID = 2221 ( regtypeout  PGNSP PGUID 12 f f t f s 1 2275 f "2206" _null_ _null_ _null_ regtypeout _null_ _null_ n 0));
DESCR("I/O");

/* regclass(text) => regclass */ 
DATA(insert OID = 1079 ( regclass  PGNSP PGUID 12 f f t f s 1 2205 f "25" _null_ _null_ _null_ text_regclass _null_ _null_ n 0));
DESCR("convert text to regclass");

/* fmgr_internal_validator(oid) => void */ 
DATA(insert OID = 2246 ( fmgr_internal_validator  PGNSP PGUID 12 f f t f s 1 2278 f "26" _null_ _null_ _null_ fmgr_internal_validator _null_ _null_ n 0));
DESCR("(internal)");

/* fmgr_c_validator(oid) => void */ 
DATA(insert OID = 2247 ( fmgr_c_validator  PGNSP PGUID 12 f f t f s 1 2278 f "26" _null_ _null_ _null_ fmgr_c_validator _null_ _null_ n 0));
DESCR("(internal)");

/* fmgr_sql_validator(oid) => void */ 
DATA(insert OID = 2248 ( fmgr_sql_validator  PGNSP PGUID 12 f f t f s 1 2278 f "26" _null_ _null_ _null_ fmgr_sql_validator _null_ _null_ n 0));
DESCR("(internal)");

/* has_database_privilege(name, text, text) => bool */ 
DATA(insert OID = 2250 ( has_database_privilege  PGNSP PGUID 12 f f t f s 3 16 f "19 25 25" _null_ _null_ _null_ has_database_privilege_name_name _null_ _null_ n 0));
DESCR("user privilege on database by username, database name");

/* has_database_privilege(name, oid, text) => bool */ 
DATA(insert OID = 2251 ( has_database_privilege  PGNSP PGUID 12 f f t f s 3 16 f "19 26 25" _null_ _null_ _null_ has_database_privilege_name_id _null_ _null_ n 0));
DESCR("user privilege on database by username, database oid");

/* has_database_privilege(oid, text, text) => bool */ 
DATA(insert OID = 2252 ( has_database_privilege  PGNSP PGUID 12 f f t f s 3 16 f "26 25 25" _null_ _null_ _null_ has_database_privilege_id_name _null_ _null_ n 0));
DESCR("user privilege on database by user oid, database name");

/* has_database_privilege(oid, oid, text) => bool */ 
DATA(insert OID = 2253 ( has_database_privilege  PGNSP PGUID 12 f f t f s 3 16 f "26 26 25" _null_ _null_ _null_ has_database_privilege_id_id _null_ _null_ n 0));
DESCR("user privilege on database by user oid, database oid");

/* has_database_privilege(text, text) => bool */ 
DATA(insert OID = 2254 ( has_database_privilege  PGNSP PGUID 12 f f t f s 2 16 f "25 25" _null_ _null_ _null_ has_database_privilege_name _null_ _null_ n 0));
DESCR("current user privilege on database by database name");

/* has_database_privilege(oid, text) => bool */ 
DATA(insert OID = 2255 ( has_database_privilege  PGNSP PGUID 12 f f t f s 2 16 f "26 25" _null_ _null_ _null_ has_database_privilege_id _null_ _null_ n 0));
DESCR("current user privilege on database by database oid");

/* has_function_privilege(name, text, text) => bool */ 
DATA(insert OID = 2256 ( has_function_privilege  PGNSP PGUID 12 f f t f s 3 16 f "19 25 25" _null_ _null_ _null_ has_function_privilege_name_name _null_ _null_ n 0));
DESCR("user privilege on function by username, function name");

/* has_function_privilege(name, oid, text) => bool */ 
DATA(insert OID = 2257 ( has_function_privilege  PGNSP PGUID 12 f f t f s 3 16 f "19 26 25" _null_ _null_ _null_ has_function_privilege_name_id _null_ _null_ n 0));
DESCR("user privilege on function by username, function oid");

/* has_function_privilege(oid, text, text) => bool */ 
DATA(insert OID = 2258 ( has_function_privilege  PGNSP PGUID 12 f f t f s 3 16 f "26 25 25" _null_ _null_ _null_ has_function_privilege_id_name _null_ _null_ n 0));
DESCR("user privilege on function by user oid, function name");

/* has_function_privilege(oid, oid, text) => bool */ 
DATA(insert OID = 2259 ( has_function_privilege  PGNSP PGUID 12 f f t f s 3 16 f "26 26 25" _null_ _null_ _null_ has_function_privilege_id_id _null_ _null_ n 0));
DESCR("user privilege on function by user oid, function oid");

/* has_function_privilege(text, text) => bool */ 
DATA(insert OID = 2260 ( has_function_privilege  PGNSP PGUID 12 f f t f s 2 16 f "25 25" _null_ _null_ _null_ has_function_privilege_name _null_ _null_ n 0));
DESCR("current user privilege on function by function name");

/* has_function_privilege(oid, text) => bool */ 
DATA(insert OID = 2261 ( has_function_privilege  PGNSP PGUID 12 f f t f s 2 16 f "26 25" _null_ _null_ _null_ has_function_privilege_id _null_ _null_ n 0));
DESCR("current user privilege on function by function oid");

/* has_language_privilege(name, text, text) => bool */ 
DATA(insert OID = 2262 ( has_language_privilege  PGNSP PGUID 12 f f t f s 3 16 f "19 25 25" _null_ _null_ _null_ has_language_privilege_name_name _null_ _null_ n 0));
DESCR("user privilege on language by username, language name");

/* has_language_privilege(name, oid, text) => bool */ 
DATA(insert OID = 2263 ( has_language_privilege  PGNSP PGUID 12 f f t f s 3 16 f "19 26 25" _null_ _null_ _null_ has_language_privilege_name_id _null_ _null_ n 0));
DESCR("user privilege on language by username, language oid");

/* has_language_privilege(oid, text, text) => bool */ 
DATA(insert OID = 2264 ( has_language_privilege  PGNSP PGUID 12 f f t f s 3 16 f "26 25 25" _null_ _null_ _null_ has_language_privilege_id_name _null_ _null_ n 0));
DESCR("user privilege on language by user oid, language name");

/* has_language_privilege(oid, oid, text) => bool */ 
DATA(insert OID = 2265 ( has_language_privilege  PGNSP PGUID 12 f f t f s 3 16 f "26 26 25" _null_ _null_ _null_ has_language_privilege_id_id _null_ _null_ n 0));
DESCR("user privilege on language by user oid, language oid");

/* has_language_privilege(text, text) => bool */ 
DATA(insert OID = 2266 ( has_language_privilege  PGNSP PGUID 12 f f t f s 2 16 f "25 25" _null_ _null_ _null_ has_language_privilege_name _null_ _null_ n 0));
DESCR("current user privilege on language by language name");

/* has_language_privilege(oid, text) => bool */ 
DATA(insert OID = 2267 ( has_language_privilege  PGNSP PGUID 12 f f t f s 2 16 f "26 25" _null_ _null_ _null_ has_language_privilege_id _null_ _null_ n 0));
DESCR("current user privilege on language by language oid");

/* has_schema_privilege(name, text, text) => bool */ 
DATA(insert OID = 2268 ( has_schema_privilege  PGNSP PGUID 12 f f t f s 3 16 f "19 25 25" _null_ _null_ _null_ has_schema_privilege_name_name _null_ _null_ n 0));
DESCR("user privilege on schema by username, schema name");

/* has_schema_privilege(name, oid, text) => bool */ 
DATA(insert OID = 2269 ( has_schema_privilege  PGNSP PGUID 12 f f t f s 3 16 f "19 26 25" _null_ _null_ _null_ has_schema_privilege_name_id _null_ _null_ n 0));
DESCR("user privilege on schema by username, schema oid");

/* has_schema_privilege(oid, text, text) => bool */ 
DATA(insert OID = 2270 ( has_schema_privilege  PGNSP PGUID 12 f f t f s 3 16 f "26 25 25" _null_ _null_ _null_ has_schema_privilege_id_name _null_ _null_ n 0));
DESCR("user privilege on schema by user oid, schema name");

/* has_schema_privilege(oid, oid, text) => bool */ 
DATA(insert OID = 2271 ( has_schema_privilege  PGNSP PGUID 12 f f t f s 3 16 f "26 26 25" _null_ _null_ _null_ has_schema_privilege_id_id _null_ _null_ n 0));
DESCR("user privilege on schema by user oid, schema oid");

/* has_schema_privilege(text, text) => bool */ 
DATA(insert OID = 2272 ( has_schema_privilege  PGNSP PGUID 12 f f t f s 2 16 f "25 25" _null_ _null_ _null_ has_schema_privilege_name _null_ _null_ n 0));
DESCR("current user privilege on schema by schema name");

/* has_schema_privilege(oid, text) => bool */ 
DATA(insert OID = 2273 ( has_schema_privilege  PGNSP PGUID 12 f f t f s 2 16 f "26 25" _null_ _null_ _null_ has_schema_privilege_id _null_ _null_ n 0));
DESCR("current user privilege on schema by schema oid");

/* has_tablespace_privilege(name, text, text) => bool */ 
DATA(insert OID = 2390 ( has_tablespace_privilege  PGNSP PGUID 12 f f t f s 3 16 f "19 25 25" _null_ _null_ _null_ has_tablespace_privilege_name_name _null_ _null_ n 0));
DESCR("user privilege on tablespace by username, tablespace name");

/* has_tablespace_privilege(name, oid, text) => bool */ 
DATA(insert OID = 2391 ( has_tablespace_privilege  PGNSP PGUID 12 f f t f s 3 16 f "19 26 25" _null_ _null_ _null_ has_tablespace_privilege_name_id _null_ _null_ n 0));
DESCR("user privilege on tablespace by username, tablespace oid");

/* has_tablespace_privilege(oid, text, text) => bool */ 
DATA(insert OID = 2392 ( has_tablespace_privilege  PGNSP PGUID 12 f f t f s 3 16 f "26 25 25" _null_ _null_ _null_ has_tablespace_privilege_id_name _null_ _null_ n 0));
DESCR("user privilege on tablespace by user oid, tablespace name");

/* has_tablespace_privilege(oid, oid, text) => bool */ 
DATA(insert OID = 2393 ( has_tablespace_privilege  PGNSP PGUID 12 f f t f s 3 16 f "26 26 25" _null_ _null_ _null_ has_tablespace_privilege_id_id _null_ _null_ n 0));
DESCR("user privilege on tablespace by user oid, tablespace oid");

/* has_tablespace_privilege(text, text) => bool */ 
DATA(insert OID = 2394 ( has_tablespace_privilege  PGNSP PGUID 12 f f t f s 2 16 f "25 25" _null_ _null_ _null_ has_tablespace_privilege_name _null_ _null_ n 0));
DESCR("current user privilege on tablespace by tablespace name");

/* has_tablespace_privilege(oid, text) => bool */ 
DATA(insert OID = 2395 ( has_tablespace_privilege  PGNSP PGUID 12 f f t f s 2 16 f "26 25" _null_ _null_ _null_ has_tablespace_privilege_id _null_ _null_ n 0));
DESCR("current user privilege on tablespace by tablespace oid");

/* has_foreign_data_wrapper_privilege(name, text, text) => bool */ 
DATA(insert OID = 3112 ( has_foreign_data_wrapper_privilege  PGNSP PGUID 12 f f t f s 3 16 f "19 25 25" _null_ _null_ _null_ has_foreign_data_wrapper_privilege_name_name _null_ _null_ n 0));
DESCR("user privilege on foreign data wrapper by username, foreign data wrapper name");

/* has_foreign_data_wrapper_privilege(name, oid, text) => bool */ 
DATA(insert OID = 3113 ( has_foreign_data_wrapper_privilege  PGNSP PGUID 12 f f t f s 3 16 f "19 26 25" _null_ _null_ _null_ has_foreign_data_wrapper_privilege_name_id _null_ _null_ n 0));
DESCR("user privilege on foreign data wrapper by username, foreign data wrapper oid");

/* has_foreign_data_wrapper_privilege(oid, text, text) => bool */ 
DATA(insert OID = 3114 ( has_foreign_data_wrapper_privilege  PGNSP PGUID 12 f f t f s 3 16 f "26 25 25" _null_ _null_ _null_ has_foreign_data_wrapper_privilege_id_name _null_ _null_ n 0));
DESCR("user privilege on foreign data wrapper by user oid, foreign data wrapper name");

/* has_foreign_data_wrapper_privilege(oid, oid, text) => bool */ 
DATA(insert OID = 3115 ( has_foreign_data_wrapper_privilege  PGNSP PGUID 12 f f t f s 3 16 f "26 26 25" _null_ _null_ _null_ has_foreign_data_wrapper_privilege_id_id _null_ _null_ n 0));
DESCR("user privilege on foreign data wrapper by user oid, foreign data wrapper oid");

/* has_foreign_data_wrapper_privilege(text, text) => bool */ 
DATA(insert OID = 3116 ( has_foreign_data_wrapper_privilege  PGNSP PGUID 12 f f t f s 2 16 f "25 25" _null_ _null_ _null_ has_foreign_data_wrapper_privilege_name _null_ _null_ n 0));
DESCR("current user privilege on foreign data wrapper by foreign data wrapper name");

/* has_foreign_data_wrapper_privilege(oid, text) => bool */ 
DATA(insert OID = 3117 ( has_foreign_data_wrapper_privilege  PGNSP PGUID 12 f f t f s 2 16 f "26 25" _null_ _null_ _null_ has_foreign_data_wrapper_privilege_id _null_ _null_ n 0));
DESCR("current user privilege on foreign data wrapper by foreign data wrapper oid");

/* has_server_privilege(name, text, text) => bool */ 
DATA(insert OID = 3118 ( has_server_privilege  PGNSP PGUID 12 f f t f s 3 16 f "19 25 25" _null_ _null_ _null_ has_server_privilege_name_name _null_ _null_ n 0));
DESCR("user privilege on server by username, server name");

/* has_server_privilege(name, oid, text) => bool */ 
DATA(insert OID = 3119 ( has_server_privilege  PGNSP PGUID 12 f f t f s 3 16 f "19 26 25" _null_ _null_ _null_ has_server_privilege_name_id _null_ _null_ n 0));
DESCR("user privilege on server by username, server oid");

/* has_server_privilege(oid, text, text) => bool */ 
DATA(insert OID = 3120 ( has_server_privilege  PGNSP PGUID 12 f f t f s 3 16 f "26 25 25" _null_ _null_ _null_ has_server_privilege_id_name _null_ _null_ n 0));
DESCR("user privilege on server by user oid, server name");

/* has_server_privilege(oid, oid, text) => bool */ 
DATA(insert OID = 3121 ( has_server_privilege  PGNSP PGUID 12 f f t f s 3 16 f "26 26 25" _null_ _null_ _null_ has_server_privilege_id_id _null_ _null_ n 0));
DESCR("user privilege on server by user oid, server oid");

/* has_server_privilege(text, text) => bool */ 
DATA(insert OID = 3122 ( has_server_privilege  PGNSP PGUID 12 f f t f s 2 16 f "25 25" _null_ _null_ _null_ has_server_privilege_name _null_ _null_ n 0));
DESCR("current user privilege on server by server name");

/* has_server_privilege(oid, text) => bool */ 
DATA(insert OID = 3123 ( has_server_privilege  PGNSP PGUID 12 f f t f s 2 16 f "26 25" _null_ _null_ _null_ has_server_privilege_id _null_ _null_ n 0));
DESCR("current user privilege on server by server oid");

/* pg_has_role(name, name, text) => bool */ 
DATA(insert OID = 2705 ( pg_has_role  PGNSP PGUID 12 f f t f s 3 16 f "19 19 25" _null_ _null_ _null_ pg_has_role_name_name _null_ _null_ n 0));
DESCR("user privilege on role by username, role name");

/* pg_has_role(name, oid, text) => bool */ 
DATA(insert OID = 2706 ( pg_has_role  PGNSP PGUID 12 f f t f s 3 16 f "19 26 25" _null_ _null_ _null_ pg_has_role_name_id _null_ _null_ n 0));
DESCR("user privilege on role by username, role oid");

/* pg_has_role(oid, name, text) => bool */ 
DATA(insert OID = 2707 ( pg_has_role  PGNSP PGUID 12 f f t f s 3 16 f "26 19 25" _null_ _null_ _null_ pg_has_role_id_name _null_ _null_ n 0));
DESCR("user privilege on role by user oid, role name");

/* pg_has_role(oid, oid, text) => bool */ 
DATA(insert OID = 2708 ( pg_has_role  PGNSP PGUID 12 f f t f s 3 16 f "26 26 25" _null_ _null_ _null_ pg_has_role_id_id _null_ _null_ n 0));
DESCR("user privilege on role by user oid, role oid");

/* pg_has_role(name, text) => bool */ 
DATA(insert OID = 2709 ( pg_has_role  PGNSP PGUID 12 f f t f s 2 16 f "19 25" _null_ _null_ _null_ pg_has_role_name _null_ _null_ n 0));
DESCR("current user privilege on role by role name");

/* pg_has_role(oid, text) => bool */ 
DATA(insert OID = 2710 ( pg_has_role  PGNSP PGUID 12 f f t f s 2 16 f "26 25" _null_ _null_ _null_ pg_has_role_id _null_ _null_ n 0));
DESCR("current user privilege on role by role oid");

/* pg_column_size("any") => int4 */ 
DATA(insert OID = 1269 ( pg_column_size  PGNSP PGUID 12 f f t f s 1 23 f "2276" _null_ _null_ _null_ pg_column_size _null_ _null_ n 0));
DESCR("bytes required to store the value, perhaps with compression");

/* pg_tablespace_size(oid) => int8 */ 
DATA(insert OID = 2322 ( pg_tablespace_size  PGNSP PGUID 12 f f t f v 1 20 f "26" _null_ _null_ _null_ pg_tablespace_size_oid _null_ _null_ r 0));
DESCR("Calculate total disk space usage for the specified tablespace");

/* pg_tablespace_size(name) => int8 */ 
DATA(insert OID = 2323 ( pg_tablespace_size  PGNSP PGUID 12 f f t f v 1 20 f "19" _null_ _null_ _null_ pg_tablespace_size_name _null_ _null_ r 0));
DESCR("Calculate total disk space usage for the specified tablespace");

/* pg_database_size(oid) => int8 */ 
DATA(insert OID = 2324 ( pg_database_size  PGNSP PGUID 12 f f t f v 1 20 f "26" _null_ _null_ _null_ pg_database_size_oid _null_ _null_ r 0));
DESCR("Calculate total disk space usage for the specified database");

/* pg_database_size(name) => int8 */ 
DATA(insert OID = 2168 ( pg_database_size  PGNSP PGUID 12 f f t f v 1 20 f "19" _null_ _null_ _null_ pg_database_size_name _null_ _null_ r 0));
DESCR("Calculate total disk space usage for the specified database");

/* pg_relation_size(oid) => int8 */ 
DATA(insert OID = 2325 ( pg_relation_size  PGNSP PGUID 12 f f t f v 1 20 f "26" _null_ _null_ _null_ pg_relation_size_oid _null_ _null_ r 0));
DESCR("Calculate disk space usage for the specified table or index");

/* pg_relation_size(text) => int8 */ 
DATA(insert OID = 2289 ( pg_relation_size  PGNSP PGUID 12 f f t f v 1 20 f "25" _null_ _null_ _null_ pg_relation_size_name _null_ _null_ r 0));
DESCR("Calculate disk space usage for the specified table or index");

/* pg_total_relation_size(oid) => int8 */ 
DATA(insert OID = 2286 ( pg_total_relation_size  PGNSP PGUID 12 f f t f v 1 20 f "26" _null_ _null_ _null_ pg_total_relation_size_oid _null_ _null_ r 0));
DESCR("Calculate total disk space usage for the specified table and associated indexes and toast tables");

/* pg_total_relation_size(text) => int8 */ 
DATA(insert OID = 2287 ( pg_total_relation_size  PGNSP PGUID 12 f f t f v 1 20 f "25" _null_ _null_ _null_ pg_total_relation_size_name _null_ _null_ r 0));
DESCR("Calculate total disk space usage for the specified table and associated indexes and toast tables");

/* pg_size_pretty(int8) => text */ 
DATA(insert OID = 2288 ( pg_size_pretty  PGNSP PGUID 12 f f t f v 1 25 f "20" _null_ _null_ _null_ pg_size_pretty _null_ _null_ r 0));
DESCR("Convert a long int to a human readable text using size units");

/* gpdb_fdw_validator(_text, oid) => bool */ 
DATA(insert OID = 2897 ( gpdb_fdw_validator  PGNSP PGUID 12 f f t f i 2 16 f "1009 26" _null_ _null_ _null_ gpdb_fdw_validator _null_ _null_ n 0));

/* record_in(cstring, oid, int4) => record */ 
DATA(insert OID = 2290 ( record_in  PGNSP PGUID 12 f f t f v 3 2249 f "2275 26 23" _null_ _null_ _null_ record_in _null_ _null_ n 0));
DESCR("I/O");

/* record_out(record) => cstring */ 
DATA(insert OID = 2291 ( record_out  PGNSP PGUID 12 f f t f v 1 2275 f "2249" _null_ _null_ _null_ record_out _null_ _null_ n 0));
DESCR("I/O");

/* cstring_in(cstring) => cstring */ 
DATA(insert OID = 2292 ( cstring_in  PGNSP PGUID 12 f f t f i 1 2275 f "2275" _null_ _null_ _null_ cstring_in _null_ _null_ n 0));
DESCR("I/O");

/* cstring_out(cstring) => cstring */ 
DATA(insert OID = 2293 ( cstring_out  PGNSP PGUID 12 f f t f i 1 2275 f "2275" _null_ _null_ _null_ cstring_out _null_ _null_ n 0));
DESCR("I/O");

/* any_in(cstring) => "any" */ 
DATA(insert OID = 2294 ( any_in  PGNSP PGUID 12 f f t f i 1 2276 f "2275" _null_ _null_ _null_ any_in _null_ _null_ n 0));
DESCR("I/O");

/* any_out("any") => cstring */ 
DATA(insert OID = 2295 ( any_out  PGNSP PGUID 12 f f t f i 1 2275 f "2276" _null_ _null_ _null_ any_out _null_ _null_ n 0));
DESCR("I/O");

/* anyarray_in(cstring) => anyarray */ 
DATA(insert OID = 2296 ( anyarray_in  PGNSP PGUID 12 f f t f i 1 2277 f "2275" _null_ _null_ _null_ anyarray_in _null_ _null_ n 0));
DESCR("I/O");

/* anyarray_out(anyarray) => cstring */ 
DATA(insert OID = 2297 ( anyarray_out  PGNSP PGUID 12 f f t f s 1 2275 f "2277" _null_ _null_ _null_ anyarray_out _null_ _null_ n 0));
DESCR("I/O");

/* void_in(cstring) => void */ 
DATA(insert OID = 2298 ( void_in  PGNSP PGUID 12 f f t f i 1 2278 f "2275" _null_ _null_ _null_ void_in _null_ _null_ n 0));
DESCR("I/O");

/* void_out(void) => cstring */ 
DATA(insert OID = 2299 ( void_out  PGNSP PGUID 12 f f t f i 1 2275 f "2278" _null_ _null_ _null_ void_out _null_ _null_ n 0));
DESCR("I/O");

/* trigger_in(cstring) => trigger */ 
DATA(insert OID = 2300 ( trigger_in  PGNSP PGUID 12 f f t f i 1 2279 f "2275" _null_ _null_ _null_ trigger_in _null_ _null_ n 0));
DESCR("I/O");

/* trigger_out(trigger) => cstring */ 
DATA(insert OID = 2301 ( trigger_out  PGNSP PGUID 12 f f t f i 1 2275 f "2279" _null_ _null_ _null_ trigger_out _null_ _null_ n 0));
DESCR("I/O");

/* language_handler_in(cstring) => language_handler */ 
DATA(insert OID = 2302 ( language_handler_in  PGNSP PGUID 12 f f t f i 1 2280 f "2275" _null_ _null_ _null_ language_handler_in _null_ _null_ n 0));
DESCR("I/O");

/* language_handler_out(language_handler) => cstring */ 
DATA(insert OID = 2303 ( language_handler_out  PGNSP PGUID 12 f f t f i 1 2275 f "2280" _null_ _null_ _null_ language_handler_out _null_ _null_ n 0));
DESCR("I/O");

/* internal_in(cstring) => internal */ 
DATA(insert OID = 2304 ( internal_in  PGNSP PGUID 12 f f t f i 1 2281 f "2275" _null_ _null_ _null_ internal_in _null_ _null_ n 0));
DESCR("I/O");

/* internal_out(internal) => cstring */ 
DATA(insert OID = 2305 ( internal_out  PGNSP PGUID 12 f f t f i 1 2275 f "2281" _null_ _null_ _null_ internal_out _null_ _null_ n 0));
DESCR("I/O");

/* opaque_in(cstring) => opaque */ 
DATA(insert OID = 2306 ( opaque_in  PGNSP PGUID 12 f f t f i 1 2282 f "2275" _null_ _null_ _null_ opaque_in _null_ _null_ n 0));
DESCR("I/O");

/* opaque_out(opaque) => cstring */ 
DATA(insert OID = 2307 ( opaque_out  PGNSP PGUID 12 f f t f i 1 2275 f "2282" _null_ _null_ _null_ opaque_out _null_ _null_ n 0));
DESCR("I/O");

/* anyelement_in(cstring) => anyelement */ 
DATA(insert OID = 2312 ( anyelement_in  PGNSP PGUID 12 f f t f i 1 2283 f "2275" _null_ _null_ _null_ anyelement_in _null_ _null_ n 0));
DESCR("I/O");

/* anyelement_out(anyelement) => cstring */ 
DATA(insert OID = 2313 ( anyelement_out  PGNSP PGUID 12 f f t f i 1 2275 f "2283" _null_ _null_ _null_ anyelement_out _null_ _null_ n 0));
DESCR("I/O");

/* shell_in(cstring) => opaque */ 
DATA(insert OID = 2398 ( shell_in  PGNSP PGUID 12 f f t f i 1 2282 f "2275" _null_ _null_ _null_ shell_in _null_ _null_ n 0));
DESCR("I/O");

/* shell_out(opaque) => cstring */ 
DATA(insert OID = 2399 ( shell_out  PGNSP PGUID 12 f f t f i 1 2275 f "2282" _null_ _null_ _null_ shell_out _null_ _null_ n 0));
DESCR("I/O");

/* domain_in(cstring, oid, int4) => "any" */ 
DATA(insert OID = 2597 ( domain_in  PGNSP PGUID 12 f f f f v 3 2276 f "2275 26 23" _null_ _null_ _null_ domain_in _null_ _null_ n 0));
DESCR("I/O");

/* domain_recv(internal, oid, int4) => "any" */ 
DATA(insert OID = 2598 ( domain_recv  PGNSP PGUID 12 f f f f v 3 2276 f "2281 26 23" _null_ _null_ _null_ domain_recv _null_ _null_ n 0));
DESCR("I/O");


/* cryptographic */
/* md5(text) => text */ 
DATA(insert OID = 2311 ( md5  PGNSP PGUID 12 f f t f i 1 25 f "25" _null_ _null_ _null_ md5_text _null_ _null_ n 0));
DESCR("calculates md5 hash");

/* md5(bytea) => text */ 
DATA(insert OID = 2321 ( md5  PGNSP PGUID 12 f f t f i 1 25 f "17" _null_ _null_ _null_ md5_bytea _null_ _null_ n 0));
DESCR("calculates md5 hash");


/* crosstype operations for date vs. timestamp and timestamptz  */
/* date_lt_timestamp(date, "timestamp") => bool */ 
DATA(insert OID = 2338 ( date_lt_timestamp  PGNSP PGUID 12 f f t f i 2 16 f "1082 1114" _null_ _null_ _null_ date_lt_timestamp _null_ _null_ n 0));
DESCR("less-than");

/* date_le_timestamp(date, "timestamp") => bool */ 
DATA(insert OID = 2339 ( date_le_timestamp  PGNSP PGUID 12 f f t f i 2 16 f "1082 1114" _null_ _null_ _null_ date_le_timestamp _null_ _null_ n 0));
DESCR("less-than-or-equal");

/* date_eq_timestamp(date, "timestamp") => bool */ 
DATA(insert OID = 2340 ( date_eq_timestamp  PGNSP PGUID 12 f f t f i 2 16 f "1082 1114" _null_ _null_ _null_ date_eq_timestamp _null_ _null_ n 0));
DESCR("equal");

/* date_gt_timestamp(date, "timestamp") => bool */ 
DATA(insert OID = 2341 ( date_gt_timestamp  PGNSP PGUID 12 f f t f i 2 16 f "1082 1114" _null_ _null_ _null_ date_gt_timestamp _null_ _null_ n 0));
DESCR("greater-than");

/* date_ge_timestamp(date, "timestamp") => bool */ 
DATA(insert OID = 2342 ( date_ge_timestamp  PGNSP PGUID 12 f f t f i 2 16 f "1082 1114" _null_ _null_ _null_ date_ge_timestamp _null_ _null_ n 0));
DESCR("greater-than-or-equal");

/* date_ne_timestamp(date, "timestamp") => bool */ 
DATA(insert OID = 2343 ( date_ne_timestamp  PGNSP PGUID 12 f f t f i 2 16 f "1082 1114" _null_ _null_ _null_ date_ne_timestamp _null_ _null_ n 0));
DESCR("not equal");

/* date_cmp_timestamp(date, "timestamp") => int4 */ 
DATA(insert OID = 2344 ( date_cmp_timestamp  PGNSP PGUID 12 f f t f i 2 23 f "1082 1114" _null_ _null_ _null_ date_cmp_timestamp _null_ _null_ n 0));
DESCR("less-equal-greater");

/* date_lt_timestamptz(date, timestamptz) => bool */ 
DATA(insert OID = 2351 ( date_lt_timestamptz  PGNSP PGUID 12 f f t f s 2 16 f "1082 1184" _null_ _null_ _null_ date_lt_timestamptz _null_ _null_ n 0));
DESCR("less-than");

/* date_le_timestamptz(date, timestamptz) => bool */ 
DATA(insert OID = 2352 ( date_le_timestamptz  PGNSP PGUID 12 f f t f s 2 16 f "1082 1184" _null_ _null_ _null_ date_le_timestamptz _null_ _null_ n 0));
DESCR("less-than-or-equal");

/* date_eq_timestamptz(date, timestamptz) => bool */ 
DATA(insert OID = 2353 ( date_eq_timestamptz  PGNSP PGUID 12 f f t f s 2 16 f "1082 1184" _null_ _null_ _null_ date_eq_timestamptz _null_ _null_ n 0));
DESCR("equal");

/* date_gt_timestamptz(date, timestamptz) => bool */ 
DATA(insert OID = 2354 ( date_gt_timestamptz  PGNSP PGUID 12 f f t f s 2 16 f "1082 1184" _null_ _null_ _null_ date_gt_timestamptz _null_ _null_ n 0));
DESCR("greater-than");

/* date_ge_timestamptz(date, timestamptz) => bool */ 
DATA(insert OID = 2355 ( date_ge_timestamptz  PGNSP PGUID 12 f f t f s 2 16 f "1082 1184" _null_ _null_ _null_ date_ge_timestamptz _null_ _null_ n 0));
DESCR("greater-than-or-equal");

/* date_ne_timestamptz(date, timestamptz) => bool */ 
DATA(insert OID = 2356 ( date_ne_timestamptz  PGNSP PGUID 12 f f t f s 2 16 f "1082 1184" _null_ _null_ _null_ date_ne_timestamptz _null_ _null_ n 0));
DESCR("not equal");

/* date_cmp_timestamptz(date, timestamptz) => int4 */ 
DATA(insert OID = 2357 ( date_cmp_timestamptz  PGNSP PGUID 12 f f t f s 2 23 f "1082 1184" _null_ _null_ _null_ date_cmp_timestamptz _null_ _null_ n 0));
DESCR("less-equal-greater");

/* timestamp_lt_date("timestamp", date) => bool */ 
DATA(insert OID = 2364 ( timestamp_lt_date  PGNSP PGUID 12 f f t f i 2 16 f "1114 1082" _null_ _null_ _null_ timestamp_lt_date _null_ _null_ n 0));
DESCR("less-than");

/* timestamp_le_date("timestamp", date) => bool */ 
DATA(insert OID = 2365 ( timestamp_le_date  PGNSP PGUID 12 f f t f i 2 16 f "1114 1082" _null_ _null_ _null_ timestamp_le_date _null_ _null_ n 0));
DESCR("less-than-or-equal");

/* timestamp_eq_date("timestamp", date) => bool */ 
DATA(insert OID = 2366 ( timestamp_eq_date  PGNSP PGUID 12 f f t f i 2 16 f "1114 1082" _null_ _null_ _null_ timestamp_eq_date _null_ _null_ n 0));
DESCR("equal");

/* timestamp_gt_date("timestamp", date) => bool */ 
DATA(insert OID = 2367 ( timestamp_gt_date  PGNSP PGUID 12 f f t f i 2 16 f "1114 1082" _null_ _null_ _null_ timestamp_gt_date _null_ _null_ n 0));
DESCR("greater-than");

/* timestamp_ge_date("timestamp", date) => bool */ 
DATA(insert OID = 2368 ( timestamp_ge_date  PGNSP PGUID 12 f f t f i 2 16 f "1114 1082" _null_ _null_ _null_ timestamp_ge_date _null_ _null_ n 0));
DESCR("greater-than-or-equal");

/* timestamp_ne_date("timestamp", date) => bool */ 
DATA(insert OID = 2369 ( timestamp_ne_date  PGNSP PGUID 12 f f t f i 2 16 f "1114 1082" _null_ _null_ _null_ timestamp_ne_date _null_ _null_ n 0));
DESCR("not equal");

/* timestamp_cmp_date("timestamp", date) => int4 */ 
DATA(insert OID = 2370 ( timestamp_cmp_date  PGNSP PGUID 12 f f t f i 2 23 f "1114 1082" _null_ _null_ _null_ timestamp_cmp_date _null_ _null_ n 0));
DESCR("less-equal-greater");

/* timestamptz_lt_date(timestamptz, date) => bool */ 
DATA(insert OID = 2377 ( timestamptz_lt_date  PGNSP PGUID 12 f f t f s 2 16 f "1184 1082" _null_ _null_ _null_ timestamptz_lt_date _null_ _null_ n 0));
DESCR("less-than");

/* timestamptz_le_date(timestamptz, date) => bool */ 
DATA(insert OID = 2378 ( timestamptz_le_date  PGNSP PGUID 12 f f t f s 2 16 f "1184 1082" _null_ _null_ _null_ timestamptz_le_date _null_ _null_ n 0));
DESCR("less-than-or-equal");

/* timestamptz_eq_date(timestamptz, date) => bool */ 
DATA(insert OID = 2379 ( timestamptz_eq_date  PGNSP PGUID 12 f f t f s 2 16 f "1184 1082" _null_ _null_ _null_ timestamptz_eq_date _null_ _null_ n 0));
DESCR("equal");

/* timestamptz_gt_date(timestamptz, date) => bool */ 
DATA(insert OID = 2380 ( timestamptz_gt_date  PGNSP PGUID 12 f f t f s 2 16 f "1184 1082" _null_ _null_ _null_ timestamptz_gt_date _null_ _null_ n 0));
DESCR("greater-than");

/* timestamptz_ge_date(timestamptz, date) => bool */ 
DATA(insert OID = 2381 ( timestamptz_ge_date  PGNSP PGUID 12 f f t f s 2 16 f "1184 1082" _null_ _null_ _null_ timestamptz_ge_date _null_ _null_ n 0));
DESCR("greater-than-or-equal");

/* timestamptz_ne_date(timestamptz, date) => bool */ 
DATA(insert OID = 2382 ( timestamptz_ne_date  PGNSP PGUID 12 f f t f s 2 16 f "1184 1082" _null_ _null_ _null_ timestamptz_ne_date _null_ _null_ n 0));
DESCR("not equal");

/* timestamptz_cmp_date(timestamptz, date) => int4 */ 
DATA(insert OID = 2383 ( timestamptz_cmp_date  PGNSP PGUID 12 f f t f s 2 23 f "1184 1082" _null_ _null_ _null_ timestamptz_cmp_date _null_ _null_ n 0));
DESCR("less-equal-greater");


/* crosstype operations for timestamp vs. timestamptz  */
/* timestamp_lt_timestamptz("timestamp", timestamptz) => bool */ 
DATA(insert OID = 2520 ( timestamp_lt_timestamptz  PGNSP PGUID 12 f f t f s 2 16 f "1114 1184" _null_ _null_ _null_ timestamp_lt_timestamptz _null_ _null_ n 0));
DESCR("less-than");

/* timestamp_le_timestamptz("timestamp", timestamptz) => bool */ 
DATA(insert OID = 2521 ( timestamp_le_timestamptz  PGNSP PGUID 12 f f t f s 2 16 f "1114 1184" _null_ _null_ _null_ timestamp_le_timestamptz _null_ _null_ n 0));
DESCR("less-than-or-equal");

/* timestamp_eq_timestamptz("timestamp", timestamptz) => bool */ 
DATA(insert OID = 2522 ( timestamp_eq_timestamptz  PGNSP PGUID 12 f f t f s 2 16 f "1114 1184" _null_ _null_ _null_ timestamp_eq_timestamptz _null_ _null_ n 0));
DESCR("equal");

/* timestamp_gt_timestamptz("timestamp", timestamptz) => bool */ 
DATA(insert OID = 2523 ( timestamp_gt_timestamptz  PGNSP PGUID 12 f f t f s 2 16 f "1114 1184" _null_ _null_ _null_ timestamp_gt_timestamptz _null_ _null_ n 0));
DESCR("greater-than");

/* timestamp_ge_timestamptz("timestamp", timestamptz) => bool */ 
DATA(insert OID = 2524 ( timestamp_ge_timestamptz  PGNSP PGUID 12 f f t f s 2 16 f "1114 1184" _null_ _null_ _null_ timestamp_ge_timestamptz _null_ _null_ n 0));
DESCR("greater-than-or-equal");

/* timestamp_ne_timestamptz("timestamp", timestamptz) => bool */ 
DATA(insert OID = 2525 ( timestamp_ne_timestamptz  PGNSP PGUID 12 f f t f s 2 16 f "1114 1184" _null_ _null_ _null_ timestamp_ne_timestamptz _null_ _null_ n 0));
DESCR("not equal");

/* timestamp_cmp_timestamptz("timestamp", timestamptz) => int4 */ 
DATA(insert OID = 2526 ( timestamp_cmp_timestamptz  PGNSP PGUID 12 f f t f s 2 23 f "1114 1184" _null_ _null_ _null_ timestamp_cmp_timestamptz _null_ _null_ n 0));
DESCR("less-equal-greater");

/* timestamptz_lt_timestamp(timestamptz, "timestamp") => bool */ 
DATA(insert OID = 2527 ( timestamptz_lt_timestamp  PGNSP PGUID 12 f f t f s 2 16 f "1184 1114" _null_ _null_ _null_ timestamptz_lt_timestamp _null_ _null_ n 0));
DESCR("less-than");

/* timestamptz_le_timestamp(timestamptz, "timestamp") => bool */ 
DATA(insert OID = 2528 ( timestamptz_le_timestamp  PGNSP PGUID 12 f f t f s 2 16 f "1184 1114" _null_ _null_ _null_ timestamptz_le_timestamp _null_ _null_ n 0));
DESCR("less-than-or-equal");

/* timestamptz_eq_timestamp(timestamptz, "timestamp") => bool */ 
DATA(insert OID = 2529 ( timestamptz_eq_timestamp  PGNSP PGUID 12 f f t f s 2 16 f "1184 1114" _null_ _null_ _null_ timestamptz_eq_timestamp _null_ _null_ n 0));
DESCR("equal");

/* timestamptz_gt_timestamp(timestamptz, "timestamp") => bool */ 
DATA(insert OID = 2530 ( timestamptz_gt_timestamp  PGNSP PGUID 12 f f t f s 2 16 f "1184 1114" _null_ _null_ _null_ timestamptz_gt_timestamp _null_ _null_ n 0));
DESCR("greater-than");

/* timestamptz_ge_timestamp(timestamptz, "timestamp") => bool */ 
DATA(insert OID = 2531 ( timestamptz_ge_timestamp  PGNSP PGUID 12 f f t f s 2 16 f "1184 1114" _null_ _null_ _null_ timestamptz_ge_timestamp _null_ _null_ n 0));
DESCR("greater-than-or-equal");

/* timestamptz_ne_timestamp(timestamptz, "timestamp") => bool */ 
DATA(insert OID = 2532 ( timestamptz_ne_timestamp  PGNSP PGUID 12 f f t f s 2 16 f "1184 1114" _null_ _null_ _null_ timestamptz_ne_timestamp _null_ _null_ n 0));
DESCR("not equal");

/* timestamptz_cmp_timestamp(timestamptz, "timestamp") => int4 */ 
DATA(insert OID = 2533 ( timestamptz_cmp_timestamp  PGNSP PGUID 12 f f t f s 2 23 f "1184 1114" _null_ _null_ _null_ timestamptz_cmp_timestamp _null_ _null_ n 0));
DESCR("less-equal-greater");


/* send/receive functions  */
/* array_recv(internal, oid, int4) => anyarray */ 
DATA(insert OID = 2400 ( array_recv  PGNSP PGUID 12 f f t f s 3 2277 f "2281 26 23" _null_ _null_ _null_ array_recv _null_ _null_ n 0));
DESCR("I/O");

/* array_send(anyarray) => bytea */ 
DATA(insert OID = 2401 ( array_send  PGNSP PGUID 12 f f t f s 1 17 f "2277" _null_ _null_ _null_ array_send _null_ _null_ n 0));
DESCR("I/O");

/* record_recv(internal, oid, int4) => record */ 
DATA(insert OID = 2402 ( record_recv  PGNSP PGUID 12 f f t f v 3 2249 f "2281 26 23" _null_ _null_ _null_ record_recv _null_ _null_ n 0));
DESCR("I/O");

/* record_send(record) => bytea */ 
DATA(insert OID = 2403 ( record_send  PGNSP PGUID 12 f f t f v 1 17 f "2249" _null_ _null_ _null_ record_send _null_ _null_ n 0));
DESCR("I/O");

/* int2recv(internal) => int2 */ 
DATA(insert OID = 2404 ( int2recv  PGNSP PGUID 12 f f t f i 1 21 f "2281" _null_ _null_ _null_ int2recv _null_ _null_ n 0));
DESCR("I/O");

/* int2send(int2) => bytea */ 
DATA(insert OID = 2405 ( int2send  PGNSP PGUID 12 f f t f i 1 17 f "21" _null_ _null_ _null_ int2send _null_ _null_ n 0));
DESCR("I/O");

/* int4recv(internal) => int4 */ 
DATA(insert OID = 2406 ( int4recv  PGNSP PGUID 12 f f t f i 1 23 f "2281" _null_ _null_ _null_ int4recv _null_ _null_ n 0));
DESCR("I/O");

/* int4send(int4) => bytea */ 
DATA(insert OID = 2407 ( int4send  PGNSP PGUID 12 f f t f i 1 17 f "23" _null_ _null_ _null_ int4send _null_ _null_ n 0));
DESCR("I/O");

/* int8recv(internal) => int8 */ 
DATA(insert OID = 2408 ( int8recv  PGNSP PGUID 12 f f t f i 1 20 f "2281" _null_ _null_ _null_ int8recv _null_ _null_ n 0));
DESCR("I/O");

/* int8send(int8) => bytea */ 
DATA(insert OID = 2409 ( int8send  PGNSP PGUID 12 f f t f i 1 17 f "20" _null_ _null_ _null_ int8send _null_ _null_ n 0));
DESCR("I/O");

/* int2vectorrecv(internal) => int2vector */ 
DATA(insert OID = 2410 ( int2vectorrecv  PGNSP PGUID 12 f f t f i 1 22 f "2281" _null_ _null_ _null_ int2vectorrecv _null_ _null_ n 0));
DESCR("I/O");

/* int2vectorsend(int2vector) => bytea */ 
DATA(insert OID = 2411 ( int2vectorsend  PGNSP PGUID 12 f f t f i 1 17 f "22" _null_ _null_ _null_ int2vectorsend _null_ _null_ n 0));
DESCR("I/O");

/* bytearecv(internal) => bytea */ 
DATA(insert OID = 2412 ( bytearecv  PGNSP PGUID 12 f f t f i 1 17 f "2281" _null_ _null_ _null_ bytearecv _null_ _null_ n 0));
DESCR("I/O");

/* byteasend(bytea) => bytea */ 
DATA(insert OID = 2413 ( byteasend  PGNSP PGUID 12 f f t f i 1 17 f "17" _null_ _null_ _null_ byteasend _null_ _null_ n 0));
DESCR("I/O");

/* textrecv(internal) => text */ 
DATA(insert OID = 2414 ( textrecv  PGNSP PGUID 12 f f t f s 1 25 f "2281" _null_ _null_ _null_ textrecv _null_ _null_ n 0));
DESCR("I/O");

/* textsend(text) => bytea */ 
DATA(insert OID = 2415 ( textsend  PGNSP PGUID 12 f f t f s 1 17 f "25" _null_ _null_ _null_ textsend _null_ _null_ n 0));
DESCR("I/O");

/* unknownrecv(internal) => unknown */ 
DATA(insert OID = 2416 ( unknownrecv  PGNSP PGUID 12 f f t f i 1 705 f "2281" _null_ _null_ _null_ unknownrecv _null_ _null_ n 0));
DESCR("I/O");

/* unknownsend(unknown) => bytea */ 
DATA(insert OID = 2417 ( unknownsend  PGNSP PGUID 12 f f t f i 1 17 f "705" _null_ _null_ _null_ unknownsend _null_ _null_ n 0));
DESCR("I/O");

/* oidrecv(internal) => oid */ 
DATA(insert OID = 2418 ( oidrecv  PGNSP PGUID 12 f f t f i 1 26 f "2281" _null_ _null_ _null_ oidrecv _null_ _null_ n 0));
DESCR("I/O");

/* oidsend(oid) => bytea */ 
DATA(insert OID = 2419 ( oidsend  PGNSP PGUID 12 f f t f i 1 17 f "26" _null_ _null_ _null_ oidsend _null_ _null_ n 0));
DESCR("I/O");

/* oidvectorrecv(internal) => oidvector */ 
DATA(insert OID = 2420 ( oidvectorrecv  PGNSP PGUID 12 f f t f i 1 30 f "2281" _null_ _null_ _null_ oidvectorrecv _null_ _null_ n 0));
DESCR("I/O");

/* oidvectorsend(oidvector) => bytea */ 
DATA(insert OID = 2421 ( oidvectorsend  PGNSP PGUID 12 f f t f i 1 17 f "30" _null_ _null_ _null_ oidvectorsend _null_ _null_ n 0));
DESCR("I/O");

/* namerecv(internal) => name */ 
DATA(insert OID = 2422 ( namerecv  PGNSP PGUID 12 f f t f s 1 19 f "2281" _null_ _null_ _null_ namerecv _null_ _null_ n 0));
DESCR("I/O");

/* namesend(name) => bytea */ 
DATA(insert OID = 2423 ( namesend  PGNSP PGUID 12 f f t f s 1 17 f "19" _null_ _null_ _null_ namesend _null_ _null_ n 0));
DESCR("I/O");

/* float4recv(internal) => float4 */ 
DATA(insert OID = 2424 ( float4recv  PGNSP PGUID 12 f f t f i 1 700 f "2281" _null_ _null_ _null_ float4recv _null_ _null_ n 0));
DESCR("I/O");

/* float4send(float4) => bytea */ 
DATA(insert OID = 2425 ( float4send  PGNSP PGUID 12 f f t f i 1 17 f "700" _null_ _null_ _null_ float4send _null_ _null_ n 0));
DESCR("I/O");

/* float8recv(internal) => float8 */ 
DATA(insert OID = 2426 ( float8recv  PGNSP PGUID 12 f f t f i 1 701 f "2281" _null_ _null_ _null_ float8recv _null_ _null_ n 0));
DESCR("I/O");

/* float8send(float8) => bytea */ 
DATA(insert OID = 2427 ( float8send  PGNSP PGUID 12 f f t f i 1 17 f "701" _null_ _null_ _null_ float8send _null_ _null_ n 0));
DESCR("I/O");

/* point_recv(internal) => point */ 
DATA(insert OID = 2428 ( point_recv  PGNSP PGUID 12 f f t f i 1 600 f "2281" _null_ _null_ _null_ point_recv _null_ _null_ n 0));
DESCR("I/O");

/* point_send(point) => bytea */ 
DATA(insert OID = 2429 ( point_send  PGNSP PGUID 12 f f t f i 1 17 f "600" _null_ _null_ _null_ point_send _null_ _null_ n 0));
DESCR("I/O");

/* bpcharrecv(internal, oid, int4) => bpchar */ 
DATA(insert OID = 2430 ( bpcharrecv  PGNSP PGUID 12 f f t f s 3 1042 f "2281 26 23" _null_ _null_ _null_ bpcharrecv _null_ _null_ n 0));
DESCR("I/O");

/* bpcharsend(bpchar) => bytea */ 
DATA(insert OID = 2431 ( bpcharsend  PGNSP PGUID 12 f f t f s 1 17 f "1042" _null_ _null_ _null_ bpcharsend _null_ _null_ n 0));
DESCR("I/O");

/* varcharrecv(internal, oid, int4) => "varchar" */ 
DATA(insert OID = 2432 ( varcharrecv  PGNSP PGUID 12 f f t f s 3 1043 f "2281 26 23" _null_ _null_ _null_ varcharrecv _null_ _null_ n 0));
DESCR("I/O");

/* varcharsend("varchar") => bytea */ 
DATA(insert OID = 2433 ( varcharsend  PGNSP PGUID 12 f f t f s 1 17 f "1043" _null_ _null_ _null_ varcharsend _null_ _null_ n 0));
DESCR("I/O");

/* charrecv(internal) => "char" */ 
DATA(insert OID = 2434 ( charrecv  PGNSP PGUID 12 f f t f i 1 18 f "2281" _null_ _null_ _null_ charrecv _null_ _null_ n 0));
DESCR("I/O");

/* charsend("char") => bytea */ 
DATA(insert OID = 2435 ( charsend  PGNSP PGUID 12 f f t f i 1 17 f "18" _null_ _null_ _null_ charsend _null_ _null_ n 0));
DESCR("I/O");

/* boolrecv(internal) => bool */ 
DATA(insert OID = 2436 ( boolrecv  PGNSP PGUID 12 f f t f i 1 16 f "2281" _null_ _null_ _null_ boolrecv _null_ _null_ n 0));
DESCR("I/O");

/* boolsend(bool) => bytea */ 
DATA(insert OID = 2437 ( boolsend  PGNSP PGUID 12 f f t f i 1 17 f "16" _null_ _null_ _null_ boolsend _null_ _null_ n 0));
DESCR("I/O");

/* tidrecv(internal) => tid */ 
DATA(insert OID = 2438 ( tidrecv  PGNSP PGUID 12 f f t f i 1 27 f "2281" _null_ _null_ _null_ tidrecv _null_ _null_ n 0));
DESCR("I/O");

/* tidsend(tid) => bytea */ 
DATA(insert OID = 2439 ( tidsend  PGNSP PGUID 12 f f t f i 1 17 f "27" _null_ _null_ _null_ tidsend _null_ _null_ n 0));
DESCR("I/O");

/* xidrecv(internal) => xid */ 
DATA(insert OID = 2440 ( xidrecv  PGNSP PGUID 12 f f t f i 1 28 f "2281" _null_ _null_ _null_ xidrecv _null_ _null_ n 0));
DESCR("I/O");

/* xidsend(xid) => bytea */ 
DATA(insert OID = 2441 ( xidsend  PGNSP PGUID 12 f f t f i 1 17 f "28" _null_ _null_ _null_ xidsend _null_ _null_ n 0));
DESCR("I/O");

/* cidrecv(internal) => cid */ 
DATA(insert OID = 2442 ( cidrecv  PGNSP PGUID 12 f f t f i 1 29 f "2281" _null_ _null_ _null_ cidrecv _null_ _null_ n 0));
DESCR("I/O");

/* cidsend(cid) => bytea */ 
DATA(insert OID = 2443 ( cidsend  PGNSP PGUID 12 f f t f i 1 17 f "29" _null_ _null_ _null_ cidsend _null_ _null_ n 0));
DESCR("I/O");

/* regprocrecv(internal) => regproc */ 
DATA(insert OID = 2444 ( regprocrecv  PGNSP PGUID 12 f f t f i 1 24 f "2281" _null_ _null_ _null_ regprocrecv _null_ _null_ n 0));
DESCR("I/O");

/* regprocsend(regproc) => bytea */ 
DATA(insert OID = 2445 ( regprocsend  PGNSP PGUID 12 f f t f i 1 17 f "24" _null_ _null_ _null_ regprocsend _null_ _null_ n 0));
DESCR("I/O");

/* regprocedurerecv(internal) => regprocedure */ 
DATA(insert OID = 2446 ( regprocedurerecv  PGNSP PGUID 12 f f t f i 1 2202 f "2281" _null_ _null_ _null_ regprocedurerecv _null_ _null_ n 0));
DESCR("I/O");

/* regproceduresend(regprocedure) => bytea */ 
DATA(insert OID = 2447 ( regproceduresend  PGNSP PGUID 12 f f t f i 1 17 f "2202" _null_ _null_ _null_ regproceduresend _null_ _null_ n 0));
DESCR("I/O");

/* regoperrecv(internal) => regoper */ 
DATA(insert OID = 2448 ( regoperrecv  PGNSP PGUID 12 f f t f i 1 2203 f "2281" _null_ _null_ _null_ regoperrecv _null_ _null_ n 0));
DESCR("I/O");

/* regopersend(regoper) => bytea */ 
DATA(insert OID = 2449 ( regopersend  PGNSP PGUID 12 f f t f i 1 17 f "2203" _null_ _null_ _null_ regopersend _null_ _null_ n 0));
DESCR("I/O");

/* regoperatorrecv(internal) => regoperator */ 
DATA(insert OID = 2450 ( regoperatorrecv  PGNSP PGUID 12 f f t f i 1 2204 f "2281" _null_ _null_ _null_ regoperatorrecv _null_ _null_ n 0));
DESCR("I/O");

/* regoperatorsend(regoperator) => bytea */ 
DATA(insert OID = 2451 ( regoperatorsend  PGNSP PGUID 12 f f t f i 1 17 f "2204" _null_ _null_ _null_ regoperatorsend _null_ _null_ n 0));
DESCR("I/O");

/* regclassrecv(internal) => regclass */ 
DATA(insert OID = 2452 ( regclassrecv  PGNSP PGUID 12 f f t f i 1 2205 f "2281" _null_ _null_ _null_ regclassrecv _null_ _null_ n 0));
DESCR("I/O");

/* regclasssend(regclass) => bytea */ 
DATA(insert OID = 2453 ( regclasssend  PGNSP PGUID 12 f f t f i 1 17 f "2205" _null_ _null_ _null_ regclasssend _null_ _null_ n 0));
DESCR("I/O");

/* regtyperecv(internal) => regtype */ 
DATA(insert OID = 2454 ( regtyperecv  PGNSP PGUID 12 f f t f i 1 2206 f "2281" _null_ _null_ _null_ regtyperecv _null_ _null_ n 0));
DESCR("I/O");

/* regtypesend(regtype) => bytea */ 
DATA(insert OID = 2455 ( regtypesend  PGNSP PGUID 12 f f t f i 1 17 f "2206" _null_ _null_ _null_ regtypesend _null_ _null_ n 0));
DESCR("I/O");

/* bit_recv(internal, oid, int4) => "bit" */ 
DATA(insert OID = 2456 ( bit_recv  PGNSP PGUID 12 f f t f i 3 1560 f "2281 26 23" _null_ _null_ _null_ bit_recv _null_ _null_ n 0));
DESCR("I/O");

/* bit_send("bit") => bytea */ 
DATA(insert OID = 2457 ( bit_send  PGNSP PGUID 12 f f t f i 1 17 f "1560" _null_ _null_ _null_ bit_send _null_ _null_ n 0));
DESCR("I/O");

/* varbit_recv(internal, oid, int4) => varbit */ 
DATA(insert OID = 2458 ( varbit_recv  PGNSP PGUID 12 f f t f i 3 1562 f "2281 26 23" _null_ _null_ _null_ varbit_recv _null_ _null_ n 0));
DESCR("I/O");

/* varbit_send(varbit) => bytea */ 
DATA(insert OID = 2459 ( varbit_send  PGNSP PGUID 12 f f t f i 1 17 f "1562" _null_ _null_ _null_ varbit_send _null_ _null_ n 0));
DESCR("I/O");

/* numeric_recv(internal, oid, int4) => "numeric" */ 
DATA(insert OID = 2460 ( numeric_recv  PGNSP PGUID 12 f f t f i 3 1700 f "2281 26 23" _null_ _null_ _null_ numeric_recv _null_ _null_ n 0));
DESCR("I/O");

/* numeric_send("numeric") => bytea */ 
DATA(insert OID = 2461 ( numeric_send  PGNSP PGUID 12 f f t f i 1 17 f "1700" _null_ _null_ _null_ numeric_send _null_ _null_ n 0));
DESCR("I/O");

/* abstimerecv(internal) => abstime */ 
DATA(insert OID = 2462 ( abstimerecv  PGNSP PGUID 12 f f t f i 1 702 f "2281" _null_ _null_ _null_ abstimerecv _null_ _null_ n 0));
DESCR("I/O");

/* abstimesend(abstime) => bytea */ 
DATA(insert OID = 2463 ( abstimesend  PGNSP PGUID 12 f f t f i 1 17 f "702" _null_ _null_ _null_ abstimesend _null_ _null_ n 0));
DESCR("I/O");

/* reltimerecv(internal) => reltime */ 
DATA(insert OID = 2464 ( reltimerecv  PGNSP PGUID 12 f f t f i 1 703 f "2281" _null_ _null_ _null_ reltimerecv _null_ _null_ n 0));
DESCR("I/O");

/* reltimesend(reltime) => bytea */ 
DATA(insert OID = 2465 ( reltimesend  PGNSP PGUID 12 f f t f i 1 17 f "703" _null_ _null_ _null_ reltimesend _null_ _null_ n 0));
DESCR("I/O");

/* tintervalrecv(internal) => tinterval */ 
DATA(insert OID = 2466 ( tintervalrecv  PGNSP PGUID 12 f f t f i 1 704 f "2281" _null_ _null_ _null_ tintervalrecv _null_ _null_ n 0));
DESCR("I/O");

/* tintervalsend(tinterval) => bytea */ 
DATA(insert OID = 2467 ( tintervalsend  PGNSP PGUID 12 f f t f i 1 17 f "704" _null_ _null_ _null_ tintervalsend _null_ _null_ n 0));
DESCR("I/O");

/* date_recv(internal) => date */ 
DATA(insert OID = 2468 ( date_recv  PGNSP PGUID 12 f f t f i 1 1082 f "2281" _null_ _null_ _null_ date_recv _null_ _null_ n 0));
DESCR("I/O");

/* date_send(date) => bytea */ 
DATA(insert OID = 2469 ( date_send  PGNSP PGUID 12 f f t f i 1 17 f "1082" _null_ _null_ _null_ date_send _null_ _null_ n 0));
DESCR("I/O");

/* time_recv(internal, oid, int4) => "time" */ 
DATA(insert OID = 2470 ( time_recv  PGNSP PGUID 12 f f t f i 3 1083 f "2281 26 23" _null_ _null_ _null_ time_recv _null_ _null_ n 0));
DESCR("I/O");

/* time_send("time") => bytea */ 
DATA(insert OID = 2471 ( time_send  PGNSP PGUID 12 f f t f i 1 17 f "1083" _null_ _null_ _null_ time_send _null_ _null_ n 0));
DESCR("I/O");

/* timetz_recv(internal, oid, int4) => timetz */ 
DATA(insert OID = 2472 ( timetz_recv  PGNSP PGUID 12 f f t f i 3 1266 f "2281 26 23" _null_ _null_ _null_ timetz_recv _null_ _null_ n 0));
DESCR("I/O");

/* timetz_send(timetz) => bytea */ 
DATA(insert OID = 2473 ( timetz_send  PGNSP PGUID 12 f f t f i 1 17 f "1266" _null_ _null_ _null_ timetz_send _null_ _null_ n 0));
DESCR("I/O");

/* timestamp_recv(internal, oid, int4) => "timestamp" */ 
DATA(insert OID = 2474 ( timestamp_recv  PGNSP PGUID 12 f f t f i 3 1114 f "2281 26 23" _null_ _null_ _null_ timestamp_recv _null_ _null_ n 0));
DESCR("I/O");

/* timestamp_send("timestamp") => bytea */ 
DATA(insert OID = 2475 ( timestamp_send  PGNSP PGUID 12 f f t f i 1 17 f "1114" _null_ _null_ _null_ timestamp_send _null_ _null_ n 0));
DESCR("I/O");

/* timestamptz_recv(internal, oid, int4) => timestamptz */ 
DATA(insert OID = 2476 ( timestamptz_recv  PGNSP PGUID 12 f f t f i 3 1184 f "2281 26 23" _null_ _null_ _null_ timestamptz_recv _null_ _null_ n 0));
DESCR("I/O");

/* timestamptz_send(timestamptz) => bytea */ 
DATA(insert OID = 2477 ( timestamptz_send  PGNSP PGUID 12 f f t f i 1 17 f "1184" _null_ _null_ _null_ timestamptz_send _null_ _null_ n 0));
DESCR("I/O");

/* interval_recv(internal, oid, int4) => "interval" */ 
DATA(insert OID = 2478 ( interval_recv  PGNSP PGUID 12 f f t f i 3 1186 f "2281 26 23" _null_ _null_ _null_ interval_recv _null_ _null_ n 0));
DESCR("I/O");

/* interval_send("interval") => bytea */ 
DATA(insert OID = 2479 ( interval_send  PGNSP PGUID 12 f f t f i 1 17 f "1186" _null_ _null_ _null_ interval_send _null_ _null_ n 0));
DESCR("I/O");

/* lseg_recv(internal) => lseg */ 
DATA(insert OID = 2480 ( lseg_recv  PGNSP PGUID 12 f f t f i 1 601 f "2281" _null_ _null_ _null_ lseg_recv _null_ _null_ n 0));
DESCR("I/O");

/* lseg_send(lseg) => bytea */ 
DATA(insert OID = 2481 ( lseg_send  PGNSP PGUID 12 f f t f i 1 17 f "601" _null_ _null_ _null_ lseg_send _null_ _null_ n 0));
DESCR("I/O");

/* path_recv(internal) => path */ 
DATA(insert OID = 2482 ( path_recv  PGNSP PGUID 12 f f t f i 1 602 f "2281" _null_ _null_ _null_ path_recv _null_ _null_ n 0));
DESCR("I/O");

/* path_send(path) => bytea */ 
DATA(insert OID = 2483 ( path_send  PGNSP PGUID 12 f f t f i 1 17 f "602" _null_ _null_ _null_ path_send _null_ _null_ n 0));
DESCR("I/O");

/* box_recv(internal) => box */ 
DATA(insert OID = 2484 ( box_recv  PGNSP PGUID 12 f f t f i 1 603 f "2281" _null_ _null_ _null_ box_recv _null_ _null_ n 0));
DESCR("I/O");

/* box_send(box) => bytea */ 
DATA(insert OID = 2485 ( box_send  PGNSP PGUID 12 f f t f i 1 17 f "603" _null_ _null_ _null_ box_send _null_ _null_ n 0));
DESCR("I/O");

/* poly_recv(internal) => polygon */ 
DATA(insert OID = 2486 ( poly_recv  PGNSP PGUID 12 f f t f i 1 604 f "2281" _null_ _null_ _null_ poly_recv _null_ _null_ n 0));
DESCR("I/O");

/* poly_send(polygon) => bytea */ 
DATA(insert OID = 2487 ( poly_send  PGNSP PGUID 12 f f t f i 1 17 f "604" _null_ _null_ _null_ poly_send _null_ _null_ n 0));
DESCR("I/O");

/* line_recv(internal) => line */ 
DATA(insert OID = 2488 ( line_recv  PGNSP PGUID 12 f f t f i 1 628 f "2281" _null_ _null_ _null_ line_recv _null_ _null_ n 0));
DESCR("I/O");

/* line_send(line) => bytea */ 
DATA(insert OID = 2489 ( line_send  PGNSP PGUID 12 f f t f i 1 17 f "628" _null_ _null_ _null_ line_send _null_ _null_ n 0));
DESCR("I/O");

/* circle_recv(internal) => circle */ 
DATA(insert OID = 2490 ( circle_recv  PGNSP PGUID 12 f f t f i 1 718 f "2281" _null_ _null_ _null_ circle_recv _null_ _null_ n 0));
DESCR("I/O");

/* circle_send(circle) => bytea */ 
DATA(insert OID = 2491 ( circle_send  PGNSP PGUID 12 f f t f i 1 17 f "718" _null_ _null_ _null_ circle_send _null_ _null_ n 0));
DESCR("I/O");

/* cash_recv(internal) => money */ 
DATA(insert OID = 2492 ( cash_recv  PGNSP PGUID 12 f f t f i 1 790 f "2281" _null_ _null_ _null_ cash_recv _null_ _null_ n 0));
DESCR("I/O");

/* cash_send(money) => bytea */ 
DATA(insert OID = 2493 ( cash_send  PGNSP PGUID 12 f f t f i 1 17 f "790" _null_ _null_ _null_ cash_send _null_ _null_ n 0));
DESCR("I/O");

/* macaddr_recv(internal) => macaddr */ 
DATA(insert OID = 2494 ( macaddr_recv  PGNSP PGUID 12 f f t f i 1 829 f "2281" _null_ _null_ _null_ macaddr_recv _null_ _null_ n 0));
DESCR("I/O");

/* macaddr_send(macaddr) => bytea */ 
DATA(insert OID = 2495 ( macaddr_send  PGNSP PGUID 12 f f t f i 1 17 f "829" _null_ _null_ _null_ macaddr_send _null_ _null_ n 0));
DESCR("I/O");

/* inet_recv(internal) => inet */ 
DATA(insert OID = 2496 ( inet_recv  PGNSP PGUID 12 f f t f i 1 869 f "2281" _null_ _null_ _null_ inet_recv _null_ _null_ n 0));
DESCR("I/O");

/* inet_send(inet) => bytea */ 
DATA(insert OID = 2497 ( inet_send  PGNSP PGUID 12 f f t f i 1 17 f "869" _null_ _null_ _null_ inet_send _null_ _null_ n 0));
DESCR("I/O");

/* cidr_recv(internal) => cidr */ 
DATA(insert OID = 2498 ( cidr_recv  PGNSP PGUID 12 f f t f i 1 650 f "2281" _null_ _null_ _null_ cidr_recv _null_ _null_ n 0));
DESCR("I/O");

/* cidr_send(cidr) => bytea */ 
DATA(insert OID = 2499 ( cidr_send  PGNSP PGUID 12 f f t f i 1 17 f "650" _null_ _null_ _null_ cidr_send _null_ _null_ n 0));
DESCR("I/O");

/* cstring_recv(internal) => cstring */ 
DATA(insert OID = 2500 ( cstring_recv  PGNSP PGUID 12 f f t f s 1 2275 f "2281" _null_ _null_ _null_ cstring_recv _null_ _null_ n 0));
DESCR("I/O");

/* cstring_send(cstring) => bytea */ 
DATA(insert OID = 2501 ( cstring_send  PGNSP PGUID 12 f f t f s 1 17 f "2275" _null_ _null_ _null_ cstring_send _null_ _null_ n 0));
DESCR("I/O");

/* anyarray_recv(internal) => anyarray */ 
DATA(insert OID = 2502 ( anyarray_recv  PGNSP PGUID 12 f f t f s 1 2277 f "2281" _null_ _null_ _null_ anyarray_recv _null_ _null_ n 0));
DESCR("I/O");

/* anyarray_send(anyarray) => bytea */ 
DATA(insert OID = 2503 ( anyarray_send  PGNSP PGUID 12 f f t f s 1 17 f "2277" _null_ _null_ _null_ anyarray_send _null_ _null_ n 0));
DESCR("I/O");


/* System-view support functions with pretty-print option  */
/* pg_get_ruledef(oid, bool) => text */ 
DATA(insert OID = 2504 ( pg_get_ruledef  PGNSP PGUID 12 f f t f s 2 25 f "26 16" _null_ _null_ _null_ pg_get_ruledef_ext _null_ _null_ n 0));
DESCR("source text of a rule with pretty-print option");

/* pg_get_viewdef(text, bool) => text */ 
DATA(insert OID = 2505 ( pg_get_viewdef  PGNSP PGUID 12 f f t f s 2 25 f "25 16" _null_ _null_ _null_ pg_get_viewdef_name_ext _null_ _null_ n 0));
DESCR("select statement of a view with pretty-print option");

/* pg_get_viewdef(oid, bool) => text */ 
DATA(insert OID = 2506 ( pg_get_viewdef  PGNSP PGUID 12 f f t f s 2 25 f "26 16" _null_ _null_ _null_ pg_get_viewdef_ext _null_ _null_ n 0));
DESCR("select statement of a view with pretty-print option");

/* pg_get_indexdef(oid, int4, bool) => text */ 
DATA(insert OID = 2507 ( pg_get_indexdef  PGNSP PGUID 12 f f t f s 3 25 f "26 23 16" _null_ _null_ _null_ pg_get_indexdef_ext _null_ _null_ n 0));
DESCR("index description (full create statement or single expression) with pretty-print option");

/* pg_get_constraintdef(oid, bool) => text */ 
DATA(insert OID = 2508 ( pg_get_constraintdef  PGNSP PGUID 12 f f t f s 2 25 f "26 16" _null_ _null_ _null_ pg_get_constraintdef_ext _null_ _null_ n 0));
DESCR("constraint description with pretty-print option");

/* pg_get_expr(text, oid, bool) => text */ 
DATA(insert OID = 2509 ( pg_get_expr  PGNSP PGUID 12 f f t f s 3 25 f "25 26 16" _null_ _null_ _null_ pg_get_expr_ext _null_ _null_ n 0));
DESCR("deparse an encoded expression with pretty-print option");

/* pg_prepared_statement() => SETOF record */ 
DATA(insert OID = 2510 ( pg_prepared_statement  PGNSP PGUID 12 f f t t s 0 2249 f "" _null_ _null_ _null_ pg_prepared_statement _null_ _null_ n 0));
DESCR("get the prepared statements for this session");

/* pg_cursor() => SETOF record */ 
DATA(insert OID = 2511 ( pg_cursor  PGNSP PGUID 12 f f t t s 0 2249 f "" _null_ _null_ _null_ pg_cursor _null_ _null_ n 0));
DESCR("get the open cursors for this session");

/* pg_timezone_abbrevs(OUT abbrev text, OUT utc_offset "interval", OUT is_dst bool) => SETOF pg_catalog.record */ 
DATA(insert OID = 2599 ( pg_timezone_abbrevs  PGNSP PGUID 12 f f t t s 0 2249 f "" "{25,1186,16}" "{o,o,o}" "{abbrev,utc_offset,is_dst}" pg_timezone_abbrevs _null_ _null_ n 0));
DESCR("get the available time zone abbreviations");

/* pg_timezone_names(OUT name text, OUT abbrev text, OUT utc_offset "interval", OUT is_dst bool) => SETOF pg_catalog.record */ 
DATA(insert OID = 2856 ( pg_timezone_names  PGNSP PGUID 12 f f t t s 0 2249 f "" "{25,25,1186,16}" "{o,o,o,o}" "{name,abbrev,utc_offset,is_dst}" pg_timezone_names _null_ _null_ n 0));
DESCR("get the available time zone names");


/* non-persistent series generator */
/* generate_series(int4, int4, int4) => SETOF int4 */ 
DATA(insert OID = 1066 ( generate_series  PGNSP PGUID 12 f f t t v 3 23 f "23 23 23" _null_ _null_ _null_ generate_series_step_int4 _null_ _null_ n 0));
DESCR("non-persistent series generator");

/* generate_series(int4, int4) => SETOF int4 */ 
DATA(insert OID = 1067 ( generate_series  PGNSP PGUID 12 f f t t v 2 23 f "23 23" _null_ _null_ _null_ generate_series_int4 _null_ _null_ n 0));
DESCR("non-persistent series generator");

/* generate_series(int8, int8, int8) => SETOF int8 */ 
DATA(insert OID = 1068 ( generate_series  PGNSP PGUID 12 f f t t v 3 20 f "20 20 20" _null_ _null_ _null_ generate_series_step_int8 _null_ _null_ n 0));
DESCR("non-persistent series generator");

/* generate_series(int8, int8) => SETOF int8 */ 
DATA(insert OID = 1069 ( generate_series  PGNSP PGUID 12 f f t t v 2 20 f "20 20" _null_ _null_ _null_ generate_series_int8 _null_ _null_ n 0));
DESCR("non-persistent series generator");

/* generate_series("timestamp", "timestamp", "interval") => SETOF "timestamp" */ 
DATA(insert OID = 2916 ( generate_series  PGNSP PGUID 12 f f t t v 3 1114 f "1114 1114 1186" _null_ _null_ _null_ generate_series_timestamp _null_ _null_ n 0));
DESCR("non-persistent series generator");

/* generate_series("timestamptz", "timestamptz", "interval") => SETOF "timestamptz" */ 
DATA(insert OID = 2917 ( generate_series  PGNSP PGUID 12 f f t t v 3 1184 f "1184 1184 1186" _null_ _null_ _null_ generate_series_timestamptz _null_ _null_ n 0));
DESCR("non-persistent series generator");


/* boolean aggregates */
/* booland_statefunc(bool, bool) => bool */ 
DATA(insert OID = 2515 ( booland_statefunc  PGNSP PGUID 12 f f t f i 2 16 f "16 16" _null_ _null_ _null_ booland_statefunc _null_ _null_ n 0));
DESCR("boolean-and aggregate transition function");

/* boolor_statefunc(bool, bool) => bool */ 
DATA(insert OID = 2516 ( boolor_statefunc  PGNSP PGUID 12 f f t f i 2 16 f "16 16" _null_ _null_ _null_ boolor_statefunc _null_ _null_ n 0));
DESCR("boolean-or aggregate transition function");

/* bool_and(bool) => bool */ 
DATA(insert OID = 2517 ( bool_and  PGNSP PGUID 12 t f f f i 1 16 f "16" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));
DESCR("boolean-and aggregate");


/* ANY, SOME? These names conflict with subquery operators. See doc. */
/* bool_or(bool) => bool */ 
DATA(insert OID = 2518 ( bool_or  PGNSP PGUID 12 t f f f i 1 16 f "16" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));
DESCR("boolean-or aggregate");

/* "every"(bool) => bool */ 
DATA(insert OID = 2519 ( every  PGNSP PGUID 12 t f f f i 1 16 f "16" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));
DESCR("boolean-and aggregate");


/* bitwise integer aggregates */
/* bit_and(int2) => int2 */ 
DATA(insert OID = 2236 ( bit_and  PGNSP PGUID 12 t f f f i 1 21 f "21" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));
DESCR("bitwise-and smallint aggregate");

/* bit_or(int2) => int2 */ 
DATA(insert OID = 2237 ( bit_or  PGNSP PGUID 12 t f f f i 1 21 f "21" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));
DESCR("bitwise-or smallint aggregate");

/* bit_and(int4) => int4 */ 
DATA(insert OID = 2238 ( bit_and  PGNSP PGUID 12 t f f f i 1 23 f "23" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));
DESCR("bitwise-and integer aggregate");

/* bit_or(int4) => int4 */ 
DATA(insert OID = 2239 ( bit_or  PGNSP PGUID 12 t f f f i 1 23 f "23" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));
DESCR("bitwise-or integer aggregate");

/* bit_and(int8) => int8 */ 
DATA(insert OID = 2240 ( bit_and  PGNSP PGUID 12 t f f f i 1 20 f "20" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));
DESCR("bitwise-and bigint aggregate");

/* bit_or(int8) => int8 */ 
DATA(insert OID = 2241 ( bit_or  PGNSP PGUID 12 t f f f i 1 20 f "20" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));
DESCR("bitwise-or bigint aggregate");

/* bit_and("bit") => "bit" */ 
DATA(insert OID = 2242 ( bit_and  PGNSP PGUID 12 t f f f i 1 1560 f "1560" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));
DESCR("bitwise-and bit aggregate");

/* bit_or("bit") => "bit" */ 
DATA(insert OID = 2243 ( bit_or  PGNSP PGUID 12 t f f f i 1 1560 f "1560" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));
DESCR("bitwise-or bit aggregate");


/* formerly-missing interval + datetime operators */
/* interval_pl_date("interval", date) => pg_catalog."timestamp" */ 
DATA(insert OID = 2546 ( interval_pl_date  PGNSP PGUID 14 f f t f i 2 1114 f "1186 1082" _null_ _null_ _null_ "select $2 + $1" _null_ _null_ c 0));

/* interval_pl_timetz("interval", timetz) => pg_catalog.timetz */ 
DATA(insert OID = 2547 ( interval_pl_timetz  PGNSP PGUID 14 f f t f i 2 1266 f "1186 1266" _null_ _null_ _null_ "select $2 + $1" _null_ _null_ c 0));

/* interval_pl_timestamp("interval", "timestamp") => pg_catalog."timestamp" */ 
DATA(insert OID = 2548 ( interval_pl_timestamp  PGNSP PGUID 14 f f t f i 2 1114 f "1186 1114" _null_ _null_ _null_ "select $2 + $1" _null_ _null_ c 0));

/* interval_pl_timestamptz("interval", timestamptz) => pg_catalog.timestamptz */ 
DATA(insert OID = 2549 ( interval_pl_timestamptz  PGNSP PGUID 14 f f t f s 2 1184 f "1186 1184" _null_ _null_ _null_ "select $2 + $1" _null_ _null_ c 0));

/* integer_pl_date(int4, date) => pg_catalog.date */ 
DATA(insert OID = 2550 ( integer_pl_date  PGNSP PGUID 14 f f t f i 2 1082 f "23 1082" _null_ _null_ _null_ "select $2 + $1" _null_ _null_ c 0));

/* pg_tablespace_databases(oid) => SETOF oid */ 
DATA(insert OID = 2556 ( pg_tablespace_databases  PGNSP PGUID 12 f f t t s 1 26 f "26" _null_ _null_ _null_ pg_tablespace_databases _null_ _null_ n 0));
DESCR("returns database oids in a tablespace");

/* bool(int4) => bool */ 
DATA(insert OID = 2557 ( bool  PGNSP PGUID 12 f f t f i 1 16 f "23" _null_ _null_ _null_ int4_bool _null_ _null_ n 0));
DESCR("convert int4 to boolean");

/* int4(bool) => int4 */ 
DATA(insert OID = 2558 ( int4  PGNSP PGUID 12 f f t f i 1 23 f "16" _null_ _null_ _null_ bool_int4 _null_ _null_ n 0));
DESCR("convert boolean to int4");

/* lastval() => int8 */ 
DATA(insert OID = 2559 ( lastval  PGNSP PGUID 12 f f t f v 0 20 f "" _null_ _null_ _null_ lastval _null_ _null_ n 0));
DESCR("current value from last used sequence");


/* start time function */
/* pg_postmaster_start_time() => timestamptz */ 
DATA(insert OID = 2560 ( pg_postmaster_start_time  PGNSP PGUID 12 f f t f s 0 1184 f "" _null_ _null_ _null_ pgsql_postmaster_start_time _null_ _null_ n 0));
DESCR("postmaster start time");


/* new functions for Y-direction rtree opclasses  */
/* box_below(box, box) => bool */ 
DATA(insert OID = 2562 ( box_below  PGNSP PGUID 12 f f t f i 2 16 f "603 603" _null_ _null_ _null_ box_below _null_ _null_ n 0));
DESCR("is below");

/* box_overbelow(box, box) => bool */ 
DATA(insert OID = 2563 ( box_overbelow  PGNSP PGUID 12 f f t f i 2 16 f "603 603" _null_ _null_ _null_ box_overbelow _null_ _null_ n 0));
DESCR("overlaps or is below");

/* box_overabove(box, box) => bool */ 
DATA(insert OID = 2564 ( box_overabove  PGNSP PGUID 12 f f t f i 2 16 f "603 603" _null_ _null_ _null_ box_overabove _null_ _null_ n 0));
DESCR("overlaps or is above");

/* box_above(box, box) => bool */ 
DATA(insert OID = 2565 ( box_above  PGNSP PGUID 12 f f t f i 2 16 f "603 603" _null_ _null_ _null_ box_above _null_ _null_ n 0));
DESCR("is above");

/* poly_below(polygon, polygon) => bool */ 
DATA(insert OID = 2566 ( poly_below  PGNSP PGUID 12 f f t f i 2 16 f "604 604" _null_ _null_ _null_ poly_below _null_ _null_ n 0));
DESCR("is below");

/* poly_overbelow(polygon, polygon) => bool */ 
DATA(insert OID = 2567 ( poly_overbelow  PGNSP PGUID 12 f f t f i 2 16 f "604 604" _null_ _null_ _null_ poly_overbelow _null_ _null_ n 0));
DESCR("overlaps or is below");

/* poly_overabove(polygon, polygon) => bool */ 
DATA(insert OID = 2568 ( poly_overabove  PGNSP PGUID 12 f f t f i 2 16 f "604 604" _null_ _null_ _null_ poly_overabove _null_ _null_ n 0));
DESCR("overlaps or is above");

/* poly_above(polygon, polygon) => bool */ 
DATA(insert OID = 2569 ( poly_above  PGNSP PGUID 12 f f t f i 2 16 f "604 604" _null_ _null_ _null_ poly_above _null_ _null_ n 0));
DESCR("is above");

/* circle_overbelow(circle, circle) => bool */ 
DATA(insert OID = 2587 ( circle_overbelow  PGNSP PGUID 12 f f t f i 2 16 f "718 718" _null_ _null_ _null_ circle_overbelow _null_ _null_ n 0));
DESCR("overlaps or is below");

/* circle_overabove(circle, circle) => bool */ 
DATA(insert OID = 2588 ( circle_overabove  PGNSP PGUID 12 f f t f i 2 16 f "718 718" _null_ _null_ _null_ circle_overabove _null_ _null_ n 0));
DESCR("overlaps or is above");


/* support functions for GiST r-tree emulation */
/* gist_box_consistent(internal, box, int4) => bool */ 
DATA(insert OID = 2578 ( gist_box_consistent  PGNSP PGUID 12 f f t f i 3 16 f "2281 603 23" _null_ _null_ _null_ gist_box_consistent _null_ _null_ n 0));
DESCR("GiST support");

/* gist_box_compress(internal) => internal */ 
DATA(insert OID = 2579 ( gist_box_compress  PGNSP PGUID 12 f f t f i 1 2281 f "2281" _null_ _null_ _null_ gist_box_compress _null_ _null_ n 0));
DESCR("GiST support");

/* gist_box_decompress(internal) => internal */ 
DATA(insert OID = 2580 ( gist_box_decompress  PGNSP PGUID 12 f f t f i 1 2281 f "2281" _null_ _null_ _null_ gist_box_decompress _null_ _null_ n 0));
DESCR("GiST support");

/* gist_box_penalty(internal, internal, internal) => internal */ 
DATA(insert OID = 2581 ( gist_box_penalty  PGNSP PGUID 12 f f t f i 3 2281 f "2281 2281 2281" _null_ _null_ _null_ gist_box_penalty _null_ _null_ n 0));
DESCR("GiST support");

/* gist_box_picksplit(internal, internal) => internal */ 
DATA(insert OID = 2582 ( gist_box_picksplit  PGNSP PGUID 12 f f t f i 2 2281 f "2281 2281" _null_ _null_ _null_ gist_box_picksplit _null_ _null_ n 0));
DESCR("GiST support");

/* gist_box_union(internal, internal) => box */ 
DATA(insert OID = 2583 ( gist_box_union  PGNSP PGUID 12 f f t f i 2 603 f "2281 2281" _null_ _null_ _null_ gist_box_union _null_ _null_ n 0));
DESCR("GiST support");

/* gist_box_same(box, box, internal) => internal */ 
DATA(insert OID = 2584 ( gist_box_same  PGNSP PGUID 12 f f t f i 3 2281 f "603 603 2281" _null_ _null_ _null_ gist_box_same _null_ _null_ n 0));
DESCR("GiST support");

/* gist_poly_consistent(internal, polygon, int4) => bool */ 
DATA(insert OID = 2585 ( gist_poly_consistent  PGNSP PGUID 12 f f t f i 3 16 f "2281 604 23" _null_ _null_ _null_ gist_poly_consistent _null_ _null_ n 0));
DESCR("GiST support");

/* gist_poly_compress(internal) => internal */ 
DATA(insert OID = 2586 ( gist_poly_compress  PGNSP PGUID 12 f f t f i 1 2281 f "2281" _null_ _null_ _null_ gist_poly_compress _null_ _null_ n 0));
DESCR("GiST support");

/* gist_circle_consistent(internal, circle, int4) => bool */ 
DATA(insert OID = 2591 ( gist_circle_consistent  PGNSP PGUID 12 f f t f i 3 16 f "2281 718 23" _null_ _null_ _null_ gist_circle_consistent _null_ _null_ n 0));
DESCR("GiST support");

/* gist_circle_compress(internal) => internal */ 
DATA(insert OID = 2592 ( gist_circle_compress  PGNSP PGUID 12 f f t f i 1 2281 f "2281" _null_ _null_ _null_ gist_circle_compress _null_ _null_ n 0));
DESCR("GiST support");


/* GIN */
/* gingettuple(internal, internal) => bool */ 
DATA(insert OID = 2730 ( gingettuple  PGNSP PGUID 12 f f t f v 2 16 f "2281 2281" _null_ _null_ _null_ gingettuple _null_ _null_ n 0));
DESCR("gin(internal)");

/* gingetmulti(internal, internal) => internal */ 
DATA(insert OID = 2731 ( gingetmulti  PGNSP PGUID 12 f f t f v 2 2281 f "2281 2281" _null_ _null_ _null_ gingetmulti _null_ _null_ n 0));
DESCR("gin(internal)");

/* gininsert(internal, internal, internal, internal, internal, internal) => bool */ 
DATA(insert OID = 2732 ( gininsert  PGNSP PGUID 12 f f t f v 6 16 f "2281 2281 2281 2281 2281 2281" _null_ _null_ _null_ gininsert _null_ _null_ n 0));
DESCR("gin(internal)");

/* ginbeginscan(internal, internal, internal) => internal */ 
DATA(insert OID = 2733 ( ginbeginscan  PGNSP PGUID 12 f f t f v 3 2281 f "2281 2281 2281" _null_ _null_ _null_ ginbeginscan _null_ _null_ n 0));
DESCR("gin(internal)");

/* ginrescan(internal, internal) => void */ 
DATA(insert OID = 2734 ( ginrescan  PGNSP PGUID 12 f f t f v 2 2278 f "2281 2281" _null_ _null_ _null_ ginrescan _null_ _null_ n 0));
DESCR("gin(internal)");

/* ginendscan(internal) => void */ 
DATA(insert OID = 2735 ( ginendscan  PGNSP PGUID 12 f f t f v 1 2278 f "2281" _null_ _null_ _null_ ginendscan _null_ _null_ n 0));
DESCR("gin(internal)");

/* ginmarkpos(internal) => void */ 
DATA(insert OID = 2736 ( ginmarkpos  PGNSP PGUID 12 f f t f v 1 2278 f "2281" _null_ _null_ _null_ ginmarkpos _null_ _null_ n 0));
DESCR("gin(internal)");

/* ginrestrpos(internal) => void */ 
DATA(insert OID = 2737 ( ginrestrpos  PGNSP PGUID 12 f f t f v 1 2278 f "2281" _null_ _null_ _null_ ginrestrpos _null_ _null_ n 0));
DESCR("gin(internal)");

/* ginbuild(internal, internal, internal) => internal */ 
DATA(insert OID = 2738 ( ginbuild  PGNSP PGUID 12 f f t f v 3 2281 f "2281 2281 2281" _null_ _null_ _null_ ginbuild _null_ _null_ n 0));
DESCR("gin(internal)");

/* ginbulkdelete(internal, internal, internal, internal) => internal */ 
DATA(insert OID = 2739 ( ginbulkdelete  PGNSP PGUID 12 f f t f v 4 2281 f "2281 2281 2281 2281" _null_ _null_ _null_ ginbulkdelete _null_ _null_ n 0));
DESCR("gin(internal)");

/* ginvacuumcleanup(internal, internal) => internal */ 
DATA(insert OID = 2740 ( ginvacuumcleanup  PGNSP PGUID 12 f f t f v 2 2281 f "2281 2281" _null_ _null_ _null_ ginvacuumcleanup _null_ _null_ n 0));
DESCR("gin(internal)");

/* gincostestimate(internal, internal, internal, internal, internal, internal, internal, internal) => void */ 
DATA(insert OID = 2741 ( gincostestimate  PGNSP PGUID 12 f f t f v 8 2278 f "2281 2281 2281 2281 2281 2281 2281 2281" _null_ _null_ _null_ gincostestimate _null_ _null_ n 0));
DESCR("gin(internal)");

/* ginoptions(_text, bool) => bytea */ 
DATA(insert OID = 2788 ( ginoptions  PGNSP PGUID 12 f f t f s 2 17 f "1009 16" _null_ _null_ _null_ ginoptions _null_ _null_ n 0));
DESCR("gin(internal)");


/* Greenplum Analytic functions */
/* matrix_transpose(anyarray) => anyarray */ 
DATA(insert OID = 3200 ( matrix_transpose  PGNSP PGUID 12 f f t f i 1 2277 f "2277" _null_ _null_ _null_ matrix_transpose _null_ _null_ n 0));
DESCR("transpose a two dimensional matrix");

/* matrix_multiply(_int2, _int2) => _int8 */ 
DATA(insert OID = 3201 ( matrix_multiply  PGNSP PGUID 12 f f t f i 2 1016 f "1005 1005" _null_ _null_ _null_ matrix_multiply _null_ _null_ n 0));
DESCR("perform matrix multiplication on two matrices");

/* matrix_multiply(_int4, _int4) => _int8 */ 
DATA(insert OID = 3202 ( matrix_multiply  PGNSP PGUID 12 f f t f i 2 1016 f "1007 1007" _null_ _null_ _null_ matrix_multiply _null_ _null_ n 0));
DESCR("perform matrix multiplication on two matrices");

/* matrix_multiply(_int8, _int8) => _int8 */ 
DATA(insert OID = 3203 ( matrix_multiply  PGNSP PGUID 12 f f t f i 2 1016 f "1016 1016" _null_ _null_ _null_ matrix_multiply _null_ _null_ n 0));
DESCR("perform matrix multiplication on two matrices");

/* matrix_multiply(_float8, _float8) => _float8 */ 
DATA(insert OID = 3204 ( matrix_multiply  PGNSP PGUID 12 f f t f i 2 1022 f "1022 1022" _null_ _null_ _null_ matrix_multiply _null_ _null_ n 0));
DESCR("perform matrix multiplication on two matrices");

/* matrix_multiply(_int8, int8) => _int8 */ 
DATA(insert OID = 3205 ( matrix_multiply  PGNSP PGUID 12 f f t f i 2 1016 f "1016 20" _null_ _null_ _null_ int8_matrix_smultiply _null_ _null_ n 0));
DESCR("multiply a matrix by a scalar value");

/* matrix_multiply(_float8, float8) => _float8 */ 
DATA(insert OID = 3206 ( matrix_multiply  PGNSP PGUID 12 f f t f i 2 1022 f "1022 701" _null_ _null_ _null_ float8_matrix_smultiply _null_ _null_ n 0));
DESCR("multiply a matrix by a scalar value");

/* matrix_add(_int2, _int2) => _int2 */ 
DATA(insert OID = 3208 ( matrix_add  PGNSP PGUID 12 f f t f i 2 1005 f "1005 1005" _null_ _null_ _null_ matrix_add _null_ _null_ n 0));
DESCR("perform matrix addition on two conformable matrices");

/* matrix_add(_int4, _int4) => _int4 */ 
DATA(insert OID = 3209 ( matrix_add  PGNSP PGUID 12 f f t f i 2 1007 f "1007 1007" _null_ _null_ _null_ matrix_add _null_ _null_ n 0));
DESCR("perform matrix addition on two conformable matrices");

/* matrix_add(_int8, _int8) => _int8 */ 
DATA(insert OID = 3210 ( matrix_add  PGNSP PGUID 12 f f t f i 2 1016 f "1016 1016" _null_ _null_ _null_ matrix_add _null_ _null_ n 0));
DESCR("perform matrix addition on two conformable matrices");

/* matrix_add(_float8, _float8) => _float8 */ 
DATA(insert OID = 3211 ( matrix_add  PGNSP PGUID 12 f f t f i 2 1022 f "1022 1022" _null_ _null_ _null_ matrix_add _null_ _null_ n 0));
DESCR("perform matrix addition on two conformable matrices");

/* int2_matrix_accum(_int8, _int2) => _int8 */ 
DATA(insert OID = 3212 ( int2_matrix_accum  PGNSP PGUID 12 f f f f i 2 1016 f "1016 1005" _null_ _null_ _null_ matrix_add _null_ _null_ n 0));
DESCR("perform matrix addition on two conformable matrices");

/* int4_matrix_accum(_int8, _int4) => _int8 */ 
DATA(insert OID = 3213 ( int4_matrix_accum  PGNSP PGUID 12 f f f f i 2 1016 f "1016 1007" _null_ _null_ _null_ matrix_add _null_ _null_ n 0));
DESCR("perform matrix addition on two conformable matrices");

/* int8_matrix_accum(_int8, _int8) => _int8 */ 
DATA(insert OID = 3214 ( int8_matrix_accum  PGNSP PGUID 12 f f t f i 2 1016 f "1016 1016" _null_ _null_ _null_ matrix_add _null_ _null_ n 0));
DESCR("perform matrix addition on two conformable matrices");

/* float8_matrix_accum(_float8, _float8) => _float8 */ 
DATA(insert OID = 3215 ( float8_matrix_accum  PGNSP PGUID 12 f f t f i 2 1022 f "1022 1022" _null_ _null_ _null_ matrix_add _null_ _null_ n 0));
DESCR("perform matrix addition on two conformable matrices");

/* sum(_int2) => _int8 */ 
DATA(insert OID = 3216 ( sum  PGNSP PGUID 12 t f f f i 1 1016 f "1005" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* sum(_int4) => _int8 */ 
DATA(insert OID = 3217 ( sum  PGNSP PGUID 12 t f f f i 1 1016 f "1007" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* sum(_int8) => _int8 */ 
DATA(insert OID = 3218 ( sum  PGNSP PGUID 12 t f f f i 1 1016 f "1016" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* sum(_float8) => _float8 */ 
DATA(insert OID = 3219 ( sum  PGNSP PGUID 12 t f f f i 1 1022 f "1022" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));


/* 3220 - reserved for sum(numeric[])  */
/* int4_pivot_accum(_int4, _text, text, int4) => _int4 */ 
DATA(insert OID = 3225 ( int4_pivot_accum  PGNSP PGUID 12 f f f f i 4 1007 f "1007 1009 25 23" _null_ _null_ _null_ int4_pivot_accum _null_ _null_ n 0));

/* pivot_sum(_text, text, int4) => _int4 */ 
DATA(insert OID = 3226 ( pivot_sum  PGNSP PGUID 12 t f f f i 3 1007 f "1009 25 23" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* int8_pivot_accum(_int8, _text, text, int8) => _int8 */ 
DATA(insert OID = 3227 ( int8_pivot_accum  PGNSP PGUID 12 f f f f i 4 1016 f "1016 1009 25 20" _null_ _null_ _null_ int8_pivot_accum _null_ _null_ n 0));

/* pivot_sum(_text, text, int8) => _int8 */ 
DATA(insert OID = 3228 ( pivot_sum  PGNSP PGUID 12 t f f f i 3 1016 f "1009 25 20" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* float8_pivot_accum(_float8, _text, text, float8) => _float8 */ 
DATA(insert OID = 3229 ( float8_pivot_accum  PGNSP PGUID 12 f f f f i 4 1022 f "1022 1009 25 701" _null_ _null_ _null_ float8_pivot_accum _null_ _null_ n 0));

/* pivot_sum(_text, text, float8) => _float8 */ 
DATA(insert OID = 3230 ( pivot_sum  PGNSP PGUID 12 t f f f i 3 1022 f "1009 25 701" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* unnest(anyarray) => SETOF anyelement */ 
DATA(insert OID = 3240 ( unnest  PGNSP PGUID 12 f f t t i 1 2283 f "2277" _null_ _null_ _null_ unnest _null_ _null_ n 0));


/* 3241-324? reserved for unpivot, see pivot.c  */

/* 3250 = nb_classification class  */
/* 3251 = nb_classification type  */
/* nb_classify_accum(nb_classification, _text, int8, _int8, _int8) => nb_classification */ 
DATA(insert OID = 3252 ( nb_classify_accum  PGNSP PGUID 12 f f f f i 5 3251 f "3251 1009 20 1016 1016" _null_ _null_ _null_ nb_classify_accum _null_ _null_ n 0));

/* nb_classify_combine(nb_classification, nb_classification) => nb_classification */ 
DATA(insert OID = 3253 ( nb_classify_combine  PGNSP PGUID 12 f f f f i 2 3251 f "3251 3251" _null_ _null_ _null_ nb_classify_combine _null_ _null_ n 0));

/* nb_classify_final(nb_classification) => text */ 
DATA(insert OID = 3254 ( nb_classify_final  PGNSP PGUID 12 f f t f i 1 25 f "3251" _null_ _null_ _null_ nb_classify_final _null_ _null_ n 0));

/* nb_classify(_text, int8, _int8, _int8) => text */ 
DATA(insert OID = 3255 ( nb_classify  PGNSP PGUID 12 t f f f i 4 25 f "1009 20 1016 1016" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* nb_classify_probabilities(nb_classification) => _float8 */ 
DATA(insert OID = 3267 ( nb_classify_probabilities  PGNSP PGUID 12 f f t f i 1 1022 f "3251" _null_ _null_ _null_ nb_classify_probabilities _null_ _null_ n 0));

/* nb_probabilities(_text, int8, _int8, _int8) => _float8 */ 
DATA(insert OID = 3268 ( nb_probabilities  PGNSP PGUID 12 t f f f i 4 1022 f "1009 20 1016 1016" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* pinv(_float8) => _float8 */ 
DATA(insert OID = 3256 ( pinv  PGNSP PGUID 12 f f t f i 1 1022 f "1022" _null_ _null_ _null_ pseudoinverse _null_ _null_ n 0));

/* float8_mregr_accum(_float8, float8, _float8) => _float8 */ 
DATA(insert OID = 3257 ( float8_mregr_accum  PGNSP PGUID 12 f f t f i 3 1022 f "1022 701 1022" _null_ _null_ _null_ float8_mregr_accum _null_ _null_ n 0));

/* float8_mregr_combine(_float8, _float8) => _float8 */ 
DATA(insert OID = 3258 ( float8_mregr_combine  PGNSP PGUID 12 f f t f i 2 1022 f "1022 1022" _null_ _null_ _null_ float8_mregr_combine _null_ _null_ n 0));

/* float8_mregr_coef(_float8) => _float8 */ 
DATA(insert OID = 3259 ( float8_mregr_coef  PGNSP PGUID 12 f f t f i 1 1022 f "1022" _null_ _null_ _null_ float8_mregr_coef _null_ _null_ n 0));

/* float8_mregr_r2(_float8) => float8 */ 
DATA(insert OID = 3260 ( float8_mregr_r2  PGNSP PGUID 12 f f t f i 1 701 f "1022" _null_ _null_ _null_ float8_mregr_r2 _null_ _null_ n 0));

/* mregr_coef(float8, _float8) => _float8 */ 
DATA(insert OID = 3261 ( mregr_coef  PGNSP PGUID 12 t f t f i 2 1022 f "701 1022" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* mregr_r2(float8, _float8) => float8 */ 
DATA(insert OID = 3262 ( mregr_r2  PGNSP PGUID 12 t f t f i 2 701 f "701 1022" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* float8_mregr_tstats(_float8) => _float8 */ 
DATA(insert OID = 3263 ( float8_mregr_tstats  PGNSP PGUID 12 f f t f i 1 1022 f "1022" _null_ _null_ _null_ float8_mregr_tstats _null_ _null_ n 0));

/* mregr_tstats(float8, _float8) => _float8 */ 
DATA(insert OID = 3264 ( mregr_tstats  PGNSP PGUID 12 t f f f i 2 1022 f "701 1022" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* float8_mregr_pvalues(_float8) => _float8 */ 
DATA(insert OID = 3265 ( float8_mregr_pvalues  PGNSP PGUID 12 f f t f i 1 1022 f "1022" _null_ _null_ _null_ float8_mregr_pvalues _null_ _null_ n 0));

/* mregr_pvalues(float8, _float8) => _float8 */ 
DATA(insert OID = 3266 ( mregr_pvalues  PGNSP PGUID 12 t f f f i 2 1022 f "701 1022" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));

/* gpaotidin(cstring) => gpaotid */ 
DATA(insert OID = 3302 ( gpaotidin  PGNSP PGUID 12 f f t f i 1 3300 f "2275" _null_ _null_ _null_ gpaotidin _null_ _null_ n 0));
DESCR("I/O");

/* gpaotidout(gpaotid) => cstring */ 
DATA(insert OID = 3303 ( gpaotidout  PGNSP PGUID 12 f f t f i 1 2275 f "3300" _null_ _null_ _null_ gpaotidout _null_ _null_ n 0));
DESCR("I/O");

/* gpaotidrecv(internal) => gpaotid */ 
DATA(insert OID = 3304 ( gpaotidrecv  PGNSP PGUID 12 f f t f i 1 3300 f "2281" _null_ _null_ _null_ gpaotidrecv _null_ _null_ n 0));
DESCR("I/O");

/* gpaotidsend(gpaotid) => bytea */ 
DATA(insert OID = 3305 ( gpaotidsend  PGNSP PGUID 12 f f t f i 1 17 f "3300" _null_ _null_ _null_ gpaotidsend _null_ _null_ n 0));
DESCR("I/O");

/* gpxloglocin(cstring) => gpxlogloc */ 
DATA(insert OID = 3312 ( gpxloglocin  PGNSP PGUID 12 f f t f i 1 3310 f "2275" _null_ _null_ _null_ gpxloglocin _null_ _null_ n 0));
DESCR("I/O");

/* gpxloglocout(gpxlogloc) => cstring */ 
DATA(insert OID = 3313 ( gpxloglocout  PGNSP PGUID 12 f f t f i 1 2275 f "3310" _null_ _null_ _null_ gpxloglocout _null_ _null_ n 0));
DESCR("I/O");

/* gpxloglocrecv(internal) => gpxlogloc */ 
DATA(insert OID = 3314 ( gpxloglocrecv  PGNSP PGUID 12 f f t f i 1 3310 f "2281" _null_ _null_ _null_ gpxloglocrecv _null_ _null_ n 0));
DESCR("I/O");

/* gpxloglocsend(gpxlogloc) => bytea */ 
DATA(insert OID = 3315 ( gpxloglocsend  PGNSP PGUID 12 f f t f i 1 17 f "3310" _null_ _null_ _null_ gpxloglocsend _null_ _null_ n 0));
DESCR("I/O");

/* gpxlogloclarger(gpxlogloc, gpxlogloc) => gpxlogloc */ 
DATA(insert OID = 3318 ( gpxlogloclarger  PGNSP PGUID 12 f f t f i 2 3310 f "3310 3310" _null_ _null_ _null_ gpxlogloclarger _null_ _null_ n 0));
DESCR("I/O");

/* gpxloglocsmaller(gpxlogloc, gpxlogloc) => gpxlogloc */ 
DATA(insert OID = 3319 ( gpxloglocsmaller  PGNSP PGUID 12 f f t f i 2 3310 f "3310 3310" _null_ _null_ _null_ gpxloglocsmaller _null_ _null_ n 0));
DESCR("I/O");

/* gpxlogloceq(gpxlogloc, gpxlogloc) => bool */ 
DATA(insert OID = 3331 ( gpxlogloceq  PGNSP PGUID 12 f f t f i 2 16 f "3310 3310" _null_ _null_ _null_ gpxlogloceq _null_ _null_ n 0));
DESCR("I/O");

/* gpxloglocne(gpxlogloc, gpxlogloc) => bool */ 
DATA(insert OID = 3320 ( gpxloglocne  PGNSP PGUID 12 f f t f i 2 16 f "3310 3310" _null_ _null_ _null_ gpxloglocne _null_ _null_ n 0));
DESCR("I/O");

/* gpxlogloclt(gpxlogloc, gpxlogloc) => bool */ 
DATA(insert OID = 3321 ( gpxlogloclt  PGNSP PGUID 12 f f t f i 2 16 f "3310 3310" _null_ _null_ _null_ gpxlogloclt _null_ _null_ n 0));
DESCR("I/O");

/* gpxloglocle(gpxlogloc, gpxlogloc) => bool */ 
DATA(insert OID = 3322 ( gpxloglocle  PGNSP PGUID 12 f f t f i 2 16 f "3310 3310" _null_ _null_ _null_ gpxloglocle _null_ _null_ n 0));
DESCR("I/O");

/* gpxloglocgt(gpxlogloc, gpxlogloc) => bool */ 
DATA(insert OID = 3323 ( gpxloglocgt  PGNSP PGUID 12 f f t f i 2 16 f "3310 3310" _null_ _null_ _null_ gpxloglocgt _null_ _null_ n 0));
DESCR("I/O");

/* gpxloglocge(gpxlogloc, gpxlogloc) => bool */ 
DATA(insert OID = 3324 ( gpxloglocge  PGNSP PGUID 12 f f t f i 2 16 f "3310 3310" _null_ _null_ _null_ gpxloglocge _null_ _null_ n 0));
DESCR("I/O");


/* Greenplum MPP exposed internally-defined functions.  */
/* gp_backup_launch(text, text, text, text, text) => text */ 
DATA(insert OID = 6003 ( gp_backup_launch  PGNSP PGUID 12 f f f f v 5 25 f "25 25 25 25 25" _null_ _null_ _null_ gp_backup_launch__ _null_ _null_ n 0));
DESCR("launch mpp backup on outboard Postgres instances");

/* gp_restore_launch(text, text, text, text, text, text, int4, bool) => text */ 
DATA(insert OID = 6004 ( gp_restore_launch  PGNSP PGUID 12 f f f f v 8 25 f "25 25 25 25 25 25 23 16" _null_ _null_ _null_ gp_restore_launch__ _null_ _null_ n 0));
DESCR("launch mpp restore on outboard Postgres instances");

/* gp_read_backup_file(text, text, regproc) => text */ 
DATA(insert OID = 6005 ( gp_read_backup_file  PGNSP PGUID 12 f f f f v 3 25 f "25 25 24" _null_ _null_ _null_ gp_read_backup_file__ _null_ _null_ n 0));
DESCR("read mpp backup file on outboard Postgres instances");

/* gp_write_backup_file(text, text, text) => text */ 
DATA(insert OID = 6006 ( gp_write_backup_file  PGNSP PGUID 12 f f f f v 3 25 f "25 25 25" _null_ _null_ _null_ gp_write_backup_file__ _null_ _null_ n 0));
DESCR("write mpp backup file on outboard Postgres instances");

/* gp_pgdatabase() => SETOF record */ 
DATA(insert OID = 6007 ( gp_pgdatabase  PGNSP PGUID 12 f f f t v 0 2249 f "" _null_ _null_ _null_ gp_pgdatabase__ _null_ _null_ n 0));
DESCR("view mpp pgdatabase state");

/* numeric_amalg(_numeric, _numeric) => _numeric */ 
DATA(insert OID = 6008 ( numeric_amalg  PGNSP PGUID 12 f f t f i 2 1231 f "1231 1231" _null_ _null_ _null_ numeric_amalg _null_ _null_ n 0));
DESCR("aggregate preliminary function");

/* numeric_avg_amalg(bytea, bytea) => bytea */ 
DATA(insert OID = 3104 ( numeric_avg_amalg  PGNSP PGUID 12 f f t f i 2 17 f "17 17" _null_ _null_ _null_ numeric_avg_amalg _null_ _null_ n 0));
DESCR("aggregate preliminary function");

/* int8_avg_amalg(bytea, bytea) => bytea */ 
DATA(insert OID = 6009 ( int8_avg_amalg  PGNSP PGUID 12 f f t f i 2 17 f "17 17" _null_ _null_ _null_ int8_avg_amalg _null_ _null_ n 0));
DESCR("aggregate preliminary function");

/* float8_amalg(_float8, _float8) => _float8 */ 
DATA(insert OID = 6010 ( float8_amalg  PGNSP PGUID 12 f f t f i 2 1022 f "1022 1022" _null_ _null_ _null_ float8_amalg _null_ _null_ n 0));
DESCR("aggregate preliminary function");

/* float8_avg_amalg(bytea, bytea) => bytea */ 
DATA(insert OID = 3111 ( float8_avg_amalg  PGNSP PGUID 12 f f t f i 2 17 f "17 17" _null_ _null_ _null_ float8_avg_amalg _null_ _null_ n 0));
DESCR("aggregate preliminary function");

/* interval_amalg(_interval, _interval) => _interval */ 
DATA(insert OID = 6011 ( interval_amalg  PGNSP PGUID 12 f f t f i 2 1187 f "1187 1187" _null_ _null_ _null_ interval_amalg _null_ _null_ n 0));
DESCR("aggregate preliminary function");

/* numeric_demalg(_numeric, _numeric) => _numeric */ 
DATA(insert OID = 6015 ( numeric_demalg  PGNSP PGUID 12 f f t f i 2 1231 f "1231 1231" _null_ _null_ _null_ numeric_demalg _null_ _null_ n 0));
DESCR("aggregate inverse preliminary function");

/* numeric_avg_demalg(bytea, bytea) => bytea */ 
DATA(insert OID = 3105 ( numeric_avg_demalg  PGNSP PGUID 12 f f t f i 2 17 f "17 17" _null_ _null_ _null_ numeric_avg_demalg _null_ _null_ n 0));
DESCR("aggregate inverse preliminary function");

/* int8_avg_demalg(bytea, bytea) => bytea */ 
DATA(insert OID = 6016 ( int8_avg_demalg  PGNSP PGUID 12 f f t f i 2 17 f "17 17" _null_ _null_ _null_ int8_avg_demalg _null_ _null_ n 0));
DESCR("aggregate preliminary function");

/* float8_demalg(_float8, _float8) => _float8 */ 
DATA(insert OID = 6017 ( float8_demalg  PGNSP PGUID 12 f f t f i 2 1022 f "1022 1022" _null_ _null_ _null_ float8_demalg _null_ _null_ n 0));

/* float8_avg_demalg(bytea, bytea) => bytea */ 
DATA(insert OID = 3110 ( float8_avg_demalg  PGNSP PGUID 12 f f t f i 2 17 f "17 17" _null_ _null_ _null_ float8_avg_demalg _null_ _null_ n 0));
DESCR("aggregate inverse preliminary function");

/* interval_demalg(_interval, _interval) => _interval */ 
DATA(insert OID = 6018 ( interval_demalg  PGNSP PGUID 12 f f t f i 2 1187 f "1187 1187" _null_ _null_ _null_ interval_demalg _null_ _null_ n 0));
DESCR("aggregate preliminary function");

/* float8_regr_amalg(_float8, _float8) => _float8 */ 
DATA(insert OID = 6014 ( float8_regr_amalg  PGNSP PGUID 12 f f t f i 2 1022 f "1022 1022" _null_ _null_ _null_ float8_regr_amalg _null_ _null_ n 0));

/* int8(tid) => int8 */ 
DATA(insert OID = 6021 ( int8  PGNSP PGUID 12 f f t f i 1 20 f "27" _null_ _null_ _null_ tidtoi8 _null_ _null_ n 0));
DESCR("convert tid to int8");


#define CDB_PROC_TIDTOI8    6021
/* gp_execution_segment() => SETOF int4 */ 
DATA(insert OID = 6022 ( gp_execution_segment  PGNSP PGUID 12 f f f t v 0 23 f "" _null_ _null_ _null_ mpp_execution_segment _null_ _null_ n 0));
DESCR("segment executing function");


#define MPP_EXECUTION_SEGMENT_OID 6022
#define MPP_EXECUTION_SEGMENT_TYPE 23
/* pg_highest_oid() => oid */ 
DATA(insert OID = 6023 ( pg_highest_oid  PGNSP PGUID 12 f f t f v 0 26 f "" _null_ _null_ _null_ pg_highest_oid _null_ _null_ r 0));
DESCR("Highest oid used so far");

/* gp_distributed_xacts() => SETOF record */ 
DATA(insert OID = 6035 ( gp_distributed_xacts  PGNSP PGUID 12 f f f t v 0 2249 f "" _null_ _null_ _null_ gp_distributed_xacts__ _null_ _null_ n 0));
DESCR("view mpp distributed transaction state");

/* gp_max_distributed_xid() => xid */ 
DATA(insert OID = 6036 ( gp_max_distributed_xid  PGNSP PGUID 12 f f t f v 0 28 f "" _null_ _null_ _null_ gp_max_distributed_xid _null_ _null_ n 0));
DESCR("Highest distributed transaction id used so far");

/* gp_distributed_xid() => xid */ 
DATA(insert OID = 6037 ( gp_distributed_xid  PGNSP PGUID 12 f f t f v 0 28 f "" _null_ _null_ _null_ gp_distributed_xid _null_ _null_ n 0));
DESCR("Current distributed transaction id");

/* gp_transaction_log() => SETOF record */ 
DATA(insert OID = 6043 ( gp_transaction_log  PGNSP PGUID 12 f f f t v 0 2249 f "" _null_ _null_ _null_ gp_transaction_log _null_ _null_ n 0));
DESCR("view logged local transaction status");

/* gp_distributed_log() => SETOF record */ 
DATA(insert OID = 6044 ( gp_distributed_log  PGNSP PGUID 12 f f f t v 0 2249 f "" _null_ _null_ _null_ gp_distributed_log _null_ _null_ n 0));
DESCR("view logged distributed transaction status");

/* gp_changetracking_log(IN filetype int4, OUT segment_id int2, OUT dbid int2, OUT space oid, OUT db oid, OUT rel oid, OUT xlogloc gpxlogloc, OUT blocknum int4, OUT persistent_tid tid, OUT persistent_sn int8) => SETOF pg_catalog.record */ 
DATA(insert OID = 6435 ( gp_changetracking_log  PGNSP PGUID 12 f f f t v 1 2249 f "23" "{23,21,21,26,26,26,3310,23,27,20}" "{i,o,o,o,o,o,o,o,o,o}" "{filetype,segment_id,dbid,space,db,rel,xlogloc,blocknum,persistent_tid,persistent_sn}" gp_changetracking_log _null_ _null_ n 0));
DESCR("view logged change tracking records");

/* gp_execution_dbid() => int4 */ 
DATA(insert OID = 6068 ( gp_execution_dbid  PGNSP PGUID 12 f f f f v 0 23 f "" _null_ _null_ _null_ gp_execution_dbid _null_ _null_ n 0));
DESCR("dbid executing function");


/* Greenplum MPP window function implementation */
/* row_number_immed(internal) => int8 */ 
DATA(insert OID = 7100 ( row_number_immed  PGNSP PGUID 12 f f t f i 1 20 f "2281" _null_ _null_ _null_ row_number_immed _null_ _null_ n 0));
DESCR("window immediate function");

/* rank_immed(internal) => int8 */ 
DATA(insert OID = 7101 ( rank_immed  PGNSP PGUID 12 f f t f i 1 20 f "2281" _null_ _null_ _null_ rank_immed _null_ _null_ n 0));
DESCR("window immediate function");

/* dense_rank_immed(internal) => int8 */ 
DATA(insert OID = 7102 ( dense_rank_immed  PGNSP PGUID 12 f f t f i 1 20 f "2281" _null_ _null_ _null_ dense_rank_immed _null_ _null_ n 0));
DESCR("window immediate function");

/* lag_bool(internal, bool, int8, bool) => bool */ 
DATA(insert OID = 7490 ( lag_bool  PGNSP PGUID 12 f f t f i 4 16 t "2281 16 20 16" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_bool(internal, bool, int8) => bool */ 
DATA(insert OID = 7492 ( lag_bool  PGNSP PGUID 12 f f t f i 3 16 t "2281 16 20" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_bool(internal, bool) => bool */ 
DATA(insert OID = 7494 ( lag_bool  PGNSP PGUID 12 f f t f i 2 16 t "2281 16" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_char(internal, "char", int8, "char") => "char" */ 
DATA(insert OID = 7496 ( lag_char  PGNSP PGUID 12 f f t f i 4 18 t "2281 18 20 18" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_char(internal, "char", int8) => "char" */ 
DATA(insert OID = 7498 ( lag_char  PGNSP PGUID 12 f f t f i 3 18 t "2281 18 20" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_char(internal, "char") => "char" */ 
DATA(insert OID = 7500 ( lag_char  PGNSP PGUID 12 f f t f i 2 18 t "2281 18" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_cidr(internal, cidr, int8, cidr) => cidr */ 
DATA(insert OID = 7502 ( lag_cidr  PGNSP PGUID 12 f f t f i 4 650 t "2281 650 20 650" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_cidr(internal, cidr, int8) => cidr */ 
DATA(insert OID = 7504 ( lag_cidr  PGNSP PGUID 12 f f t f i 3 650 t "2281 650 20" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_cidr(internal, cidr) => cidr */ 
DATA(insert OID = 7506 ( lag_cidr  PGNSP PGUID 12 f f t f i 2 650 t "2281 650" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_circle(internal, circle, int8, circle) => circle */ 
DATA(insert OID = 7508 ( lag_circle  PGNSP PGUID 12 f f t f i 4 718 t "2281 718 20 718" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_circle(internal, circle, int8) => circle */ 
DATA(insert OID = 7510 ( lag_circle  PGNSP PGUID 12 f f t f i 3 718 t "2281 718 20" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_circle(internal, circle) => circle */ 
DATA(insert OID = 7512 ( lag_circle  PGNSP PGUID 12 f f t f i 2 718 t "2281 718" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_float4(internal, float4, int8, float4) => float4 */ 
DATA(insert OID = 7514 ( lag_float4  PGNSP PGUID 12 f f t f i 4 700 t "2281 700 20 700" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_float4(internal, float4, int8) => float4 */ 
DATA(insert OID = 7516 ( lag_float4  PGNSP PGUID 12 f f t f i 3 700 t "2281 700 20" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_float4(internal, float4) => float4 */ 
DATA(insert OID = 7518 ( lag_float4  PGNSP PGUID 12 f f t f i 2 700 t "2281 700" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_float8(internal, float8, int8, float8) => float8 */ 
DATA(insert OID = 7520 ( lag_float8  PGNSP PGUID 12 f f t f i 4 701 t "2281 701 20 701" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_float8(internal, float8, int8) => float8 */ 
DATA(insert OID = 7522 ( lag_float8  PGNSP PGUID 12 f f t f i 3 701 t "2281 701 20" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_float8(internal, float8) => float8 */ 
DATA(insert OID = 7524 ( lag_float8  PGNSP PGUID 12 f f t f i 2 701 t "2281 701" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_inet(internal, inet, int8, inet) => inet */ 
DATA(insert OID = 7526 ( lag_inet  PGNSP PGUID 12 f f t f i 4 869 t "2281 869 20 869" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_inet(internal, inet, int8) => inet */ 
DATA(insert OID = 7528 ( lag_inet  PGNSP PGUID 12 f f t f i 3 869 t "2281 869 20" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_inet(internal, inet) => inet */ 
DATA(insert OID = 7530 ( lag_inet  PGNSP PGUID 12 f f t f i 2 869 t "2281 869" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_interval(internal, "interval", int8, "interval") => "interval" */ 
DATA(insert OID = 7532 ( lag_interval  PGNSP PGUID 12 f f t f i 4 1186 t "2281 1186 20 1186" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_interval(internal, "interval", int8) => "interval" */ 
DATA(insert OID = 7534 ( lag_interval  PGNSP PGUID 12 f f t f i 3 1186 t "2281 1186 20" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_interval(internal, "interval") => "interval" */ 
DATA(insert OID = 7536 ( lag_interval  PGNSP PGUID 12 f f t f i 2 1186 t "2281 1186" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_line(internal, line, int8, line) => line */ 
DATA(insert OID = 7538 ( lag_line  PGNSP PGUID 12 f f t f i 4 628 t "2281 628 20 628" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_line(internal, line, int8) => line */ 
DATA(insert OID = 7540 ( lag_line  PGNSP PGUID 12 f f t f i 3 628 t "2281 628 20" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_line(internal, line) => line */ 
DATA(insert OID = 7542 ( lag_line  PGNSP PGUID 12 f f t f i 2 628 t "2281 628" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_lseg(internal, lseg, int8, lseg) => lseg */ 
DATA(insert OID = 7544 ( lag_lseg  PGNSP PGUID 12 f f t f i 4 601 t "2281 601 20 601" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_lseg(internal, lseg, int8) => lseg */ 
DATA(insert OID = 7546 ( lag_lseg  PGNSP PGUID 12 f f t f i 3 601 t "2281 601 20" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_lseg(internal, lseg) => lseg */ 
DATA(insert OID = 7548 ( lag_lseg  PGNSP PGUID 12 f f t f i 2 601 t "2281 601" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_macaddr(internal, macaddr, int8, macaddr) => macaddr */ 
DATA(insert OID = 7550 ( lag_macaddr  PGNSP PGUID 12 f f t f i 4 829 t "2281 829 20 829" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_macaddr(internal, macaddr, int8) => macaddr */ 
DATA(insert OID = 7552 ( lag_macaddr  PGNSP PGUID 12 f f t f i 3 829 t "2281 829 20" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_macaddr(internal, macaddr) => macaddr */ 
DATA(insert OID = 7554 ( lag_macaddr  PGNSP PGUID 12 f f t f i 2 829 t "2281 829" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_smallint(internal, int2, int8, int2) => int2 */ 
DATA(insert OID = 7556 ( lag_smallint  PGNSP PGUID 12 f f t f i 4 21 t "2281 21 20 21" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_smallint(internal, int2, int8) => int2 */ 
DATA(insert OID = 7558 ( lag_smallint  PGNSP PGUID 12 f f t f i 3 21 t "2281 21 20" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_smallint(internal, int2) => int2 */ 
DATA(insert OID = 7560 ( lag_smallint  PGNSP PGUID 12 f f t f i 2 21 t "2281 21" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_int4(internal, int4, int8, int4) => int4 */ 
DATA(insert OID = 7562 ( lag_int4  PGNSP PGUID 12 f f t f i 4 23 t "2281 23 20 23" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_int4(internal, int4, int8) => int4 */ 
DATA(insert OID = 7564 ( lag_int4  PGNSP PGUID 12 f f t f i 3 23 t "2281 23 20" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_int4(internal, int4) => int4 */ 
DATA(insert OID = 7566 ( lag_int4  PGNSP PGUID 12 f f t f i 2 23 t "2281 23" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_int8(internal, int8, int8, int8) => int8 */ 
DATA(insert OID = 7568 ( lag_int8  PGNSP PGUID 12 f f t f i 4 20 t "2281 20 20 20" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_int8(internal, int8, int8) => int8 */ 
DATA(insert OID = 7570 ( lag_int8  PGNSP PGUID 12 f f t f i 3 20 t "2281 20 20" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_int8(internal, int8) => int8 */ 
DATA(insert OID = 7572 ( lag_int8  PGNSP PGUID 12 f f t f i 2 20 t "2281 20" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_money(internal, money, int8, money) => money */ 
DATA(insert OID = 7574 ( lag_money  PGNSP PGUID 12 f f t f i 4 790 t "2281 790 20 790" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_money(internal, money, int8) => money */ 
DATA(insert OID = 7576 ( lag_money  PGNSP PGUID 12 f f t f i 3 790 t "2281 790 20" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_money(internal, money) => money */ 
DATA(insert OID = 7578 ( lag_money  PGNSP PGUID 12 f f t f i 2 790 t "2281 790" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_name(internal, name, int8, name) => name */ 
DATA(insert OID = 7580 ( lag_name  PGNSP PGUID 12 f f t f i 4 19 t "2281 19 20 19" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_name(internal, name, int8) => name */ 
DATA(insert OID = 7582 ( lag_name  PGNSP PGUID 12 f f t f i 3 19 t "2281 19 20" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_name(internal, name) => name */ 
DATA(insert OID = 7584 ( lag_name  PGNSP PGUID 12 f f t f i 2 19 t "2281 19" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_numeric(internal, "numeric", int8, "numeric") => "numeric" */ 
DATA(insert OID = 7586 ( lag_numeric  PGNSP PGUID 12 f f t f i 4 1700 t "2281 1700 20 1700" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_numeric(internal, "numeric", int8) => "numeric" */ 
DATA(insert OID = 7588 ( lag_numeric  PGNSP PGUID 12 f f t f i 3 1700 t "2281 1700 20" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_numeric(internal, "numeric") => "numeric" */ 
DATA(insert OID = 7590 ( lag_numeric  PGNSP PGUID 12 f f t f i 2 1700 t "2281 1700" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_oid(internal, oid, int8, oid) => oid */ 
DATA(insert OID = 7592 ( lag_oid  PGNSP PGUID 12 f f t f i 4 26 t "2281 26 20 26" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_oid(internal, oid, int8) => oid */ 
DATA(insert OID = 7594 ( lag_oid  PGNSP PGUID 12 f f t f i 3 26 t "2281 26 20" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_oid(internal, oid) => oid */ 
DATA(insert OID = 7596 ( lag_oid  PGNSP PGUID 12 f f t f i 2 26 t "2281 26" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_path(internal, path, int8, path) => path */ 
DATA(insert OID = 7598 ( lag_path  PGNSP PGUID 12 f f t f i 4 602 t "2281 602 20 602" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_path(internal, path, int8) => path */ 
DATA(insert OID = 7600 ( lag_path  PGNSP PGUID 12 f f t f i 3 602 t "2281 602 20" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_path(internal, path) => path */ 
DATA(insert OID = 7602 ( lag_path  PGNSP PGUID 12 f f t f i 2 602 t "2281 602" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_point(internal, point, int8, point) => point */ 
DATA(insert OID = 7604 ( lag_point  PGNSP PGUID 12 f f t f i 4 600 t "2281 600 20 600" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_point(internal, point, int8) => point */ 
DATA(insert OID = 7606 ( lag_point  PGNSP PGUID 12 f f t f i 3 600 t "2281 600 20" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_point(internal, point) => point */ 
DATA(insert OID = 7608 ( lag_point  PGNSP PGUID 12 f f t f i 2 600 t "2281 600" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_polygon(internal, polygon, int8, polygon) => polygon */ 
DATA(insert OID = 7610 ( lag_polygon  PGNSP PGUID 12 f f t f i 4 604 t "2281 604 20 604" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_polygon(internal, polygon, int8) => polygon */ 
DATA(insert OID = 7612 ( lag_polygon  PGNSP PGUID 12 f f t f i 3 604 t "2281 604 20" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_polygon(internal, polygon) => polygon */ 
DATA(insert OID = 7614 ( lag_polygon  PGNSP PGUID 12 f f t f i 2 604 t "2281 604" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_reltime(internal, reltime, int8, reltime) => reltime */ 
DATA(insert OID = 7616 ( lag_reltime  PGNSP PGUID 12 f f t f i 4 703 t "2281 703 20 703" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_reltime(internal, reltime, int8) => reltime */ 
DATA(insert OID = 7618 ( lag_reltime  PGNSP PGUID 12 f f t f i 3 703 t "2281 703 20" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_reltime(internal, reltime) => reltime */ 
DATA(insert OID = 7620 ( lag_reltime  PGNSP PGUID 12 f f t f i 2 703 t "2281 703" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_text(internal, text, int8, text) => text */ 
DATA(insert OID = 7622 ( lag_text  PGNSP PGUID 12 f f t f i 4 25 t "2281 25 20 25" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_text(internal, text, int8) => text */ 
DATA(insert OID = 7624 ( lag_text  PGNSP PGUID 12 f f t f i 3 25 t "2281 25 20" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_text(internal, text) => text */ 
DATA(insert OID = 7626 ( lag_text  PGNSP PGUID 12 f f t f i 2 25 t "2281 25" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_tid(internal, tid, int8, tid) => tid */ 
DATA(insert OID = 7628 ( lag_tid  PGNSP PGUID 12 f f t f i 4 27 t "2281 27 20 27" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_tid(internal, tid, int8) => tid */ 
DATA(insert OID = 7630 ( lag_tid  PGNSP PGUID 12 f f t f i 3 27 t "2281 27 20" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_tid(internal, tid) => tid */ 
DATA(insert OID = 7632 ( lag_tid  PGNSP PGUID 12 f f t f i 2 27 t "2281 27" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_time(internal, "time", int8, "time") => "time" */ 
DATA(insert OID = 7634 ( lag_time  PGNSP PGUID 12 f f t f i 4 1083 t "2281 1083 20 1083" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_time(internal, "time", int8) => "time" */ 
DATA(insert OID = 7636 ( lag_time  PGNSP PGUID 12 f f t f i 3 1083 t "2281 1083 20" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_time(internal, "time") => "time" */ 
DATA(insert OID = 7638 ( lag_time  PGNSP PGUID 12 f f t f i 2 1083 t "2281 1083" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_timestamp(internal, "timestamp", int8, "timestamp") => "timestamp" */ 
DATA(insert OID = 7640 ( lag_timestamp  PGNSP PGUID 12 f f t f i 4 1114 t "2281 1114 20 1114" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_timestamp(internal, "timestamp", int8) => "timestamp" */ 
DATA(insert OID = 7642 ( lag_timestamp  PGNSP PGUID 12 f f t f i 3 1114 t "2281 1114 20" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_timestamp(internal, "timestamp") => "timestamp" */ 
DATA(insert OID = 7644 ( lag_timestamp  PGNSP PGUID 12 f f t f i 2 1114 t "2281 1114" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_timestamptz(internal, timestamptz, int8, timestamptz) => timestamptz */ 
DATA(insert OID = 7646 ( lag_timestamptz  PGNSP PGUID 12 f f t f i 4 1184 t "2281 1184 20 1184" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_timestamptz(internal, timestamptz, int8) => timestamptz */ 
DATA(insert OID = 7648 ( lag_timestamptz  PGNSP PGUID 12 f f t f i 3 1184 t "2281 1184 20" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_timestamptz(internal, timestamptz) => timestamptz */ 
DATA(insert OID = 7650 ( lag_timestamptz  PGNSP PGUID 12 f f t f i 2 1184 t "2281 1184" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_timetz(internal, timetz, int8, timetz) => timetz */ 
DATA(insert OID = 7652 ( lag_timetz  PGNSP PGUID 12 f f t f i 4 1266 t "2281 1266 20 1266" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_timetz(internal, timetz, int8) => timetz */ 
DATA(insert OID = 7654 ( lag_timetz  PGNSP PGUID 12 f f t f i 3 1266 t "2281 1266 20" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_timetz(internal, timetz) => timetz */ 
DATA(insert OID = 7656 ( lag_timetz  PGNSP PGUID 12 f f t f i 2 1266 t "2281 1266" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_varbit(internal, varbit, int8, varbit) => varbit */ 
DATA(insert OID = 7658 ( lag_varbit  PGNSP PGUID 12 f f t f i 4 1562 t "2281 1562 20 1562" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_varbit(internal, varbit, int8) => varbit */ 
DATA(insert OID = 7660 ( lag_varbit  PGNSP PGUID 12 f f t f i 3 1562 t "2281 1562 20" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_varbit(internal, varbit) => varbit */ 
DATA(insert OID = 7662 ( lag_varbit  PGNSP PGUID 12 f f t f i 2 1562 t "2281 1562" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_varchar(internal, "varchar", int8, "varchar") => "varchar" */ 
DATA(insert OID = 7664 ( lag_varchar  PGNSP PGUID 12 f f t f i 4 1043 t "2281 1043 20 1043" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_varchar(internal, "varchar", int8) => "varchar" */ 
DATA(insert OID = 7666 ( lag_varchar  PGNSP PGUID 12 f f t f i 3 1043 t "2281 1043 20" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_varchar(internal, "varchar") => "varchar" */ 
DATA(insert OID = 7668 ( lag_varchar  PGNSP PGUID 12 f f t f i 2 1043 t "2281 1043" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_xid(internal, xid, int8, xid) => xid */ 
DATA(insert OID = 7670 ( lag_xid  PGNSP PGUID 12 f f t f i 4 28 t "2281 28 20 28" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_xid(internal, xid, int8) => xid */ 
DATA(insert OID = 7672 ( lag_xid  PGNSP PGUID 12 f f t f i 3 28 t "2281 28 20" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_xid(internal, xid) => xid */ 
DATA(insert OID = 7674 ( lag_xid  PGNSP PGUID 12 f f t f i 2 28 t "2281 28" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_any(internal, anyarray, int8, anyarray) => anyarray */ 
DATA(insert OID = 7208 ( lag_any  PGNSP PGUID 12 f f t f i 4 2277 t "2281 2277 20 2277" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_any(internal, anyarray, int8) => anyarray */ 
DATA(insert OID = 7209 ( lag_any  PGNSP PGUID 12 f f t f i 3 2277 t "2281 2277 20" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_any(internal, anyarray) => anyarray */ 
DATA(insert OID = 7210 ( lag_any  PGNSP PGUID 12 f f t f i 2 2277 t "2281 2277" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_bytea(internal, bytea) => bytea */ 
DATA(insert OID = 7227 ( lag_bytea  PGNSP PGUID 12 f f t f i 2 17 t "2281 17" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_bytea(internal, bytea, int8) => bytea */ 
DATA(insert OID = 7229 ( lag_bytea  PGNSP PGUID 12 f f t f i 3 17 t "2281 17 20" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_bytea(internal, bytea, int8, bytea) => bytea */ 
DATA(insert OID = 7231 ( lag_bytea  PGNSP PGUID 12 f f t f i 4 17 t "2281 17 20 17" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_bit(internal, "bit") => "bit" */ 
DATA(insert OID = 7251 ( lag_bit  PGNSP PGUID 12 f f t f i 2 1560 t "2281 1560" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_bit(internal, "bit", int8) => "bit" */ 
DATA(insert OID = 7253 ( lag_bit  PGNSP PGUID 12 f f t f i 3 1560 t "2281 1560 20" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_bit(internal, "bit", int8, "bit") => "bit" */ 
DATA(insert OID = 7255 ( lag_bit  PGNSP PGUID 12 f f t f i 4 1560 t "2281 1560 20 1560" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_box(internal, box) => box */ 
DATA(insert OID = 7267 ( lag_box  PGNSP PGUID 12 f f t f i 2 603 t "2281 603" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_box(internal, box, int8) => box */ 
DATA(insert OID = 7269 ( lag_box  PGNSP PGUID 12 f f t f i 3 603 t "2281 603 20" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lag_box(internal, box, int8, box) => box */ 
DATA(insert OID = 7271 ( lag_box  PGNSP PGUID 12 f f t f i 4 603 t "2281 603 20 603" _null_ _null_ _null_ lag_generic _null_ _null_ n 0));

/* lead_int(internal, int4, int8, int4) => int4 */ 
DATA(insert OID = 7106 ( lead_int  PGNSP PGUID 12 f f t f i 4 23 t "2281 23 20 23" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_int(internal, int4, int8) => int4 */ 
DATA(insert OID = 7104 ( lead_int  PGNSP PGUID 12 f f t f i 3 23 t "2281 23 20" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_int(internal, int4) => int4 */ 
DATA(insert OID = 7105 ( lead_int  PGNSP PGUID 12 f f t f i 2 23 t "2281 23" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_bool(internal, bool, int8, bool) => bool */ 
DATA(insert OID = 7311 ( lead_bool  PGNSP PGUID 12 f f t f i 4 16 t "2281 16 20 16" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_bool(internal, bool, int8) => bool */ 
DATA(insert OID = 7313 ( lead_bool  PGNSP PGUID 12 f f t f i 3 16 t "2281 16 20" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_bool(internal, bool) => bool */ 
DATA(insert OID = 7315 ( lead_bool  PGNSP PGUID 12 f f t f i 2 16 t "2281 16" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_char(internal, "char", int8, "char") => "char" */ 
DATA(insert OID = 7317 ( lead_char  PGNSP PGUID 12 f f t f i 4 18 t "2281 18 20 18" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_char(internal, "char", int8) => "char" */ 
DATA(insert OID = 7319 ( lead_char  PGNSP PGUID 12 f f t f i 3 18 t "2281 18 20" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_char(internal, "char") => "char" */ 
DATA(insert OID = 7321 ( lead_char  PGNSP PGUID 12 f f t f i 2 18 t "2281 18" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_cidr(internal, cidr, int8, cidr) => cidr */ 
DATA(insert OID = 7323 ( lead_cidr  PGNSP PGUID 12 f f t f i 4 650 t "2281 650 20 650" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_cidr(internal, cidr, int8) => cidr */ 
DATA(insert OID = 7325 ( lead_cidr  PGNSP PGUID 12 f f t f i 3 650 t "2281 650 20" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_cidr(internal, cidr) => cidr */ 
DATA(insert OID = 7327 ( lead_cidr  PGNSP PGUID 12 f f t f i 2 650 t "2281 650" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_circle(internal, circle, int8, circle) => circle */ 
DATA(insert OID = 7329 ( lead_circle  PGNSP PGUID 12 f f t f i 4 718 t "2281 718 20 718" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_circle(internal, circle, int8) => circle */ 
DATA(insert OID = 7331 ( lead_circle  PGNSP PGUID 12 f f t f i 3 718 t "2281 718 20" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_circle(internal, circle) => circle */ 
DATA(insert OID = 7333 ( lead_circle  PGNSP PGUID 12 f f t f i 2 718 t "2281 718" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_float4(internal, float4, int8, float4) => float4 */ 
DATA(insert OID = 7335 ( lead_float4  PGNSP PGUID 12 f f t f i 4 700 t "2281 700 20 700" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_float4(internal, float4, int8) => float4 */ 
DATA(insert OID = 7337 ( lead_float4  PGNSP PGUID 12 f f t f i 3 700 t "2281 700 20" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_float4(internal, float4) => float4 */ 
DATA(insert OID = 7339 ( lead_float4  PGNSP PGUID 12 f f t f i 2 700 t "2281 700" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_float8(internal, float8, int8, float8) => float8 */ 
DATA(insert OID = 7341 ( lead_float8  PGNSP PGUID 12 f f t f i 4 701 t "2281 701 20 701" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_float8(internal, float8, int8) => float8 */ 
DATA(insert OID = 7343 ( lead_float8  PGNSP PGUID 12 f f t f i 3 701 t "2281 701 20" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_float8(internal, float8) => float8 */ 
DATA(insert OID = 7345 ( lead_float8  PGNSP PGUID 12 f f t f i 2 701 t "2281 701" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_inet(internal, inet, int8, inet) => inet */ 
DATA(insert OID = 7347 ( lead_inet  PGNSP PGUID 12 f f t f i 4 869 t "2281 869 20 869" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_inet(internal, inet, int8) => inet */ 
DATA(insert OID = 7349 ( lead_inet  PGNSP PGUID 12 f f t f i 3 869 t "2281 869 20" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_inet(internal, inet) => inet */ 
DATA(insert OID = 7351 ( lead_inet  PGNSP PGUID 12 f f t f i 2 869 t "2281 869" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_interval(internal, "interval", int8, "interval") => "interval" */ 
DATA(insert OID = 7353 ( lead_interval  PGNSP PGUID 12 f f t f i 4 1186 t "2281 1186 20 1186" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_interval(internal, "interval", int8) => "interval" */ 
DATA(insert OID = 7355 ( lead_interval  PGNSP PGUID 12 f f t f i 3 1186 t "2281 1186 20" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_interval(internal, "interval") => "interval" */ 
DATA(insert OID = 7357 ( lead_interval  PGNSP PGUID 12 f f t f i 2 1186 t "2281 1186" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_line(internal, line, int8, line) => line */ 
DATA(insert OID = 7359 ( lead_line  PGNSP PGUID 12 f f t f i 4 628 t "2281 628 20 628" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_line(internal, line, int8) => line */ 
DATA(insert OID = 7361 ( lead_line  PGNSP PGUID 12 f f t f i 3 628 t "2281 628 20" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_line(internal, line) => line */ 
DATA(insert OID = 7363 ( lead_line  PGNSP PGUID 12 f f t f i 2 628 t "2281 628" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_lseg(internal, lseg, int8, lseg) => lseg */ 
DATA(insert OID = 7365 ( lead_lseg  PGNSP PGUID 12 f f t f i 4 601 t "2281 601 20 601" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_lseg(internal, lseg, int8) => lseg */ 
DATA(insert OID = 7367 ( lead_lseg  PGNSP PGUID 12 f f t f i 3 601 t "2281 601 20" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_lseg(internal, lseg) => lseg */ 
DATA(insert OID = 7369 ( lead_lseg  PGNSP PGUID 12 f f t f i 2 601 t "2281 601" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_macaddr(internal, macaddr, int8, macaddr) => macaddr */ 
DATA(insert OID = 7371 ( lead_macaddr  PGNSP PGUID 12 f f t f i 4 829 t "2281 829 20 829" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_macaddr(internal, macaddr, int8) => macaddr */ 
DATA(insert OID = 7373 ( lead_macaddr  PGNSP PGUID 12 f f t f i 3 829 t "2281 829 20" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_macaddr(internal, macaddr) => macaddr */ 
DATA(insert OID = 7375 ( lead_macaddr  PGNSP PGUID 12 f f t f i 2 829 t "2281 829" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_smallint(internal, int2, int8, int2) => int2 */ 
DATA(insert OID = 7377 ( lead_smallint  PGNSP PGUID 12 f f t f i 4 21 t "2281 21 20 21" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_smallint(internal, int2, int8) => int2 */ 
DATA(insert OID = 7379 ( lead_smallint  PGNSP PGUID 12 f f t f i 3 21 t "2281 21 20" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_smallint(internal, int2) => int2 */ 
DATA(insert OID = 7381 ( lead_smallint  PGNSP PGUID 12 f f t f i 2 21 t "2281 21" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_int8(internal, int8, int8, int8) => int8 */ 
DATA(insert OID = 7383 ( lead_int8  PGNSP PGUID 12 f f t f i 4 20 t "2281 20 20 20" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_int8(internal, int8, int8) => int8 */ 
DATA(insert OID = 7385 ( lead_int8  PGNSP PGUID 12 f f t f i 3 20 t "2281 20 20" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_int8(internal, int8) => int8 */ 
DATA(insert OID = 7387 ( lead_int8  PGNSP PGUID 12 f f t f i 2 20 t "2281 20" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_money(internal, money, int8, money) => money */ 
DATA(insert OID = 7389 ( lead_money  PGNSP PGUID 12 f f t f i 4 790 t "2281 790 20 790" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_money(internal, money, int8) => money */ 
DATA(insert OID = 7391 ( lead_money  PGNSP PGUID 12 f f t f i 3 790 t "2281 790 20" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_money(internal, money) => money */ 
DATA(insert OID = 7393 ( lead_money  PGNSP PGUID 12 f f t f i 2 790 t "2281 790" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_name(internal, name, int8, name) => name */ 
DATA(insert OID = 7395 ( lead_name  PGNSP PGUID 12 f f t f i 4 19 t "2281 19 20 19" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_name(internal, name, int8) => name */ 
DATA(insert OID = 7397 ( lead_name  PGNSP PGUID 12 f f t f i 3 19 t "2281 19 20" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_name(internal, name) => name */ 
DATA(insert OID = 7399 ( lead_name  PGNSP PGUID 12 f f t f i 2 19 t "2281 19" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_numeric(internal, "numeric", int8, "numeric") => "numeric" */ 
DATA(insert OID = 7401 ( lead_numeric  PGNSP PGUID 12 f f t f i 4 1700 t "2281 1700 20 1700" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_numeric(internal, "numeric", int8) => "numeric" */ 
DATA(insert OID = 7403 ( lead_numeric  PGNSP PGUID 12 f f t f i 3 1700 t "2281 1700 20" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_numeric(internal, "numeric") => "numeric" */ 
DATA(insert OID = 7405 ( lead_numeric  PGNSP PGUID 12 f f t f i 2 1700 t "2281 1700" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_oid(internal, oid, int8, oid) => oid */ 
DATA(insert OID = 7407 ( lead_oid  PGNSP PGUID 12 f f t f i 4 26 t "2281 26 20 26" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_oid(internal, oid, int8) => oid */ 
DATA(insert OID = 7409 ( lead_oid  PGNSP PGUID 12 f f t f i 3 26 t "2281 26 20" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_oid(internal, oid) => oid */ 
DATA(insert OID = 7411 ( lead_oid  PGNSP PGUID 12 f f t f i 2 26 t "2281 26" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_path(internal, path, int8, path) => path */ 
DATA(insert OID = 7413 ( lead_path  PGNSP PGUID 12 f f t f i 4 602 t "2281 602 20 602" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_path(internal, path, int8) => path */ 
DATA(insert OID = 7415 ( lead_path  PGNSP PGUID 12 f f t f i 3 602 t "2281 602 20" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_path(internal, path) => path */ 
DATA(insert OID = 7417 ( lead_path  PGNSP PGUID 12 f f t f i 2 602 t "2281 602" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_point(internal, point, int8, point) => point */ 
DATA(insert OID = 7419 ( lead_point  PGNSP PGUID 12 f f t f i 4 600 t "2281 600 20 600" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_point(internal, point, int8) => point */ 
DATA(insert OID = 7421 ( lead_point  PGNSP PGUID 12 f f t f i 3 600 t "2281 600 20" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_point(internal, point) => point */ 
DATA(insert OID = 7423 ( lead_point  PGNSP PGUID 12 f f t f i 2 600 t "2281 600" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_polygon(internal, polygon, int8, polygon) => polygon */ 
DATA(insert OID = 7425 ( lead_polygon  PGNSP PGUID 12 f f t f i 4 604 t "2281 604 20 604" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_polygon(internal, polygon, int8) => polygon */ 
DATA(insert OID = 7427 ( lead_polygon  PGNSP PGUID 12 f f t f i 3 604 t "2281 604 20" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_polygon(internal, polygon) => polygon */ 
DATA(insert OID = 7429 ( lead_polygon  PGNSP PGUID 12 f f t f i 2 604 t "2281 604" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_reltime(internal, reltime, int8, reltime) => reltime */ 
DATA(insert OID = 7431 ( lead_reltime  PGNSP PGUID 12 f f t f i 4 703 t "2281 703 20 703" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_reltime(internal, reltime, int8) => reltime */ 
DATA(insert OID = 7433 ( lead_reltime  PGNSP PGUID 12 f f t f i 3 703 t "2281 703 20" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_reltime(internal, reltime) => reltime */ 
DATA(insert OID = 7435 ( lead_reltime  PGNSP PGUID 12 f f t f i 2 703 t "2281 703" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_text(internal, text, int8, text) => text */ 
DATA(insert OID = 7437 ( lead_text  PGNSP PGUID 12 f f t f i 4 25 t "2281 25 20 25" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_text(internal, text, int8) => text */ 
DATA(insert OID = 7439 ( lead_text  PGNSP PGUID 12 f f t f i 3 25 t "2281 25 20" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_text(internal, text) => text */ 
DATA(insert OID = 7441 ( lead_text  PGNSP PGUID 12 f f t f i 2 25 t "2281 25" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_tid(internal, tid, int8, tid) => tid */ 
DATA(insert OID = 7443 ( lead_tid  PGNSP PGUID 12 f f t f i 4 27 t "2281 27 20 27" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_tid(internal, tid, int8) => tid */ 
DATA(insert OID = 7445 ( lead_tid  PGNSP PGUID 12 f f t f i 3 27 t "2281 27 20" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_tid(internal, tid) => tid */ 
DATA(insert OID = 7447 ( lead_tid  PGNSP PGUID 12 f f t f i 2 27 t "2281 27" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_time(internal, "time", int8, "time") => "time" */ 
DATA(insert OID = 7449 ( lead_time  PGNSP PGUID 12 f f t f i 4 1083 t "2281 1083 20 1083" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_time(internal, "time", int8) => "time" */ 
DATA(insert OID = 7451 ( lead_time  PGNSP PGUID 12 f f t f i 3 1083 t "2281 1083 20" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_time(internal, "time") => "time" */ 
DATA(insert OID = 7453 ( lead_time  PGNSP PGUID 12 f f t f i 2 1083 t "2281 1083" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_timestamp(internal, "timestamp", int8, "timestamp") => "timestamp" */ 
DATA(insert OID = 7455 ( lead_timestamp  PGNSP PGUID 12 f f t f i 4 1114 t "2281 1114 20 1114" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_timestamp(internal, "timestamp", int8) => "timestamp" */ 
DATA(insert OID = 7457 ( lead_timestamp  PGNSP PGUID 12 f f t f i 3 1114 t "2281 1114 20" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_timestamp(internal, "timestamp") => "timestamp" */ 
DATA(insert OID = 7459 ( lead_timestamp  PGNSP PGUID 12 f f t f i 2 1114 t "2281 1114" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_timestamptz(internal, timestamptz, int8, timestamptz) => timestamptz */ 
DATA(insert OID = 7461 ( lead_timestamptz  PGNSP PGUID 12 f f t f i 4 1184 t "2281 1184 20 1184" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_timestamptz(internal, timestamptz, int8) => timestamptz */ 
DATA(insert OID = 7463 ( lead_timestamptz  PGNSP PGUID 12 f f t f i 3 1184 t "2281 1184 20" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_timestamptz(internal, timestamptz) => timestamptz */ 
DATA(insert OID = 7465 ( lead_timestamptz  PGNSP PGUID 12 f f t f i 2 1184 t "2281 1184" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_timetz(internal, timetz, int8, timetz) => timetz */ 
DATA(insert OID = 7467 ( lead_timetz  PGNSP PGUID 12 f f t f i 4 1266 t "2281 1266 20 1266" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_timetz(internal, timetz, int8) => timetz */ 
DATA(insert OID = 7469 ( lead_timetz  PGNSP PGUID 12 f f t f i 3 1266 t "2281 1266 20" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_timetz(internal, timetz) => timetz */ 
DATA(insert OID = 7471 ( lead_timetz  PGNSP PGUID 12 f f t f i 2 1266 t "2281 1266" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_varbit(internal, varbit, int8, varbit) => varbit */ 
DATA(insert OID = 7473 ( lead_varbit  PGNSP PGUID 12 f f t f i 4 1562 t "2281 1562 20 1562" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_varbit(internal, varbit, int8) => varbit */ 
DATA(insert OID = 7475 ( lead_varbit  PGNSP PGUID 12 f f t f i 3 1562 t "2281 1562 20" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_varbit(internal, varbit) => varbit */ 
DATA(insert OID = 7477 ( lead_varbit  PGNSP PGUID 12 f f t f i 2 1562 t "2281 1562" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_varchar(internal, "varchar", int8, "varchar") => "varchar" */ 
DATA(insert OID = 7479 ( lead_varchar  PGNSP PGUID 12 f f t f i 4 1043 t "2281 1043 20 1043" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_varchar(internal, "varchar", int8) => "varchar" */ 
DATA(insert OID = 7481 ( lead_varchar  PGNSP PGUID 12 f f t f i 3 1043 t "2281 1043 20" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_varchar(internal, "varchar") => "varchar" */ 
DATA(insert OID = 7483 ( lead_varchar  PGNSP PGUID 12 f f t f i 2 1043 t "2281 1043" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_xid(internal, xid, int8, xid) => xid */ 
DATA(insert OID = 7485 ( lead_xid  PGNSP PGUID 12 f f t f i 4 28 t "2281 28 20 28" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_xid(internal, xid, int8) => xid */ 
DATA(insert OID = 7487 ( lead_xid  PGNSP PGUID 12 f f t f i 3 28 t "2281 28 20" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_xid(internal, xid) => xid */ 
DATA(insert OID = 7489 ( lead_xid  PGNSP PGUID 12 f f t f i 2 28 t "2281 28" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_any(internal, anyarray, int8, anyarray) => anyarray */ 
DATA(insert OID = 7217 ( lead_any  PGNSP PGUID 12 f f t f i 4 2277 t "2281 2277 20 2277" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_any(internal, anyarray, int8) => anyarray */ 
DATA(insert OID = 7218 ( lead_any  PGNSP PGUID 12 f f t f i 3 2277 t "2281 2277 20" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_any(internal, anyarray) => anyarray */ 
DATA(insert OID = 7219 ( lead_any  PGNSP PGUID 12 f f t f i 2 2277 t "2281 2277" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_bytea(internal, bytea) => bytea */ 
DATA(insert OID = 7221 ( lead_bytea  PGNSP PGUID 12 f f t f i 2 17 t "2281 17" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_bytea(internal, bytea, int8) => bytea */ 
DATA(insert OID = 7223 ( lead_bytea  PGNSP PGUID 12 f f t f i 3 17 t "2281 17 20" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_bytea(internal, bytea, int8, bytea) => bytea */ 
DATA(insert OID = 7225 ( lead_bytea  PGNSP PGUID 12 f f t f i 4 17 t "2281 17 20 17" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_bit(internal, "bit") => "bit" */ 
DATA(insert OID = 7245 ( lead_bit  PGNSP PGUID 12 f f t f i 2 1560 t "2281 1560" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_bit(internal, "bit", int8) => "bit" */ 
DATA(insert OID = 7247 ( lead_bit  PGNSP PGUID 12 f f t f i 3 1560 t "2281 1560 20" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_bit(internal, "bit", int8, "bit") => "bit" */ 
DATA(insert OID = 7249 ( lead_bit  PGNSP PGUID 12 f f t f i 4 1560 t "2281 1560 20 1560" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_box(internal, box) => box */ 
DATA(insert OID = 7261 ( lead_box  PGNSP PGUID 12 f f t f i 2 603 t "2281 603" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_box(internal, box, int8) => box */ 
DATA(insert OID = 7263 ( lead_box  PGNSP PGUID 12 f f t f i 3 603 t "2281 603 20" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* lead_box(internal, box, int8, box) => box */ 
DATA(insert OID = 7265 ( lead_box  PGNSP PGUID 12 f f t f i 4 603 t "2281 603 20 603" _null_ _null_ _null_ lead_generic _null_ _null_ n 0));

/* first_value_bool(internal, bool) => bool */ 
DATA(insert OID = 7111 ( first_value_bool  PGNSP PGUID 12 f f t f i 2 16 t "2281 16" _null_ _null_ _null_ first_value_generic _null_ _null_ n 0));

/* first_value_char(internal, "char") => "char" */ 
DATA(insert OID = 7112 ( first_value_char  PGNSP PGUID 12 f f t f i 2 18 t "2281 18" _null_ _null_ _null_ first_value_generic _null_ _null_ n 0));

/* first_value_cidr(internal, cidr) => cidr */ 
DATA(insert OID = 7113 ( first_value_cidr  PGNSP PGUID 12 f f t f i 2 650 t "2281 650" _null_ _null_ _null_ first_value_generic _null_ _null_ n 0));

/* first_value_circle(internal, circle) => circle */ 
DATA(insert OID = 7114 ( first_value_circle  PGNSP PGUID 12 f f t f i 2 718 t "2281 718" _null_ _null_ _null_ first_value_generic _null_ _null_ n 0));

/* first_value_float4(internal, float4) => float4 */ 
DATA(insert OID = 7115 ( first_value_float4  PGNSP PGUID 12 f f t f i 2 700 t "2281 700" _null_ _null_ _null_ first_value_generic _null_ _null_ n 0));

/* first_value_float8(internal, float8) => float8 */ 
DATA(insert OID = 7116 ( first_value_float8  PGNSP PGUID 12 f f t f i 2 701 t "2281 701" _null_ _null_ _null_ first_value_generic _null_ _null_ n 0));

/* first_value_inet(internal, inet) => inet */ 
DATA(insert OID = 7117 ( first_value_inet  PGNSP PGUID 12 f f t f i 2 869 t "2281 869" _null_ _null_ _null_ first_value_generic _null_ _null_ n 0));

/* first_value_interval(internal, "interval") => "interval" */ 
DATA(insert OID = 7118 ( first_value_interval  PGNSP PGUID 12 f f t f i 2 1186 t "2281 1186" _null_ _null_ _null_ first_value_generic _null_ _null_ n 0));

/* first_value_line(internal, line) => line */ 
DATA(insert OID = 7119 ( first_value_line  PGNSP PGUID 12 f f t f i 2 628 t "2281 628" _null_ _null_ _null_ first_value_generic _null_ _null_ n 0));

/* first_value_lseg(internal, lseg) => lseg */ 
DATA(insert OID = 7120 ( first_value_lseg  PGNSP PGUID 12 f f t f i 2 601 t "2281 601" _null_ _null_ _null_ first_value_generic _null_ _null_ n 0));

/* first_value_macaddr(internal, macaddr) => macaddr */ 
DATA(insert OID = 7121 ( first_value_macaddr  PGNSP PGUID 12 f f t f i 2 829 t "2281 829" _null_ _null_ _null_ first_value_generic _null_ _null_ n 0));

/* first_value_smallint(internal, int2) => int2 */ 
DATA(insert OID = 7122 ( first_value_smallint  PGNSP PGUID 12 f f t f i 2 21 t "2281 21" _null_ _null_ _null_ first_value_generic _null_ _null_ n 0));

/* first_value_int4(internal, int4) => int4 */ 
DATA(insert OID = 7123 ( first_value_int4  PGNSP PGUID 12 f f t f i 2 23 t "2281 23" _null_ _null_ _null_ first_value_generic _null_ _null_ n 0));

/* first_value_int8(internal, int8) => int8 */ 
DATA(insert OID = 7124 ( first_value_int8  PGNSP PGUID 12 f f t f i 2 20 t "2281 20" _null_ _null_ _null_ first_value_generic _null_ _null_ n 0));

/* first_value_money(internal, money) => money */ 
DATA(insert OID = 7125 ( first_value_money  PGNSP PGUID 12 f f t f i 2 790 t "2281 790" _null_ _null_ _null_ first_value_generic _null_ _null_ n 0));

/* first_value_name(internal, name) => name */ 
DATA(insert OID = 7126 ( first_value_name  PGNSP PGUID 12 f f t f i 2 19 t "2281 19" _null_ _null_ _null_ first_value_generic _null_ _null_ n 0));

/* first_value_numeric(internal, "numeric") => "numeric" */ 
DATA(insert OID = 7127 ( first_value_numeric  PGNSP PGUID 12 f f t f i 2 1700 t "2281 1700" _null_ _null_ _null_ first_value_generic _null_ _null_ n 0));

/* first_value_oid(internal, oid) => oid */ 
DATA(insert OID = 7128 ( first_value_oid  PGNSP PGUID 12 f f t f i 2 26 t "2281 26" _null_ _null_ _null_ first_value_generic _null_ _null_ n 0));

/* first_value_path(internal, path) => path */ 
DATA(insert OID = 7129 ( first_value_path  PGNSP PGUID 12 f f t f i 2 602 t "2281 602" _null_ _null_ _null_ first_value_generic _null_ _null_ n 0));

/* first_value_point(internal, point) => point */ 
DATA(insert OID = 7130 ( first_value_point  PGNSP PGUID 12 f f t f i 2 600 t "2281 600" _null_ _null_ _null_ first_value_generic _null_ _null_ n 0));

/* first_value_polygon(internal, polygon) => polygon */ 
DATA(insert OID = 7131 ( first_value_polygon  PGNSP PGUID 12 f f t f i 2 604 t "2281 604" _null_ _null_ _null_ first_value_generic _null_ _null_ n 0));

/* first_value_reltime(internal, reltime) => reltime */ 
DATA(insert OID = 7132 ( first_value_reltime  PGNSP PGUID 12 f f t f i 2 703 t "2281 703" _null_ _null_ _null_ first_value_generic _null_ _null_ n 0));

/* first_value_text(internal, text) => text */ 
DATA(insert OID = 7133 ( first_value_text  PGNSP PGUID 12 f f t f i 2 25 t "2281 25" _null_ _null_ _null_ first_value_generic _null_ _null_ n 0));

/* first_value_tid(internal, tid) => tid */ 
DATA(insert OID = 7134 ( first_value_tid  PGNSP PGUID 12 f f t f i 2 27 t "2281 27" _null_ _null_ _null_ first_value_generic _null_ _null_ n 0));

/* first_value_time(internal, "time") => "time" */ 
DATA(insert OID = 7135 ( first_value_time  PGNSP PGUID 12 f f t f i 2 1083 t "2281 1083" _null_ _null_ _null_ first_value_generic _null_ _null_ n 0));

/* first_value_timestamp(internal, "timestamp") => "timestamp" */ 
DATA(insert OID = 7136 ( first_value_timestamp  PGNSP PGUID 12 f f t f i 2 1114 t "2281 1114" _null_ _null_ _null_ first_value_generic _null_ _null_ n 0));

/* first_value_timestamptz(internal, timestamptz) => timestamptz */ 
DATA(insert OID = 7137 ( first_value_timestamptz  PGNSP PGUID 12 f f t f i 2 1184 t "2281 1184" _null_ _null_ _null_ first_value_generic _null_ _null_ n 0));

/* first_value_timetz(internal, timetz) => timetz */ 
DATA(insert OID = 7138 ( first_value_timetz  PGNSP PGUID 12 f f t f i 2 1266 t "2281 1266" _null_ _null_ _null_ first_value_generic _null_ _null_ n 0));

/* first_value_varbit(internal, varbit) => varbit */ 
DATA(insert OID = 7139 ( first_value_varbit  PGNSP PGUID 12 f f t f i 2 1562 t "2281 1562" _null_ _null_ _null_ first_value_generic _null_ _null_ n 0));

/* first_value_varchar(internal, "varchar") => "varchar" */ 
DATA(insert OID = 7140 ( first_value_varchar  PGNSP PGUID 12 f f t f i 2 1043 t "2281 1043" _null_ _null_ _null_ first_value_generic _null_ _null_ n 0));

/* first_value_xid(internal, xid) => xid */ 
DATA(insert OID = 7141 ( first_value_xid  PGNSP PGUID 12 f f t f i 2 28 t "2281 28" _null_ _null_ _null_ first_value_generic _null_ _null_ n 0));

/* first_value_bytea(internal, bytea) => bytea */ 
DATA(insert OID = 7233 ( first_value_bytea  PGNSP PGUID 12 f f t f i 2 17 t "2281 17" _null_ _null_ _null_ first_value_generic _null_ _null_ n 0));

/* first_value_bit(internal, "bit") => "bit" */ 
DATA(insert OID = 7257 ( first_value_bit  PGNSP PGUID 12 f f t f i 2 1560 t "2281 1560" _null_ _null_ _null_ first_value_generic _null_ _null_ n 0));

/* first_value_box(internal, box) => box */ 
DATA(insert OID = 7273 ( first_value_box  PGNSP PGUID 12 f f t f i 2 603 t "2281 603" _null_ _null_ _null_ first_value_generic _null_ _null_ n 0));

/* first_value_any(internal, anyarray) => anyarray */ 
DATA(insert OID = 7289 ( first_value_any  PGNSP PGUID 12 f f t f i 2 2277 t "2281 2277" _null_ _null_ _null_ first_value_generic _null_ _null_ n 0));

/* last_value_int(internal, int4) => int4 */ 
DATA(insert OID = 7103 ( last_value_int  PGNSP PGUID 12 f f t f i 2 23 t "2281 23" _null_ _null_ _null_ last_value_generic _null_ _null_ n 0));

/* last_value_smallint(internal, int2) => int2 */ 
DATA(insert OID = 7107 ( last_value_smallint  PGNSP PGUID 12 f f t f i 2 21 t "2281 21" _null_ _null_ _null_ last_value_generic _null_ _null_ n 0));

/* last_value_bigint(internal, int8) => int8 */ 
DATA(insert OID = 7108 ( last_value_bigint  PGNSP PGUID 12 f f t f i 2 20 t "2281 20" _null_ _null_ _null_ last_value_generic _null_ _null_ n 0));

/* last_value_numeric(internal, "numeric") => "numeric" */ 
DATA(insert OID = 7109 ( last_value_numeric  PGNSP PGUID 12 f f t f i 2 1700 t "2281 1700" _null_ _null_ _null_ last_value_generic _null_ _null_ n 0));

/* last_value_text(internal, text) => text */ 
DATA(insert OID = 7110 ( last_value_text  PGNSP PGUID 12 f f t f i 2 25 t "2281 25" _null_ _null_ _null_ last_value_generic _null_ _null_ n 0));

/* last_value_bool(internal, bool) => bool */ 
DATA(insert OID = 7165 ( last_value_bool  PGNSP PGUID 12 f f t f i 2 16 t "2281 16" _null_ _null_ _null_ last_value_generic _null_ _null_ n 0));

/* last_value_char(internal, "char") => "char" */ 
DATA(insert OID = 7166 ( last_value_char  PGNSP PGUID 12 f f t f i 2 18 t "2281 18" _null_ _null_ _null_ last_value_generic _null_ _null_ n 0));

/* last_value_cidr(internal, cidr) => cidr */ 
DATA(insert OID = 7167 ( last_value_cidr  PGNSP PGUID 12 f f t f i 2 650 t "2281 650" _null_ _null_ _null_ last_value_generic _null_ _null_ n 0));

/* last_value_circle(internal, circle) => circle */ 
DATA(insert OID = 7168 ( last_value_circle  PGNSP PGUID 12 f f t f i 2 718 t "2281 718" _null_ _null_ _null_ last_value_generic _null_ _null_ n 0));

/* last_value_float4(internal, float4) => float4 */ 
DATA(insert OID = 7142 ( last_value_float4  PGNSP PGUID 12 f f t f i 2 700 t "2281 700" _null_ _null_ _null_ last_value_generic _null_ _null_ n 0));

/* last_value_float8(internal, float8) => float8 */ 
DATA(insert OID = 7143 ( last_value_float8  PGNSP PGUID 12 f f t f i 2 701 t "2281 701" _null_ _null_ _null_ last_value_generic _null_ _null_ n 0));

/* last_value_inet(internal, inet) => inet */ 
DATA(insert OID = 7144 ( last_value_inet  PGNSP PGUID 12 f f t f i 2 869 t "2281 869" _null_ _null_ _null_ last_value_generic _null_ _null_ n 0));

/* last_value_interval(internal, "interval") => "interval" */ 
DATA(insert OID = 7145 ( last_value_interval  PGNSP PGUID 12 f f t f i 2 1186 t "2281 1186" _null_ _null_ _null_ last_value_generic _null_ _null_ n 0));

/* last_value_line(internal, line) => line */ 
DATA(insert OID = 7146 ( last_value_line  PGNSP PGUID 12 f f t f i 2 628 t "2281 628" _null_ _null_ _null_ last_value_generic _null_ _null_ n 0));

/* last_value_lseg(internal, lseg) => lseg */ 
DATA(insert OID = 7147 ( last_value_lseg  PGNSP PGUID 12 f f t f i 2 601 t "2281 601" _null_ _null_ _null_ last_value_generic _null_ _null_ n 0));

/* last_value_macaddr(internal, macaddr) => macaddr */ 
DATA(insert OID = 7148 ( last_value_macaddr  PGNSP PGUID 12 f f t f i 2 829 t "2281 829" _null_ _null_ _null_ last_value_generic _null_ _null_ n 0));

/* last_value_money(internal, money) => money */ 
DATA(insert OID = 7149 ( last_value_money  PGNSP PGUID 12 f f t f i 2 790 t "2281 790" _null_ _null_ _null_ last_value_generic _null_ _null_ n 0));

/* last_value_name(internal, name) => name */ 
DATA(insert OID = 7150 ( last_value_name  PGNSP PGUID 12 f f t f i 2 19 t "2281 19" _null_ _null_ _null_ last_value_generic _null_ _null_ n 0));

/* last_value_oid(internal, oid) => oid */ 
DATA(insert OID = 7151 ( last_value_oid  PGNSP PGUID 12 f f t f i 2 26 t "2281 26" _null_ _null_ _null_ last_value_generic _null_ _null_ n 0));

/* last_value_path(internal, path) => path */ 
DATA(insert OID = 7152 ( last_value_path  PGNSP PGUID 12 f f t f i 2 602 t "2281 602" _null_ _null_ _null_ last_value_generic _null_ _null_ n 0));

/* last_value_point(internal, point) => point */ 
DATA(insert OID = 7153 ( last_value_point  PGNSP PGUID 12 f f t f i 2 600 t "2281 600" _null_ _null_ _null_ last_value_generic _null_ _null_ n 0));

/* last_value_polygon(internal, polygon) => polygon */ 
DATA(insert OID = 7154 ( last_value_polygon  PGNSP PGUID 12 f f t f i 2 604 t "2281 604" _null_ _null_ _null_ last_value_generic _null_ _null_ n 0));

/* last_value_reltime(internal, reltime) => reltime */ 
DATA(insert OID = 7155 ( last_value_reltime  PGNSP PGUID 12 f f t f i 2 703 t "2281 703" _null_ _null_ _null_ last_value_generic _null_ _null_ n 0));

/* last_value_tid(internal, tid) => tid */ 
DATA(insert OID = 7157 ( last_value_tid  PGNSP PGUID 12 f f t f i 2 27 t "2281 27" _null_ _null_ _null_ last_value_generic _null_ _null_ n 0));

/* last_value_time(internal, "time") => "time" */ 
DATA(insert OID = 7158 ( last_value_time  PGNSP PGUID 12 f f t f i 2 1083 t "2281 1083" _null_ _null_ _null_ last_value_generic _null_ _null_ n 0));

/* last_value_timestamp(internal, "timestamp") => "timestamp" */ 
DATA(insert OID = 7159 ( last_value_timestamp  PGNSP PGUID 12 f f t f i 2 1114 t "2281 1114" _null_ _null_ _null_ last_value_generic _null_ _null_ n 0));

/* last_value_timestamptz(internal, timestamptz) => timestamptz */ 
DATA(insert OID = 7160 ( last_value_timestamptz  PGNSP PGUID 12 f f t f i 2 1184 t "2281 1184" _null_ _null_ _null_ last_value_generic _null_ _null_ n 0));

/* last_value_timetz(internal, timetz) => timetz */ 
DATA(insert OID = 7161 ( last_value_timetz  PGNSP PGUID 12 f f t f i 2 1266 t "2281 1266" _null_ _null_ _null_ last_value_generic _null_ _null_ n 0));

/* last_value_varbit(internal, varbit) => varbit */ 
DATA(insert OID = 7162 ( last_value_varbit  PGNSP PGUID 12 f f t f i 2 1562 t "2281 1562" _null_ _null_ _null_ last_value_generic _null_ _null_ n 0));

/* last_value_varchar(internal, "varchar") => "varchar" */ 
DATA(insert OID = 7163 ( last_value_varchar  PGNSP PGUID 12 f f t f i 2 1043 t "2281 1043" _null_ _null_ _null_ last_value_generic _null_ _null_ n 0));

/* last_value_xid(internal, xid) => xid */ 
DATA(insert OID = 7164 ( last_value_xid  PGNSP PGUID 12 f f t f i 2 28 t "2281 28" _null_ _null_ _null_ last_value_generic _null_ _null_ n 0));

/* last_value_bytea(internal, bytea) => bytea */ 
DATA(insert OID = 7239 ( last_value_bytea  PGNSP PGUID 12 f f t f i 2 17 t "2281 17" _null_ _null_ _null_ last_value_generic _null_ _null_ n 0));

/* last_value_bit(internal, "bit") => "bit" */ 
DATA(insert OID = 7259 ( last_value_bit  PGNSP PGUID 12 f f t f i 2 1560 t "2281 1560" _null_ _null_ _null_ last_value_generic _null_ _null_ n 0));

/* last_value_box(internal, box) => box */ 
DATA(insert OID = 7275 ( last_value_box  PGNSP PGUID 12 f f t f i 2 603 t "2281 603" _null_ _null_ _null_ last_value_generic _null_ _null_ n 0));

/* last_value_any(internal, anyarray) => anyarray */ 
DATA(insert OID = 7291 ( last_value_any  PGNSP PGUID 12 f f t f i 2 2277 t "2281 2277" _null_ _null_ _null_ last_value_generic _null_ _null_ n 0));

/* cume_dist_prelim(internal) => int8 */ 
DATA(insert OID = 7204 ( cume_dist_prelim  PGNSP PGUID 12 f f t f i 1 20 f "2281" _null_ _null_ _null_ cume_dist_prelim _null_ _null_ n 0));
DESCR("window preliminary function");


#define CUME_DIST_PRELIM_OID 7204
#define CUME_DIST_PRELIM_TYPE 20
/* ntile_prelim_int(internal, int4) => _int8 */ 
DATA(insert OID = 7205 ( ntile_prelim_int  PGNSP PGUID 12 f f t f i 2 1016 f "2281 23" _null_ _null_ _null_ ntile_prelim_int _null_ _null_ n 0));
DESCR("window preliminary function");

/* ntile_prelim_bigint(internal, int8) => _int8 */ 
DATA(insert OID = 7206 ( ntile_prelim_bigint  PGNSP PGUID 12 f f t f i 2 1016 f "2281 20" _null_ _null_ _null_ ntile_prelim_bigint _null_ _null_ n 0));
DESCR("window preliminary function");

/* ntile_prelim_numeric(internal, "numeric") => _int8 */ 
DATA(insert OID = 7207 ( ntile_prelim_numeric  PGNSP PGUID 12 f f t f i 2 1016 f "2281 1700" _null_ _null_ _null_ ntile_prelim_numeric _null_ _null_ n 0));
DESCR("window preliminary function");

/* percent_rank_final(int8, int8) => float8 */ 
DATA(insert OID = 7303 ( percent_rank_final  PGNSP PGUID 12 f f t f i 2 701 f "20 20" _null_ _null_ _null_ percent_rank_final _null_ _null_ n 0));
DESCR("window final function");

/* cume_dist_final(int8, int8) => float8 */ 
DATA(insert OID = 7304 ( cume_dist_final  PGNSP PGUID 12 f f t f i 2 701 f "20 20" _null_ _null_ _null_ cume_dist_final _null_ _null_ n 0));
DESCR("window final function");

/* ntile_final(_int8, int8) => int8 */ 
DATA(insert OID = 7305 ( ntile_final  PGNSP PGUID 12 f f t f i 2 20 f "1016 20" _null_ _null_ _null_ ntile_final _null_ _null_ n 0));
DESCR("window final function");

/* get_ao_distribution(IN reloid oid, OUT segmentid int4, OUT tupcount float8) => SETOF pg_catalog.record */ 
DATA(insert OID = 7169 ( get_ao_distribution  PGNSP PGUID 12 f f f t v 1 2249 f "26" "{26,23,701}" "{i,o,o}" "{reloid,segmentid,tupcount}" get_ao_distribution_oid _null_ _null_ r 0));
DESCR("show append only table tuple distribution across segment databases");

/* get_ao_distribution(IN relname text, OUT segmentid int4, OUT tupcount float8) => SETOF pg_catalog.record */ 
DATA(insert OID = 7170 ( get_ao_distribution  PGNSP PGUID 12 f f f t v 1 2249 f "25" "{25,23,701}" "{i,o,o}" "{relname,segmentid,tupcount}" get_ao_distribution_name _null_ _null_ r 0));
DESCR("show append only table tuple distribution across segment databases");

/* get_ao_compression_ratio(oid) => float8 */ 
DATA(insert OID = 7171 ( get_ao_compression_ratio  PGNSP PGUID 12 f f f f v 1 701 f "26" _null_ _null_ _null_ get_ao_compression_ratio_oid _null_ _null_ r 0));
DESCR("show append only table compression ratio");

/* get_ao_compression_ratio(text) => float8 */ 
DATA(insert OID = 7172 ( get_ao_compression_ratio  PGNSP PGUID 12 f f f f v 1 701 f "25" _null_ _null_ _null_ get_ao_compression_ratio_name _null_ _null_ r 0));
DESCR("show append only table compression ratio");

/* gp_update_ao_master_stats(oid) => float8 */ 
DATA(insert OID = 7173 ( gp_update_ao_master_stats  PGNSP PGUID 12 f f f f v 1 701 f "26" _null_ _null_ _null_ gp_update_ao_master_stats_oid _null_ _null_ m 0));
DESCR("append only tables utility function");

/* gp_update_ao_master_stats(text) => float8 */ 
DATA(insert OID = 7174 ( gp_update_ao_master_stats  PGNSP PGUID 12 f f f f v 1 701 f "25" _null_ _null_ _null_ gp_update_ao_master_stats_name _null_ _null_ m 0));
DESCR("append only tables utility function");

/* gp_persistent_build_db(bool) => int4 */ 
DATA(insert OID = 7178 ( gp_persistent_build_db  PGNSP PGUID 12 f f f f v 1 23 f "16" _null_ _null_ _null_ gp_persistent_build_db _null_ _null_ n 0));
DESCR("populate the persistent tables and gp_relation_node for the current database");

/* gp_persistent_build_all(bool) => int4 */ 
DATA(insert OID = 7179 ( gp_persistent_build_all  PGNSP PGUID 12 f f f f v 1 23 f "16" _null_ _null_ _null_ gp_persistent_build_all _null_ _null_ n 0));
DESCR("populate the persistent tables and gp_relation_node for the whole database instance");

/* gp_persistent_reset_all() => int4 */ 
DATA(insert OID = 7180 ( gp_persistent_reset_all  PGNSP PGUID 12 f f f f v 0 23 f "" _null_ _null_ _null_ gp_persistent_reset_all _null_ _null_ n 0));
DESCR("Remove the persistent tables and gp_relation_node for the whole database instance");

/* gp_persistent_repair_delete(int4, tid) => int4 */ 
DATA(insert OID = 7181 ( gp_persistent_repair_delete  PGNSP PGUID 12 f f f f v 2 23 f "23 27" _null_ _null_ _null_ gp_persistent_repair_delete _null_ _null_ n 0));
DESCR("Remove an entry specified by TID from a persistent table for the current database instance");

/* gp_persistent_set_relation_bufpool_kind_all() => int4 */ 
DATA(insert OID = 7182 ( gp_persistent_set_relation_bufpool_kind_all  PGNSP PGUID 12 f f f f v 0 23 f "" _null_ _null_ _null_ gp_persistent_set_relation_bufpool_kind_all _null_ _null_ n 0));
DESCR("Populate the gp_persistent_relation_node table's relation_bufpool_kind column for the whole database instance for upgrade from 4.0 to 4.1");


/* GIN array support */
/* ginarrayextract(anyarray, internal) => internal */ 
DATA(insert OID = 2743 ( ginarrayextract  PGNSP PGUID 12 f f t f i 2 2281 f "2277 2281" _null_ _null_ _null_ ginarrayextract _null_ _null_ n 0));
DESCR("GIN array support");

/* ginarrayconsistent(internal, int2, internal) => bool */ 
DATA(insert OID = 2744 ( ginarrayconsistent  PGNSP PGUID 12 f f t f i 3 16 f "2281 21 2281" _null_ _null_ _null_ ginarrayconsistent _null_ _null_ n 0));
DESCR("GIN array support");


/* overlap/contains/contained */
/* arrayoverlap(anyarray, anyarray) => bool */ 
DATA(insert OID = 2747 ( arrayoverlap  PGNSP PGUID 12 f f t f i 2 16 f "2277 2277" _null_ _null_ _null_ arrayoverlap _null_ _null_ n 0));
DESCR("overlaps");

/* arraycontains(anyarray, anyarray) => bool */ 
DATA(insert OID = 2748 ( arraycontains  PGNSP PGUID 12 f f t f i 2 16 f "2277 2277" _null_ _null_ _null_ arraycontains _null_ _null_ n 0));
DESCR("contains");

/* arraycontained(anyarray, anyarray) => bool */ 
DATA(insert OID = 2749 ( arraycontained  PGNSP PGUID 12 f f t f i 2 16 f "2277 2277" _null_ _null_ _null_ arraycontained _null_ _null_ n 0));
DESCR("is contained by");


/* userlock replacements */
/* pg_advisory_lock(int8) => void */ 
DATA(insert OID = 2880 ( pg_advisory_lock  PGNSP PGUID 12 f f t f v 1 2278 f "20" _null_ _null_ _null_ pg_advisory_lock_int8 _null_ _null_ n 0));
DESCR("obtain exclusive advisory lock");

/* pg_advisory_lock_shared(int8) => void */ 
DATA(insert OID = 2881 ( pg_advisory_lock_shared  PGNSP PGUID 12 f f t f v 1 2278 f "20" _null_ _null_ _null_ pg_advisory_lock_shared_int8 _null_ _null_ n 0));
DESCR("obtain shared advisory lock");

/* pg_try_advisory_lock(int8) => bool */ 
DATA(insert OID = 2882 ( pg_try_advisory_lock  PGNSP PGUID 12 f f t f v 1 16 f "20" _null_ _null_ _null_ pg_try_advisory_lock_int8 _null_ _null_ n 0));
DESCR("obtain exclusive advisory lock if available");

/* pg_try_advisory_lock_shared(int8) => bool */ 
DATA(insert OID = 2883 ( pg_try_advisory_lock_shared  PGNSP PGUID 12 f f t f v 1 16 f "20" _null_ _null_ _null_ pg_try_advisory_lock_shared_int8 _null_ _null_ n 0));
DESCR("obtain shared advisory lock if available");

/* pg_advisory_unlock(int8) => bool */ 
DATA(insert OID = 2884 ( pg_advisory_unlock  PGNSP PGUID 12 f f t f v 1 16 f "20" _null_ _null_ _null_ pg_advisory_unlock_int8 _null_ _null_ n 0));
DESCR("release exclusive advisory lock");

/* pg_advisory_unlock_shared(int8) => bool */ 
DATA(insert OID = 2885 ( pg_advisory_unlock_shared  PGNSP PGUID 12 f f t f v 1 16 f "20" _null_ _null_ _null_ pg_advisory_unlock_shared_int8 _null_ _null_ n 0));
DESCR("release shared advisory lock");

/* pg_advisory_lock(int4, int4) => void */ 
DATA(insert OID = 2886 ( pg_advisory_lock  PGNSP PGUID 12 f f t f v 2 2278 f "23 23" _null_ _null_ _null_ pg_advisory_lock_int4 _null_ _null_ n 0));
DESCR("obtain exclusive advisory lock");

/* pg_advisory_lock_shared(int4, int4) => void */ 
DATA(insert OID = 2887 ( pg_advisory_lock_shared  PGNSP PGUID 12 f f t f v 2 2278 f "23 23" _null_ _null_ _null_ pg_advisory_lock_shared_int4 _null_ _null_ n 0));
DESCR("obtain shared advisory lock");

/* pg_try_advisory_lock(int4, int4) => bool */ 
DATA(insert OID = 2888 ( pg_try_advisory_lock  PGNSP PGUID 12 f f t f v 2 16 f "23 23" _null_ _null_ _null_ pg_try_advisory_lock_int4 _null_ _null_ n 0));
DESCR("obtain exclusive advisory lock if available");

/* pg_try_advisory_lock_shared(int4, int4) => bool */ 
DATA(insert OID = 2889 ( pg_try_advisory_lock_shared  PGNSP PGUID 12 f f t f v 2 16 f "23 23" _null_ _null_ _null_ pg_try_advisory_lock_shared_int4 _null_ _null_ n 0));
DESCR("obtain shared advisory lock if available");

/* pg_advisory_unlock(int4, int4) => bool */ 
DATA(insert OID = 2890 ( pg_advisory_unlock  PGNSP PGUID 12 f f t f v 2 16 f "23 23" _null_ _null_ _null_ pg_advisory_unlock_int4 _null_ _null_ n 0));
DESCR("release exclusive advisory lock");

/* pg_advisory_unlock_shared(int4, int4) => bool */ 
DATA(insert OID = 2891 ( pg_advisory_unlock_shared  PGNSP PGUID 12 f f t f v 2 16 f "23 23" _null_ _null_ _null_ pg_advisory_unlock_shared_int4 _null_ _null_ n 0));
DESCR("release shared advisory lock");

/* pg_advisory_unlock_all() => void */ 
DATA(insert OID = 2892 ( pg_advisory_unlock_all  PGNSP PGUID 12 f f t f v 0 2278 f "" _null_ _null_ _null_ pg_advisory_unlock_all _null_ _null_ n 0));
DESCR("release all advisory locks");


/* xml functions */
/* xml_in(cstring) => xml */ 
DATA(insert OID = 2973 ( xml_in  PGNSP PGUID 12 f f t f i 1 142 f "2275" _null_ _null_ _null_ xml_in _null_ _null_ n 0));
DESCR("I/O");

/* xml_out(xml) => cstring */ 
DATA(insert OID = 2974 ( xml_out  PGNSP PGUID 12 f f t f i 1 2275 f "142" _null_ _null_ _null_ xml_out _null_ _null_ n 0));
DESCR("I/O");

/* xmlcomment(text) => xml */ 
DATA(insert OID = 2975 ( xmlcomment  PGNSP PGUID 12 f f t f i 1 142 f "25" _null_ _null_ _null_ xmlcomment _null_ _null_ n 0));
DESCR("generate XML comment");

/* xml(text) => xml */ 
DATA(insert OID = 2976 ( xml  PGNSP PGUID 12 f f t f i 1 142 f "25" _null_ _null_ _null_ texttoxml _null_ _null_ n 0));
DESCR("perform a non-validating parse of a character string to produce an XML value");

/* xmlvalidate(xml, text) => bool */ 
DATA(insert OID = 2977 ( xmlvalidate  PGNSP PGUID 12 f f t f i 2 16 f "142 25" _null_ _null_ _null_ xmlvalidate _null_ _null_ n 0));
DESCR("validate an XML value");

/* xml_recv(internal) => xml */ 
DATA(insert OID = 2978 ( xml_recv  PGNSP PGUID 12 f f t f i 1 142 f "2281" _null_ _null_ _null_ xml_recv _null_ _null_ n 0));
DESCR("I/O");

/* xml_send(xml) => bytea */ 
DATA(insert OID = 2979 ( xml_send  PGNSP PGUID 12 f f t f i 1 17 f "142" _null_ _null_ _null_ xml_send _null_ _null_ n 0));
DESCR("I/O");

/* xmlconcat2(xml, xml) => xml */ 
DATA(insert OID = 2980 ( xmlconcat2  PGNSP PGUID 12 f f t f i 2 142 f "142 142" _null_ _null_ _null_ xmlconcat2 _null_ _null_ n 0));
DESCR("aggregate transition function");

/* xmlagg(xml) => xml */ 
DATA(insert OID = 2981 ( xmlagg  PGNSP PGUID 12 t f f f i 1 142 f "142" _null_ _null_ _null_ aggregate_dummy _null_ _null_ n 0));
DESCR("concatenate XML values");

/* text(xml) => text */ 
DATA(insert OID = 2982 ( text  PGNSP PGUID 12 f f t f i 1 25 f "142" _null_ _null_ _null_ xmltotext _null_ _null_ n 0));
DESCR("serialize an XML value to a character string");

/* xpath(text, xml, _text) => _xml */ 
DATA(insert OID = 2983 ( xpath  PGNSP PGUID 12 f f t f i 3 143 f "25 142 1009" _null_ _null_ _null_ xpath _null_ _null_ n 0));
DESCR("evaluate XPath expression, with namespaces support");

/* xpath(text, xml) => _xml */ 
DATA(insert OID = 2984 ( xpath  PGNSP PGUID 14 f f t f i 2 143 f "25 142" _null_ _null_ _null_ "select pg_catalog.xpath($1, $2, ''{}''::pg_catalog.text[])" _null_ _null_ c 0));
DESCR("evaluate XPath expression");

/* xmlexists(text, xml) => bool */ 
DATA(insert OID = 2985 ( xmlexists  PGNSP PGUID 12 f f t f i 2 16 f "25 142" _null_ _null_ _null_ xmlexists _null_ _null_ n 0));
DESCR("test XML value against XPath expression");

/* xpath_exists(text, xml, _text) => bool */ 
DATA(insert OID = 2986 ( xpath_exists  PGNSP PGUID 12 f f t f i 3 16 f "25 142 1009" _null_ _null_ _null_ xpath_exists _null_ _null_ n 0));
DESCR("test XML value against XPath expression, with namespace support");

/* xpath_exists(text, xml) => bool */ 
DATA(insert OID = 2987 ( xpath_exists  PGNSP PGUID 14 f f t f i 2 16 f "25 142" _null_ _null_ _null_ "select pg_catalog.xpath_exists($1, $2, ''{}''::pg_catalog.text[])" _null_ _null_ c 0));
DESCR("test XML value against XPath expression");

/* xml_is_well_formed(text) => bool */ 
DATA(insert OID = 2988 ( xml_is_well_formed  PGNSP PGUID 12 f f t f i 1 16 f "25" _null_ _null_ _null_ xml_is_well_formed _null_ _null_ n 0));
DESCR("determine if a string is well formed XML");

/* xml_is_well_formed_document(text) => bool */ 
DATA(insert OID = 2989 ( xml_is_well_formed_document  PGNSP PGUID 12 f f t f i 1 16 f "25" _null_ _null_ _null_ xml_is_well_formed_document _null_ _null_ n 0));
DESCR("determine if a string is well formed XML document");

/* xml_is_well_formed_content(text) => bool */ 
DATA(insert OID = 2990 ( xml_is_well_formed_content  PGNSP PGUID 12 f f t f i 1 16 f "25" _null_ _null_ _null_ xml_is_well_formed_content _null_ _null_ n 0));
DESCR("determine if a string is well formed XML content");


/* the bitmap index access method routines */
/* bmgettuple(internal, internal) => bool */ 
DATA(insert OID = 3050 ( bmgettuple  PGNSP PGUID 12 f f t f v 2 16 f "2281 2281" _null_ _null_ _null_ bmgettuple _null_ _null_ n 0));
DESCR("bitmap(internal)");

/* bmgetmulti(internal, internal) => internal */ 
DATA(insert OID = 3051 ( bmgetmulti  PGNSP PGUID 12 f f t f v 2 2281 f "2281 2281" _null_ _null_ _null_ bmgetmulti _null_ _null_ n 0));
DESCR("bitmap(internal)");

/* bminsert(internal, internal, internal, internal, internal, internal) => bool */ 
DATA(insert OID = 3001 ( bminsert  PGNSP PGUID 12 f f t f v 6 16 f "2281 2281 2281 2281 2281 2281" _null_ _null_ _null_ bminsert _null_ _null_ n 0));
DESCR("bitmap(internal)");

/* bmbeginscan(internal, internal, internal) => internal */ 
DATA(insert OID = 3002 ( bmbeginscan  PGNSP PGUID 12 f f t f v 3 2281 f "2281 2281 2281" _null_ _null_ _null_ bmbeginscan _null_ _null_ n 0));
DESCR("bitmap(internal)");

/* bmrescan(internal, internal) => void */ 
DATA(insert OID = 3003 ( bmrescan  PGNSP PGUID 12 f f t f v 2 2278 f "2281 2281" _null_ _null_ _null_ bmrescan _null_ _null_ n 0));
DESCR("bitmap(internal)");

/* bmendscan(internal) => void */ 
DATA(insert OID = 3004 ( bmendscan  PGNSP PGUID 12 f f t f v 1 2278 f "2281" _null_ _null_ _null_ bmendscan _null_ _null_ n 0));
DESCR("bitmap(internal)");

/* bmmarkpos(internal) => void */ 
DATA(insert OID = 3005 ( bmmarkpos  PGNSP PGUID 12 f f t f v 1 2278 f "2281" _null_ _null_ _null_ bmmarkpos _null_ _null_ n 0));
DESCR("bitmap(internal)");

/* bmrestrpos(internal) => void */ 
DATA(insert OID = 3006 ( bmrestrpos  PGNSP PGUID 12 f f t f v 1 2278 f "2281" _null_ _null_ _null_ bmrestrpos _null_ _null_ n 0));
DESCR("bitmap(internal)");

/* bmbuild(internal, internal, internal) => internal */ 
DATA(insert OID = 3007 ( bmbuild  PGNSP PGUID 12 f f t f v 3 2281 f "2281 2281 2281" _null_ _null_ _null_ bmbuild _null_ _null_ n 0));
DESCR("bitmap(internal)");

/* bmbulkdelete(internal, internal, internal, internal) => internal */ 
DATA(insert OID = 3008 ( bmbulkdelete  PGNSP PGUID 12 f f t f v 4 2281 f "2281 2281 2281 2281" _null_ _null_ _null_ bmbulkdelete _null_ _null_ n 0));
DESCR("bitmap(internal)");

/* bmvacuumcleanup(internal, internal) => internal */ 
DATA(insert OID = 3009 ( bmvacuumcleanup  PGNSP PGUID 12 f f t f v 2 2281 f "2281 2281" _null_ _null_ _null_ bmvacuumcleanup _null_ _null_ n 0));

/* bmcostestimate(internal, internal, internal, internal, internal, internal, internal, internal) => void */ 
DATA(insert OID = 3010 ( bmcostestimate  PGNSP PGUID 12 f f t f v 8 2278 f "2281 2281 2281 2281 2281 2281 2281 2281" _null_ _null_ _null_ bmcostestimate _null_ _null_ n 0));
DESCR("bitmap(internal)");

/* bmoptions(_text, bool) => bytea */ 
DATA(insert OID = 3011 ( bmoptions  PGNSP PGUID 12 f f t f s 2 17 f "1009 16" _null_ _null_ _null_ bmoptions _null_ _null_ n 0));
DESCR("btree(internal)");


/* AOCS functions. */
/* aocsvpinfo_decode(varbit, int4, int4) => int8 */ 
DATA(insert OID = 9900 ( aocsvpinfo_decode  PGNSP PGUID 12 f f t f i 3 20 f "1562 23 23" _null_ _null_ _null_ aocsvpinfo_decode _null_ _null_ n 0));


/* raises deprecation error */
/* gp_deprecated() => void */ 
DATA(insert OID = 9997 ( gp_deprecated  PGNSP PGUID 12 f f f f i 0 2278 f "" _null_ _null_ _null_ gp_deprecated _null_ _null_ n 0));
DESCR("raises function deprecation error");


/* A convenient utility */
/* pg_objname_to_oid(text) => oid */ 
DATA(insert OID = 9998 ( pg_objname_to_oid  PGNSP PGUID 12 f f t f i 1 26 f "25" _null_ _null_ _null_ pg_objname_to_oid _null_ _null_ n 0));
DESCR("convert an object name to oid");


/* Fault injection */
/* gp_fault_inject(int4, int8) => int8 */ 
DATA(insert OID = 9999 ( gp_fault_inject  PGNSP PGUID 12 f f t f v 2 20 f "23 20" _null_ _null_ _null_ gp_fault_inject _null_ _null_ n 0));
DESCR("Greenplum fault testing only");


/* Analyze related */
/* gp_statistics_estimate_reltuples_relpages_oid(oid) => _float4 */ 
DATA(insert OID = 5032 ( gp_statistics_estimate_reltuples_relpages_oid  PGNSP PGUID 12 f f t f v 1 1021 f "26" _null_ _null_ _null_ gp_statistics_estimate_reltuples_relpages_oid _null_ _null_ n 0));
DESCR("Return reltuples/relpages information for relation.");


/* Backoff related */
/* gp_adjust_priority(int4, int4, int4) => int4 */ 
DATA(insert OID = 5040 ( gp_adjust_priority  PGNSP PGUID 12 f f t f v 3 23 f "23 23 23" _null_ _null_ _null_ gp_adjust_priority_int _null_ _null_ n 0));
DESCR("change weight of all the backends for a given session id");

/* gp_adjust_priority(int4, int4, text) => int4 */ 
DATA(insert OID = 5041 ( gp_adjust_priority  PGNSP PGUID 12 f f t f v 3 23 f "23 23 25" _null_ _null_ _null_ gp_adjust_priority_value _null_ _null_ n 0));
DESCR("change weight of all the backends for a given session id");

/* gp_list_backend_priorities() => SETOF record */ 
DATA(insert OID = 5042 ( gp_list_backend_priorities  PGNSP PGUID 12 f f f t v 0 2249 f "" _null_ _null_ _null_ gp_list_backend_priorities _null_ _null_ n 0));
DESCR("list priorities of backends");


/* elog related */
/* gp_elog(text) => void */ 
DATA(insert OID = 5044 ( gp_elog  PGNSP PGUID 12 f f t f i 1 2278 f "25" _null_ _null_ _null_ gp_elog _null_ _null_ n 0));
DESCR("Insert text into the error log");

/* gp_elog(text, bool) => void */ 
DATA(insert OID = 5045 ( gp_elog  PGNSP PGUID 12 f f t f i 2 2278 f "25 16" _null_ _null_ _null_ gp_elog _null_ _null_ n 0));
DESCR("Insert text into the error log");


/* Segment and master administration functions, see utils/gp/segadmin.c */
/* gp_add_master_standby(text, text, _text) => int2 */ 
DATA(insert OID = 5046 ( gp_add_master_standby  PGNSP PGUID 12 f f f f v 3 21 f "25 25 1009" _null_ _null_ _null_ gp_add_master_standby _null_ _null_ n 0));
DESCR("Perform the catalog operations necessary for adding a new standby");

/* gp_add_master_standby(text, text, _text, int4) => int2 */ 
DATA(insert OID = 5038 ( gp_add_master_standby  PGNSP PGUID 12 f f f f v 4 21 f "25 25 1009 23" _null_ _null_ _null_ gp_add_master_standby_port _null_ _null_ n 0));
DESCR("Perform the catalog operations necessary for adding a new standby");

/* gp_remove_master_standby() => bool */ 
DATA(insert OID = 5047 ( gp_remove_master_standby  PGNSP PGUID 12 f f f f v 0 16 f "" _null_ _null_ _null_ gp_remove_master_standby _null_ _null_ n 0));
DESCR("Remove a master standby from the system catalog");

/* gp_add_segment_mirror(int2, text, text, int4, int4, _text) => int2 */ 
DATA(insert OID = 5048 ( gp_add_segment_mirror  PGNSP PGUID 12 f f f f v 6 21 f "21 25 25 23 23 1009" _null_ _null_ _null_ gp_add_segment_mirror _null_ _null_ n 0));
DESCR("Perform the catalog operations necessary for adding a new segment mirror");

/* gp_remove_segment_mirror(int2) => bool */ 
DATA(insert OID = 5049 ( gp_remove_segment_mirror  PGNSP PGUID 12 f f f f v 1 16 f "21" _null_ _null_ _null_ gp_remove_segment_mirror _null_ _null_ n 0));
DESCR("Remove a segment mirror from the system catalog");

/* gp_add_segment(text, text, int4, _text) => int2 */ 
DATA(insert OID = 5050 ( gp_add_segment  PGNSP PGUID 12 f f f f v 4 21 f "25 25 23 1009" _null_ _null_ _null_ gp_add_segment _null_ _null_ n 0));
DESCR("Perform the catalog operations necessary for adding a new primary segment");

/* gp_remove_segment(int2) => bool */ 
DATA(insert OID = 5051 ( gp_remove_segment  PGNSP PGUID 12 f f f f v 1 16 f "21" _null_ _null_ _null_ gp_remove_segment _null_ _null_ n 0));
DESCR("Remove a primary segment from the system catalog");

/* gp_prep_new_segment(_text) => bool */ 
DATA(insert OID = 5052 ( gp_prep_new_segment  PGNSP PGUID 12 f f f f v 1 16 f "1009" _null_ _null_ _null_ gp_prep_new_segment _null_ _null_ n 0));
DESCR("Convert a cloned master catalog for use as a segment");

/* gp_activate_standby() => bool */ 
DATA(insert OID = 5053 ( gp_activate_standby  PGNSP PGUID 12 f f f f v 0 16 f "" _null_ _null_ _null_ gp_activate_standby _null_ _null_ n 0));
DESCR("Activate a standby");


/* We cheat in the following two functions: they are technically volatile but */
/* we can only dispatch them if they're immutable :(. */
/* gp_add_segment_persistent_entries(int2, int2, _text) => bool */ 
DATA(insert OID = 5054 ( gp_add_segment_persistent_entries  PGNSP PGUID 12 f f f f i 3 16 f "21 21 1009" _null_ _null_ _null_ gp_add_segment_persistent_entries _null_ _null_ n 0));
DESCR("Persist object nodes on a segment");

/* gp_remove_segment_persistent_entries(int2, int2) => bool */ 
DATA(insert OID = 5055 ( gp_remove_segment_persistent_entries  PGNSP PGUID 12 f f f f i 2 16 f "21 21" _null_ _null_ _null_ gp_remove_segment_persistent_entries _null_ _null_ n 0));
DESCR("Remove persistent object node references at a segment");


/* persistent table repair functions */
/* gp_add_persistent_filespace_node_entry(tid, oid, int2, text, int2, text, int2, int8, int2, int4, int4, int8, tid) => bool */ 
DATA(insert OID = 5056 ( gp_add_persistent_filespace_node_entry  PGNSP PGUID 12 f f f f v 13 16 f "27 26 21 25 21 25 21 20 21 23 23 20 27" _null_ _null_ _null_ gp_add_persistent_filespace_node_entry _null_ _null_ n 0));
DESCR("Add a new entry to gp_persistent_filespace_node");

/* gp_add_persistent_tablespace_node_entry(tid, oid, oid, int2, int8, int2, int4, int4, int8, tid) => bool */ 
DATA(insert OID = 5057 ( gp_add_persistent_tablespace_node_entry  PGNSP PGUID 12 f f f f v 10 16 f "27 26 26 21 20 21 23 23 20 27" _null_ _null_ _null_ gp_add_persistent_tablespace_node_entry _null_ _null_ n 0));
DESCR("Add a new entry to gp_persistent_tablespace_node");

/* gp_add_persistent_database_node_entry(tid, oid, oid, int2, int8, int2, int4, int4, int8, tid) => bool */ 
DATA(insert OID = 5058 ( gp_add_persistent_database_node_entry  PGNSP PGUID 12 f f f f v 10 16 f "27 26 26 21 20 21 23 23 20 27" _null_ _null_ _null_ gp_add_persistent_database_node_entry _null_ _null_ n 0));
DESCR("Add a new entry to gp_persistent_database_node");

/* gp_add_persistent_relation_node_entry(tid, oid, oid, oid, int4, int2, int2, int8, int2, int2, bool, int8, gpxlogloc, int4, int8, int8, int4, int4, int8, tid) => bool */ 
DATA(insert OID = 5059 ( gp_add_persistent_relation_node_entry  PGNSP PGUID 12 f f f f v 20 16 f "27 26 26 26 23 21 21 20 21 21 16 20 3310 23 20 20 23 23 20 27" _null_ _null_ _null_ gp_add_persistent_relation_node_entry _null_ _null_ n 0));
DESCR("Add a new entry to gp_persistent_relation_node");

/* gp_add_global_sequence_entry(tid, int8) => bool */ 
DATA(insert OID = 5060 ( gp_add_global_sequence_entry  PGNSP PGUID 12 f f f f v 2 16 f "27 20" _null_ _null_ _null_ gp_add_global_sequence_entry _null_ _null_ n 0));
DESCR("Add a new entry to gp_global_sequence");

/* gp_add_relation_node_entry(tid, oid, int4, int8, tid, int8) => bool */ 
DATA(insert OID = 5061 ( gp_add_relation_node_entry  PGNSP PGUID 12 f f f f v 6 16 f "27 26 23 20 27 20" _null_ _null_ _null_ gp_add_relation_node_entry _null_ _null_ n 0));
DESCR("Add a new entry to gp_relation_node");

/* gp_update_persistent_filespace_node_entry(tid, oid, int2, text, int2, text, int2, int8, int2, int4, int4, int8, tid) => bool */ 
DATA(insert OID = 5062 ( gp_update_persistent_filespace_node_entry  PGNSP PGUID 12 f f f f v 13 16 f "27 26 21 25 21 25 21 20 21 23 23 20 27" _null_ _null_ _null_ gp_update_persistent_filespace_node_entry _null_ _null_ n 0));
DESCR("Update an entry in gp_persistent_filespace_node");

/* gp_update_persistent_tablespace_node_entry(tid, oid, oid, int2, int8, int2, int4, int4, int8, tid) => bool */ 
DATA(insert OID = 5063 ( gp_update_persistent_tablespace_node_entry  PGNSP PGUID 12 f f f f v 10 16 f "27 26 26 21 20 21 23 23 20 27" _null_ _null_ _null_ gp_update_persistent_tablespace_node_entry _null_ _null_ n 0));
DESCR("Update an entry in gp_persistent_tablespace_node");

/* gp_update_persistent_database_node_entry(tid, oid, oid, int2, int8, int2, int4, int4, int8, tid) => bool */ 
DATA(insert OID = 5064 ( gp_update_persistent_database_node_entry  PGNSP PGUID 12 f f f f v 10 16 f "27 26 26 21 20 21 23 23 20 27" _null_ _null_ _null_ gp_update_persistent_database_node_entry _null_ _null_ n 0));
DESCR("Update an entry in gp_persistent_database_node");

/* gp_update_persistent_relation_node_entry(tid, oid, oid, oid, int4, int2, int2, int8, int2, int2, bool, int8, gpxlogloc, int4, int8, int8, int4, int4, int8, tid) => bool */ 
DATA(insert OID = 5065 ( gp_update_persistent_relation_node_entry  PGNSP PGUID 12 f f f f v 20 16 f "27 26 26 26 23 21 21 20 21 21 16 20 3310 23 20 20 23 23 20 27" _null_ _null_ _null_ gp_update_persistent_relation_node_entry _null_ _null_ n 0));
DESCR("Update an entry in gp_persistent_relation_node");

/* gp_update_global_sequence_entry(tid, int8) => bool */ 
DATA(insert OID = 5066 ( gp_update_global_sequence_entry  PGNSP PGUID 12 f f f f v 2 16 f "27 20" _null_ _null_ _null_ gp_update_global_sequence_entry _null_ _null_ n 0));
DESCR("Update an entry in gp_global_sequence");

/* gp_update_relation_node_entry(tid, oid, int4, int8, tid, int8) => bool */ 
DATA(insert OID = 5067 ( gp_update_relation_node_entry  PGNSP PGUID 12 f f f f v 6 16 f "27 26 23 20 27 20" _null_ _null_ _null_ gp_update_relation_node_entry _null_ _null_ n 0));
DESCR("Update an entry in gp_relation_node");

/* gp_delete_persistent_filespace_node_entry(tid) => bool */ 
DATA(insert OID = 5068 ( gp_delete_persistent_filespace_node_entry  PGNSP PGUID 12 f f f f v 1 16 f "27" _null_ _null_ _null_ gp_delete_persistent_filespace_node_entry _null_ _null_ n 0));
DESCR("Remove an entry from gp_persistent_filespace_node");

/* gp_delete_persistent_tablespace_node_entry(tid) => bool */ 
DATA(insert OID = 5069 ( gp_delete_persistent_tablespace_node_entry  PGNSP PGUID 12 f f f f v 1 16 f "27" _null_ _null_ _null_ gp_delete_persistent_tablespace_node_entry _null_ _null_ n 0));
DESCR("Remove an entry from gp_persistent_tablespace_node");

/* gp_delete_persistent_database_node_entry(tid) => bool */ 
DATA(insert OID = 5070 ( gp_delete_persistent_database_node_entry  PGNSP PGUID 12 f f f f v 1 16 f "27" _null_ _null_ _null_ gp_delete_persistent_database_node_entry _null_ _null_ n 0));
DESCR("Remove an entry from gp_persistent_database_node");

/* gp_delete_persistent_relation_node_entry(tid) => bool */ 
DATA(insert OID = 5071 ( gp_delete_persistent_relation_node_entry  PGNSP PGUID 12 f f f f v 1 16 f "27" _null_ _null_ _null_ gp_delete_persistent_relation_node_entry _null_ _null_ n 0));
DESCR("Remove an entry from gp_persistent_relation_node");

/* gp_delete_global_sequence_entry(tid) => bool */ 
DATA(insert OID = 5072 ( gp_delete_global_sequence_entry  PGNSP PGUID 12 f f f f v 1 16 f "27" _null_ _null_ _null_ gp_delete_global_sequence_entry _null_ _null_ n 0));
DESCR("Remove an entry from gp_global_sequence");

/* gp_delete_relation_node_entry(tid) => bool */ 
DATA(insert OID = 5073 ( gp_delete_relation_node_entry  PGNSP PGUID 12 f f f f v 1 16 f "27" _null_ _null_ _null_ gp_delete_relation_node_entry _null_ _null_ n 0));
DESCR("Remove an entry from gp_relation_node");

/* gp_persistent_relation_node_check() => SETOF gp_persistent_relation_node */ 
DATA(insert OID = 5074 ( gp_persistent_relation_node_check  PGNSP PGUID 12 f f f t v 0 6990 f "" _null_ _null_ _null_ gp_persistent_relation_node_check _null_ _null_ n 0));
DESCR("physical filesystem information");

/* gp_dbspecific_ptcat_verification() => bool */ 
DATA(insert OID = 5075 ( gp_dbspecific_ptcat_verification  PGNSP PGUID 12 f f f f v 0 16 f "" _null_ _null_ _null_ gp_dbspecific_ptcat_verification _null_ _null_ n 0));
DESCR("perform database specific PersistentTables-Catalog verification");

/* gp_nondbspecific_ptcat_verification() => bool */ 
DATA(insert OID = 5080 ( gp_nondbspecific_ptcat_verification  PGNSP PGUID 12 f f f f v 0 16 f "" _null_ _null_ _null_ gp_nondbspecific_ptcat_verification _null_ _null_ n 0));
DESCR("perform non-database specific PersistentTables-Catalog verification");

/* cosh(float8) => float8 */ 
DATA(insert OID = 3539 ( cosh  PGNSP PGUID 12 f f f f i 1 701 f "701" _null_ _null_ _null_ dcosh _null_ _null_ n 0));
DESCR("Hyperbolic cosine function");

/* sinh(float8) => float8 */ 
DATA(insert OID = 3540 ( sinh  PGNSP PGUID 12 f f f f i 1 701 f "701" _null_ _null_ _null_ dsinh _null_ _null_ n 0));
DESCR("Hyperbolic sine function");

/* tanh(float8) => float8 */ 
DATA(insert OID = 3541 ( tanh  PGNSP PGUID 12 f f f f i 1 701 f "701" _null_ _null_ _null_ dtanh _null_ _null_ n 0));
DESCR("Hyperbolic tangent function");

/* anytable_in(cstring) => anytable */ 
DATA(insert OID = 3054 ( anytable_in  PGNSP PGUID 12 f f t f i 1 3053 f "2275" _null_ _null_ _null_ anytable_in _null_ _null_ n 0));
DESCR("anytable type serialization input function");

/* anytable_out(anytable) => cstring */ 
DATA(insert OID = 3055 ( anytable_out  PGNSP PGUID 12 f f t f i 1 2275 f "3053" _null_ _null_ _null_ anytable_out _null_ _null_ n 0));
DESCR("anytable type serialization output function");

/* gp_quicklz_constructor(internal, internal, bool) => internal */ 
DATA(insert OID = 5076 ( gp_quicklz_constructor  PGNSP PGUID 12 f f f f v 3 2281 f "2281 2281 16" _null_ _null_ _null_ quicklz_constructor _null_ _null_ n 0));
DESCR("quicklz constructor");

/* gp_quicklz_destructor(internal) => void */ 
DATA(insert OID = 5077 ( gp_quicklz_destructor  PGNSP PGUID 12 f f f f v 1 2278 f "2281" _null_ _null_ _null_ quicklz_destructor _null_ _null_ n 0));
DESCR("quicklz destructor");

/* gp_quicklz_compress(internal, int4, internal, int4, internal, internal) => void */ 
DATA(insert OID = 5078 ( gp_quicklz_compress  PGNSP PGUID 12 f f f f i 6 2278 f "2281 23 2281 23 2281 2281" _null_ _null_ _null_ quicklz_compress _null_ _null_ n 0));
DESCR("quicklz compressor");

/* gp_quicklz_decompress(internal, int4, internal, int4, internal, internal) => void */ 
DATA(insert OID = 5079 ( gp_quicklz_decompress  PGNSP PGUID 12 f f f f i 6 2278 f "2281 23 2281 23 2281 2281" _null_ _null_ _null_ quicklz_decompress _null_ _null_ n 0));
DESCR("quicklz decompressor");

/* gp_quicklz_validator(internal) => void */ 
DATA(insert OID = 9925 ( gp_quicklz_validator  PGNSP PGUID 12 f f f f i 1 2278 f "2281" _null_ _null_ _null_ quicklz_validator _null_ _null_ n 0));
DESCR("quicklz compression validator");

/* gp_zlib_constructor(internal, internal, bool) => internal */ 
DATA(insert OID = 9910 ( gp_zlib_constructor  PGNSP PGUID 12 f f f f v 3 2281 f "2281 2281 16" _null_ _null_ _null_ zlib_constructor _null_ _null_ n 0));
DESCR("zlib constructor");

/* gp_zlib_destructor(internal) => void */ 
DATA(insert OID = 9911 ( gp_zlib_destructor  PGNSP PGUID 12 f f f f v 1 2278 f "2281" _null_ _null_ _null_ zlib_destructor _null_ _null_ n 0));
DESCR("zlib destructor");

/* gp_zlib_compress(internal, int4, internal, int4, internal, internal) => void */ 
DATA(insert OID = 9912 ( gp_zlib_compress  PGNSP PGUID 12 f f f f i 6 2278 f "2281 23 2281 23 2281 2281" _null_ _null_ _null_ zlib_compress _null_ _null_ n 0));
DESCR("zlib compressor");

/* gp_zlib_decompress(internal, int4, internal, int4, internal, internal) => void */ 
DATA(insert OID = 9913 ( gp_zlib_decompress  PGNSP PGUID 12 f f f f i 6 2278 f "2281 23 2281 23 2281 2281" _null_ _null_ _null_ zlib_decompress _null_ _null_ n 0));
DESCR("zlib decompressor");

/* gp_zlib_validator(internal) => void */ 
DATA(insert OID = 9924 ( gp_zlib_validator  PGNSP PGUID 12 f f f f i 1 2278 f "2281" _null_ _null_ _null_ zlib_validator _null_ _null_ n 0));
DESCR("zlib compression validator");

/* gp_rle_type_constructor(internal, internal, bool) => internal */ 
DATA(insert OID = 9914 ( gp_rle_type_constructor  PGNSP PGUID 12 f f f f v 3 2281 f "2281 2281 16" _null_ _null_ _null_ rle_type_constructor _null_ _null_ n 0));
DESCR("Type specific RLE constructor");

/* gp_rle_type_destructor(internal) => void */ 
DATA(insert OID = 9915 ( gp_rle_type_destructor  PGNSP PGUID 12 f f f f v 1 2278 f "2281" _null_ _null_ _null_ rle_type_destructor _null_ _null_ n 0));
DESCR("Type specific RLE destructor");

/* gp_rle_type_compress(internal, int4, internal, int4, internal, internal) => void */ 
DATA(insert OID = 9916 ( gp_rle_type_compress  PGNSP PGUID 12 f f f f i 6 2278 f "2281 23 2281 23 2281 2281" _null_ _null_ _null_ rle_type_compress _null_ _null_ n 0));
DESCR("Type specific RLE compressor");

/* gp_rle_type_decompress(internal, int4, internal, int4, internal, internal) => void */ 
DATA(insert OID = 9917 ( gp_rle_type_decompress  PGNSP PGUID 12 f f f f i 6 2278 f "2281 23 2281 23 2281 2281" _null_ _null_ _null_ rle_type_decompress _null_ _null_ n 0));
DESCR("Type specific RLE decompressor");

/* gp_rle_type_validator(internal) => void */ 
DATA(insert OID = 9923 ( gp_rle_type_validator  PGNSP PGUID 12 f f f f i 1 2278 f "2281" _null_ _null_ _null_ rle_type_validator _null_ _null_ n 0));
DESCR("Type speific RLE compression validator");

/* gp_dummy_compression_constructor(internal, internal, bool) => internal */ 
DATA(insert OID = 3064 ( gp_dummy_compression_constructor  PGNSP PGUID 12 f f f f v 3 2281 f "2281 2281 16" _null_ _null_ _null_ dummy_compression_constructor _null_ _null_ n 0));
DESCR("Dummy compression destructor");

/* gp_dummy_compression_destructor(internal) => internal */ 
DATA(insert OID = 3065 ( gp_dummy_compression_destructor  PGNSP PGUID 12 f f f f v 1 2281 f "2281" _null_ _null_ _null_ dummy_compression_destructor _null_ _null_ n 0));
DESCR("Dummy compression destructor");

/* gp_dummy_compression_compress(internal, int4, internal, int4, internal, internal) => internal */ 
DATA(insert OID = 3066 ( gp_dummy_compression_compress  PGNSP PGUID 12 f f f f v 6 2281 f "2281 23 2281 23 2281 2281" _null_ _null_ _null_ dummy_compression_compress _null_ _null_ n 0));
DESCR("Dummy compression compressor");

/* gp_dummy_compression_decompress(internal, int4, internal, int4, internal, internal) => internal */ 
DATA(insert OID = 3067 ( gp_dummy_compression_decompress  PGNSP PGUID 12 f f f f v 6 2281 f "2281 23 2281 23 2281 2281" _null_ _null_ _null_ dummy_compression_decompress _null_ _null_ n 0));
DESCR("Dummy compression decompressor");

/* gp_dummy_compression_validator(internal) => internal */ 
DATA(insert OID = 3068 ( gp_dummy_compression_validator  PGNSP PGUID 12 f f f f v 1 2281 f "2281" _null_ _null_ _null_ dummy_compression_validator _null_ _null_ n 0));
DESCR("Dummy compression validator");

/* linear_interpolate( anyelement, anyelement, int8, anyelement, int8) => int8 */ 
DATA(insert OID = 6072 ( linear_interpolate  PGNSP PGUID 12 f f t f i 5 20 f "2283 2283 20 2283 20" _null_ _null_ _null_ linterp_int64 _null_ _null_ n 0));
DESCR("linear interpolation: x, x0,y0, x1,y1");

/* linear_interpolate( anyelement, anyelement, int4, anyelement, int4) => int4 */ 
DATA(insert OID = 6073 ( linear_interpolate  PGNSP PGUID 12 f f t f i 5 23 f "2283 2283 23 2283 23" _null_ _null_ _null_ linterp_int32 _null_ _null_ n 0));
DESCR("linear interpolation: x, x0,y0, x1,y1");

/* linear_interpolate( anyelement, anyelement, int2, anyelement, int2) => int2 */ 
DATA(insert OID = 6074 ( linear_interpolate  PGNSP PGUID 12 f f t f i 5 21 f "2283 2283 21 2283 21" _null_ _null_ _null_ linterp_int16 _null_ _null_ n 0));
DESCR("linear interpolation: x, x0,y0, x1,y1");

/* linear_interpolate( anyelement, anyelement, float8, anyelement, float8) => float8 */ 
DATA(insert OID = 6075 ( linear_interpolate  PGNSP PGUID 12 f f t f i 5 701 f "2283 2283 701 2283 701" _null_ _null_ _null_ linterp_float8 _null_ _null_ n 0));
DESCR("linear interpolation: x, x0,y0, x1,y1");

/* linear_interpolate( anyelement, anyelement, float4, anyelement, float4) => float4 */ 
DATA(insert OID = 6076 ( linear_interpolate  PGNSP PGUID 12 f f t f i 5 700 f "2283 2283 700 2283 700" _null_ _null_ _null_ linterp_float4 _null_ _null_ n 0));
DESCR("linear interpolation: x, x0,y0, x1,y1");

/* linear_interpolate( anyelement, anyelement, date, anyelement, date) => date */ 
DATA(insert OID = 6077 ( linear_interpolate  PGNSP PGUID 12 f f t f i 5 1082 f "2283 2283 1082 2283 1082" _null_ _null_ _null_ linterp_DateADT _null_ _null_ n 0));
DESCR("linear interpolation: x, x0,y0, x1,y1");

/* linear_interpolate( anyelement, anyelement, time, anyelement, time) => time */ 
DATA(insert OID = 6078 ( linear_interpolate  PGNSP PGUID 12 f f t f i 5 1083 f "2283 2283 1083 2283 1083" _null_ _null_ _null_ linterp_TimeADT _null_ _null_ n 0));
DESCR("linear interpolation: x, x0,y0, x1,y1");

/* linear_interpolate( anyelement, anyelement, timestamp, anyelement, timestamp) => timestamp */ 
DATA(insert OID = 6079 ( linear_interpolate  PGNSP PGUID 12 f f t f i 5 1114 f "2283 2283 1114 2283 1114" _null_ _null_ _null_ linterp_Timestamp _null_ _null_ n 0));
DESCR("linear interpolation: x, x0,y0, x1,y1");

/* linear_interpolate( anyelement, anyelement, timestamptz, anyelement, timestamptz) => timestamptz */ 
DATA(insert OID = 6080 ( linear_interpolate  PGNSP PGUID 12 f f t f i 5 1184 f "2283 2283 1184 2283 1184" _null_ _null_ _null_ linterp_TimestampTz _null_ _null_ n 0));
DESCR("linear interpolation: x, x0,y0, x1,y1");

/* linear_interpolate( anyelement, anyelement, "interval", anyelement, "interval") => "interval" */ 
DATA(insert OID = 6081 ( linear_interpolate  PGNSP PGUID 12 f f t f i 5 1186 f "2283 2283 1186 2283 1186" _null_ _null_ _null_ linterp_Interval _null_ _null_ n 0));
DESCR("linear interpolation: x, x0,y0, x1,y1");

/* linear_interpolate( anyelement, anyelement, "numeric", anyelement, "numeric") => "numeric" */ 
DATA(insert OID = 6082 ( linear_interpolate  PGNSP PGUID 12 f f t f i 5 1700 f "2283 2283 1700 2283 1700" _null_ _null_ _null_ linterp_Numeric _null_ _null_ n 0));
DESCR("linear interpolation: x, x0,y0, x1,y1");

/* lead_lag_frame_maker(internal) => internal */ 
DATA(insert OID = 5081 ( lead_lag_frame_maker  PGNSP PGUID 12 f f t f v 1 2281 f "2281" _null_ _null_ _null_ lead_lag_frame_maker _null_ _null_ n 0));

/* gp_partition_propagation(int4, oid) => void */ 
DATA(insert OID = 6083 ( gp_partition_propagation  PGNSP PGUID 12 f f t f v 2 2278 f "23 26" _null_ _null_ _null_ gp_partition_propagation _null_ _null_ n 0));
DESCR("inserts a partition oid into specified pid-index");

/* gp_partition_selection(oid, anyelement) => oid */ 
DATA(insert OID = 6084 ( gp_partition_selection  PGNSP PGUID 12 f f t f s 2 26 f "26 2283" _null_ _null_ _null_ gp_partition_selection _null_ _null_ n 0));
DESCR("selects the child partition oid which satisfies a given partition key value");

/* gp_partition_expansion(oid) => setof oid */ 
DATA(insert OID = 6085 ( gp_partition_expansion  PGNSP PGUID 12 f f t t s 1 26 f "26" _null_ _null_ _null_ gp_partition_expansion _null_ _null_ n 0));
DESCR("finds all child partition oids for the given parent oid");

/* gp_partition_inverse(oid) => setof record */ 
DATA(insert OID = 6086 ( gp_partition_inverse  PGNSP PGUID 12 f f t t s 1 2249 f "26" _null_ _null_ _null_ gp_partition_inverse _null_ _null_ n 0));
DESCR("returns all child partitition oids with their constraints for a given parent oid");

/* disable_xform(text) => text */ 
DATA(insert OID = 6087 ( disable_xform  PGNSP PGUID 12 f f t f i 1 25 f "25" _null_ _null_ _null_ disable_xform _null_ _null_ n 0));
DESCR("disables transformations in the optimizer");

/* enable_xform(text) => text */ 
DATA(insert OID = 6088 ( enable_xform  PGNSP PGUID 12 f f t f i 1 25 f "25" _null_ _null_ _null_ enable_xform _null_ _null_ n 0));
DESCR("enables transformations in the optimizer");

/* gp_opt_version() => text */ 
DATA(insert OID = 6089 ( gp_opt_version  PGNSP PGUID 12 f f t f i 0 25 f "" _null_ _null_ _null_ gp_opt_version _null_ _null_ n 0));
DESCR("Returns the optimizer and gpos library versions");


/* TIDYCAT_END_PG_PROC_GEN */

/*
 *  These are dummy at this point until the next catalog update, but need to be here
 *  for fmgr generator script as well as unused_oids.

DATA(insert OID = 3000 (gp_read_error_log PGNSP PGUID 12 f f t t v 1 2249 f "25" _null_ _null_ _null_ gp_read_error_log - _null_ r ));
DATA(insert OID = 3069 (gp_truncate_error_log PGNSP PGUID 12 f f t t v 1 16 f "25" _null_ _null_ _null_ gp_truncate_error_log - _null_ r ));

*/

/*
 * Symbolic values for provolatile column: these indicate whether the result
 * of a function is dependent *only* on the values of its explicit arguments,
 * or can change due to outside factors (such as parameter variables or
 * table contents).  NOTE: functions having side-effects, such as setval(),
 * must be labeled volatile to ensure they will not get optimized away,
 * even if the actual return value is not changeable.
 */
#define PROVOLATILE_IMMUTABLE	'i'		/* never changes for given input */
#define PROVOLATILE_STABLE		's'		/* does not change within a scan */
#define PROVOLATILE_VOLATILE	'v'		/* can change even within a scan */

/*
 * Symbolic values for proargmodes column.	Note that these must agree with
 * the FunctionParameterMode enum in parsenodes.h; we declare them here to
 * be accessible from either header.
 */
#define PROARGMODE_IN		'i'
#define PROARGMODE_OUT		'o'
#define PROARGMODE_INOUT	'b'
#define PROARGMODE_VARIADIC 'v'
#define PROARGMODE_TABLE	't'

/*
 * Symbolic values for prodataaccess column: these provide a hint regarding
 * what kind of statements are included in the function.
 */
#define PRODATAACCESS_NONE		'n'
#define PRODATAACCESS_CONTAINS	'c'
#define PRODATAACCESS_READS		'r'
#define PRODATAACCESS_MODIFIES	'm'
/* This is an internal-only data access property. */
#define PRODATAACCESS_SEGMENT	's'

/*
 * prototypes for functions in pg_proc.c
 */
extern Oid ProcedureCreate(const char *procedureName,
						   Oid procNamespace,
						   bool replace,
						   bool returnsSet,
						   Oid returnType,
						   Oid languageObjectId,
						   Oid languageValidator,
						   Oid describeFuncOid,
						   const char *prosrc,
						   const char *probin,
						   bool isAgg,
						   bool isWin,
						   bool security_definer,
						   bool isStrict,
						   char volatility,
						   const oidvector *parameterTypes,
						   Datum allParameterTypes,
						   Datum parameterModes,
						   Datum parameterNames,
						   char prodataaccess,
						   Oid procOid);

extern bool function_parse_error_transpose(const char *prosrc);

/*
 * API to access prodataaccess colum
 */
typedef enum SQLDataAccess
{
   SDA_NO_SQL = 0,		/* procedure does not possibly contain SQL */
   SDA_CONTAINS_SQL,		/* possibly contains SQL */
   SDA_READS_SQL,		/* possibly reads SQL */
   SDA_MODIFIES_SQL		/* possibly modifies SQL */
}  SQLDataAccess;

extern SQLDataAccess GetFuncSQLDataAccess(Oid);

#endif   /* PG_PROC_H */
