/*-------------------------------------------------------------------------
 *
 * pg_type.h
 *	  definition of the system "type" relation (pg_type)
 *	  along with the relation's initial contents.
 *
 *
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/catalog/pg_type.h,v 1.172.2.2 2009/02/24 01:39:01 tgl Exp $
 *
 * NOTES
 *	  the genbki.sh script reads this file and generates .bki
 *	  information from the DATA() statements.
 *
 *-------------------------------------------------------------------------
 */
#ifndef PG_TYPE_H
#define PG_TYPE_H


#include "catalog/genbki.h"
#include "nodes/nodes.h"

/* ----------------
 *		pg_type definition.  cpp turns this into
 *		typedef struct FormData_pg_type
 *
 *		Some of the values in a pg_type instance are copied into
 *		pg_attribute instances.  Some parts of Postgres use the pg_type copy,
 *		while others use the pg_attribute copy, so they must match.
 *		See struct FormData_pg_attribute for details.
 * ----------------
 */

/* XXX XXX XXX XXX XXX XXX XXX XXX
 *   
 *   Use a fake tidycat definition here.  Note that it does not
 *   interfere with the type entry code generation because fake
 *   definitions are only parsed if tidycat.pl is called with the
 *   "sqldef" option, which extracts sql definitions and does not
 *   generate real code.
 *   
 * XXX XXX XXX XXX XXX XXX XXX XXX
*/

/* TIDYCAT_BEGINFAKEDEF

   CREATE TABLE pg_type
   with (bootstrap=true, relid=1247)
   (
   typname        name     ,
   typnamespace   oid      ,
   typowner       oid      ,
   typlen         smallint ,
   typbyval       boolean  ,
   typtype        "char"   ,
   typisdefined   boolean  ,
   typdelim       "char"   ,
   typrelid       oid      ,
   typelem        oid      ,
   typinput       regproc  ,
   typoutput      regproc  ,
   typreceive     regproc  ,
   typsend        regproc  ,
   typanalyze     regproc  ,
   typalign       "char"   ,
   typstorage     "char"   ,
   typnotnull     boolean  ,
   typbasetype    oid      ,
   typtypmod      integer  ,
   typndims       integer  ,
   typdefaultbin  text     ,
   typdefault     text     
   );

   create unique index on pg_type(oid) with (indexid=2703, CamelCase=TypeOid, syscacheid=TYPEOID, syscache_nbuckets=1024);
   create unique index on pg_type(typname, typnamespace) with (indexid=2704, CamelCase=TypeNameNsp, syscacheid=TYPENAMENSP, syscache_nbuckets=1024);

   alter table pg_type add fk typnamespace on pg_namespace(oid);
   alter table pg_type add fk typowner on pg_authid(oid);
   alter table pg_type add fk typrelid on pg_class(oid);
   alter table pg_type add fk typinput on pg_proc(oid);
   alter table pg_type add fk typoutput on pg_proc(oid);
   alter table pg_type add fk typreceive on pg_proc(oid);
   alter table pg_type add fk typsend on pg_proc(oid);
   alter table pg_type add fk typanalyze on pg_proc(oid);

   TIDYCAT_ENDFAKEDEF
*/

#define TypeRelationId	1247

CATALOG(pg_type,1247) BKI_BOOTSTRAP
{
	NameData	typname;		/* type name */
	Oid			typnamespace;	/* OID of namespace containing this type */
	Oid			typowner;		/* type owner */

	/*
	 * For a fixed-size type, typlen is the number of bytes we use to
	 * represent a value of this type, e.g. 4 for an int4.	But for a
	 * variable-length type, typlen is negative.  We use -1 to indicate a
	 * "varlena" type (one that has a length word), -2 to indicate a
	 * null-terminated C string.
	 */
	int2		typlen;

	/*
	 * typbyval determines whether internal Postgres routines pass a value of
	 * this type by value or by reference.	typbyval had better be FALSE if
	 * the length is not 1, 2, or 4 (or 8 on 8-byte-Datum machines).
	 * Variable-length types are always passed by reference. Note that
	 * typbyval can be false even if the length would allow pass-by-value;
	 * this is currently true for type float4, for example.
	 */
	bool		typbyval;

	/*
	 * typtype is 'b' for a base type, 'c' for a composite type (e.g., a
	 * table's rowtype), 'd' for a domain type, or 'p'
	 * for a pseudo-type.  (Use the TYPTYPE macros below.)
	 *
	 * If typtype is 'c', typrelid is the OID of the class' entry in pg_class.
	 */
	char		typtype;

	/*
	 * If typisdefined is false, the entry is only a placeholder (forward
	 * reference).	We know the type name, but not yet anything else about it.
	 */
	bool		typisdefined;

	char		typdelim;		/* delimiter for arrays of this type */

	Oid			typrelid;		/* 0 if not a composite type */

	/*
	 * If typelem is not 0 then it identifies another row in pg_type. The
	 * current type can then be subscripted like an array yielding values of
	 * type typelem. A non-zero typelem does not guarantee this type to be a
	 * "real" array type; some ordinary fixed-length types can also be
	 * subscripted (e.g., name, point). Variable-length types can *not* be
	 * turned into pseudo-arrays like that. Hence, the way to determine
	 * whether a type is a "true" array type is if:
	 *
	 * typelem != 0 and typlen == -1.
	 */
	Oid			typelem;

	/*
	 * I/O conversion procedures for the datatype.
	 */
	regproc		typinput;		/* text format (required) */
	regproc		typoutput;
	regproc		typreceive;		/* binary format (optional) */
	regproc		typsend;

	/*
	 * Custom ANALYZE procedure for the datatype (0 selects the default).
	 */
	regproc		typanalyze;

	/* ----------------
	 * typalign is the alignment required when storing a value of this
	 * type.  It applies to storage on disk as well as most
	 * representations of the value inside Postgres.  When multiple values
	 * are stored consecutively, such as in the representation of a
	 * complete row on disk, padding is inserted before a datum of this
	 * type so that it begins on the specified boundary.  The alignment
	 * reference is the beginning of the first datum in the sequence.
	 *
	 * 'c' = CHAR alignment, ie no alignment needed.
	 * 's' = SHORT alignment (2 bytes on most machines).
	 * 'i' = INT alignment (4 bytes on most machines).
	 * 'd' = DOUBLE alignment (8 bytes on many machines, but by no means all).
	 *
	 * See include/access/tupmacs.h for the macros that compute these
	 * alignment requirements.	Note also that we allow the nominal alignment
	 * to be violated when storing "packed" varlenas; the TOAST mechanism
	 * takes care of hiding that from most code.
	 *
	 * NOTE: for types used in system tables, it is critical that the
	 * size and alignment defined in pg_type agree with the way that the
	 * compiler will lay out the field in a struct representing a table row.
	 * ----------------
	 */
	char		typalign;

	/* ----------------
	 * typstorage tells if the type is prepared for toasting and what
	 * the default strategy for attributes of this type should be.
	 *
	 * 'p' PLAIN	  type not prepared for toasting
	 * 'e' EXTERNAL   external storage possible, don't try to compress
	 * 'x' EXTENDED   try to compress and store external if required
	 * 'm' MAIN		  like 'x' but try to keep in main tuple
	 * ----------------
	 */
	char		typstorage;

	/*
	 * This flag represents a "NOT NULL" constraint against this datatype.
	 *
	 * If true, the attnotnull column for a corresponding table column using
	 * this datatype will always enforce the NOT NULL constraint.
	 *
	 * Used primarily for domain types.
	 */
	bool		typnotnull;

	/*
	 * Domains use typbasetype to show the base (or domain) type that the
	 * domain is based on.	Zero if the type is not a domain.
	 */
	Oid			typbasetype;

	/*
	 * Domains use typtypmod to record the typmod to be applied to their base
	 * type (-1 if base type does not use a typmod).  -1 if this type is not a
	 * domain.
	 */
	int4		typtypmod;

	/*
	 * typndims is the declared number of dimensions for an array domain type
	 * (i.e., typbasetype is an array type; the domain's typelem will match
	 * the base type's typelem).  Otherwise zero.
	 */
	int4		typndims;

	/*
	 * If typdefaultbin is not NULL, it is the nodeToString representation of
	 * a default expression for the type.  Currently this is only used for
	 * domains.
	 */
	text		typdefaultbin;	/* VARIABLE LENGTH FIELD */

	/*
	 * typdefault is NULL if the type has no associated default value. If
	 * typdefaultbin is not NULL, typdefault must contain a human-readable
	 * version of the default expression represented by typdefaultbin. If
	 * typdefaultbin is NULL and typdefault is not, then typdefault is the
	 * external representation of the type's default value, which may be fed
	 * to the type's input converter to produce a constant.
	 */
	text		typdefault;		/* VARIABLE LENGTH FIELD */

} FormData_pg_type;

/* ----------------
 *		Form_pg_type corresponds to a pointer to a row with
 *		the format of pg_type relation.
 * ----------------
 */
typedef FormData_pg_type *Form_pg_type;

/* ----------------
 *		compiler constants for pg_type
 * ----------------
 */
#define Natts_pg_type					23
#define Anum_pg_type_typname			1
#define Anum_pg_type_typnamespace		2
#define Anum_pg_type_typowner			3
#define Anum_pg_type_typlen				4
#define Anum_pg_type_typbyval			5
#define Anum_pg_type_typtype			6
#define Anum_pg_type_typisdefined		7
#define Anum_pg_type_typdelim			8
#define Anum_pg_type_typrelid			9
#define Anum_pg_type_typelem			10
#define Anum_pg_type_typinput			11
#define Anum_pg_type_typoutput			12
#define Anum_pg_type_typreceive			13
#define Anum_pg_type_typsend			14
#define Anum_pg_type_typanalyze			15
#define Anum_pg_type_typalign			16
#define Anum_pg_type_typstorage			17
#define Anum_pg_type_typnotnull			18
#define Anum_pg_type_typbasetype		19
#define Anum_pg_type_typtypmod			20
#define Anum_pg_type_typndims			21
#define Anum_pg_type_typdefaultbin		22
#define Anum_pg_type_typdefault			23


/* ----------------
 *		initial contents of pg_type
 * ----------------
 */

/*
 * Keep the following ordered by OID so that later changes can be made more
 * easily.
 *
 * For types used in the system catalogs, make sure the typlen, typbyval, and
 * typalign values here match the initial values for attlen, attbyval, and
 * attalign in both places in pg_attribute.h for every instance.  Also see
 * TypInfo[] in bootstrap.c.
 */

/* TIDYCAT_BEGIN_PG_TYPE_GEN 


   WARNING: DO NOT MODIFY THE FOLLOWING SECTION: 
   Generated by catullus.pl version 8
   on Wed Oct 16 08:43:20 2013

   Please make your changes in pg_type.sql
*/


/*  OIDS 1 - 99  */
DATA(insert OID = 16 (	bool	   PGNSP PGUID 1 t b t \054 0	0 boolin boolout boolrecv boolsend - c p f 0 -1 0 _null_ _null_ ));
DESCR("boolean, 'true'/'false'");
#define BOOLOID			16

DATA(insert OID = 17 (	bytea	   PGNSP PGUID -1 f b t \054 0	0 byteain byteaout bytearecv byteasend - i x f 0 -1 0 _null_ _null_ ));
DESCR("variable-length string, binary values escaped");
#define BYTEAOID		17

DATA(insert OID = 18 (	char	   PGNSP PGUID 1 t b t \054 0	0 charin charout charrecv charsend - c p f 0 -1 0 _null_ _null_ ));
DESCR("single character");
#define CHAROID			18

DATA(insert OID = 19 (	name	   PGNSP PGUID NAMEDATALEN f b t \054 0	18 namein nameout namerecv namesend - i p f 0 -1 0 _null_ _null_ ));
DESCR("63-character type for storing system identifiers");
#define NAMEOID			19

DATA(insert OID = 20 (	int8	   PGNSP PGUID 8 t b t \054 0	0 int8in int8out int8recv int8send - d p f 0 -1 0 _null_ _null_ ));
DESCR("~18 digit integer, 8-byte storage");
#define INT8OID			20

DATA(insert OID = 21 (	int2	   PGNSP PGUID 2 t b t \054 0	0 int2in int2out int2recv int2send - s p f 0 -1 0 _null_ _null_ ));
DESCR("-32 thousand to 32 thousand, 2-byte storage");
#define INT2OID			21

DATA(insert OID = 22 (	int2vector	   PGNSP PGUID -1 f b t \054 0	21 int2vectorin int2vectorout int2vectorrecv int2vectorsend - i p f 0 -1 0 _null_ _null_ ));
DESCR("array of int2, used in system tables");
#define INT2VECTOROID	22

DATA(insert OID = 23 (	int4	   PGNSP PGUID 4 t b t \054 0	0 int4in int4out int4recv int4send - i p f 0 -1 0 _null_ _null_ ));
DESCR("-2 billion to 2 billion integer, 4-byte storage");
#define INT4OID			23

DATA(insert OID = 24 (	regproc	   PGNSP PGUID 4 t b t \054 0	0 regprocin regprocout regprocrecv regprocsend - i p f 0 -1 0 _null_ _null_ ));
DESCR("registered procedure");
#define REGPROCOID		24

DATA(insert OID = 25 (	text	   PGNSP PGUID -1 f b t \054 0	0 textin textout textrecv textsend - i x f 0 -1 0 _null_ _null_ ));
DESCR("variable-length string, no limit specified");
#define TEXTOID			25

DATA(insert OID = 26 (	oid	   PGNSP PGUID 4 t b t \054 0	0 oidin oidout oidrecv oidsend - i p f 0 -1 0 _null_ _null_ ));
DESCR("object identifier(oid), maximum 4 billion");
#define OIDOID			26

DATA(insert OID = 27 (	tid	   PGNSP PGUID 6 f b t \054 0	0 tidin tidout tidrecv tidsend - s p f 0 -1 0 _null_ _null_ ));
DESCR("(Block, offset), physical location of tuple");
#define TIDOID		27

DATA(insert OID = 28 (	xid	   PGNSP PGUID 4 t b t \054 0	0 xidin xidout xidrecv xidsend - i p f 0 -1 0 _null_ _null_ ));
DESCR("transaction id");
#define XIDOID 28

DATA(insert OID = 29 (	cid	   PGNSP PGUID 4 t b t \054 0	0 cidin cidout cidrecv cidsend - i p f 0 -1 0 _null_ _null_ ));
DESCR("command identifier type, sequence in transaction id");
#define CIDOID 29

DATA(insert OID = 30 (	oidvector	   PGNSP PGUID -1 f b t \054 0	26 oidvectorin oidvectorout oidvectorrecv oidvectorsend - i p f 0 -1 0 _null_ _null_ ));
DESCR("array of oids, used in system tables");
#define OIDVECTOROID	30

/*  hand-built rowtype entries for bootstrapped catalogs:  */
DATA(insert OID = 71 (	pg_type	   PGNSP PGUID -1 f c t \054 1247	0 record_in record_out record_recv record_send - d x f 0 -1 0 _null_ _null_ ));
#define PG_TYPE_RELTYPE_OID 71

DATA(insert OID = 75 (	pg_attribute	   PGNSP PGUID -1 f c t \054 1249	0 record_in record_out record_recv record_send - d x f 0 -1 0 _null_ _null_ ));
#define PG_ATTRIBUTE_RELTYPE_OID 75

DATA(insert OID = 81 (	pg_proc	   PGNSP PGUID -1 f c t \054 1255	0 record_in record_out record_recv record_send - d x f 0 -1 0 _null_ _null_ ));
#define PG_PROC_RELTYPE_OID 81

DATA(insert OID = 83 (	pg_class	   PGNSP PGUID -1 f c t \054 1259	0 record_in record_out record_recv record_send - d x f 0 -1 0 _null_ _null_ ));
#define PG_CLASS_RELTYPE_OID 83

DATA(insert OID = 2922 (	pg_authid	   PGNSP PGUID -1 f c t \054 1260	0 record_in record_out record_recv record_send - d x f 0 -1 0 _null_ _null_ ));
#define PG_AUTHID_RELTYPE_OID 2922

DATA(insert OID = 2923 (	pg_auth_members	   PGNSP PGUID -1 f c t \054 1261	0 record_in record_out record_recv record_send - d x f 0 -1 0 _null_ _null_ ));
#define PG_AUTH_MEMBERS_RELTYPE_OID 2923

DATA(insert OID = 2924 (	pg_database	   PGNSP PGUID -1 f c t \054 1262	0 record_in record_out record_recv record_send - d x f 0 -1 0 _null_ _null_ ));
#define PG_DATABASE_RELTYPE_OID 2924

/*  OIDS 100 - 199  */
DATA(insert OID = 142 (	xml	   PGNSP PGUID -1 f b t \054 0	0 xml_in xml_out xml_recv xml_send - i x f 0 -1 0 _null_ _null_ ));
DESCR("XML content");
#define XMLOID 142
DATA(insert OID = 143 (	_xml	   PGNSP PGUID -1 f b t \054 0	142 array_in array_out array_recv array_send - i x f 0 -1 0 _null_ _null_ ));

/*  OIDS 200 - 299  */

DATA(insert OID = 210 (	smgr	   PGNSP PGUID 2 t b t \054 0	0 smgrin smgrout - - - s p f 0 -1 0 _null_ _null_ ));
DESCR("storage manager");



/*  OIDS 300 - 399  */

/*  OIDS 400 - 499  */

/*  OIDS 500 - 599  */

/*  OIDS 600 - 699  */

DATA(insert OID = 600 (	point	   PGNSP PGUID 16 f b t \054 0	701 point_in point_out point_recv point_send - d p f 0 -1 0 _null_ _null_ ));
DESCR("geometric point '(x, y)'");
#define POINTOID		600

DATA(insert OID = 601 (	lseg	   PGNSP PGUID 32 f b t \054 0	600 lseg_in lseg_out lseg_recv lseg_send - d p f 0 -1 0 _null_ _null_ ));
DESCR("geometric line segment '(pt1,pt2)'");
#define LSEGOID			601

DATA(insert OID = 602 (	path	   PGNSP PGUID -1 f b t \054 0	0 path_in path_out path_recv path_send - d x f 0 -1 0 _null_ _null_ ));
DESCR("geometric path '(pt1,...)'");
#define PATHOID			602

DATA(insert OID = 603 (	box	   PGNSP PGUID 32 f b t \073 0	600 box_in box_out box_recv box_send - d p f 0 -1 0 _null_ _null_ ));
DESCR("geometric box '(lower left,upper right)'");
#define BOXOID			603

DATA(insert OID = 604 (	polygon	   PGNSP PGUID -1 f b t \054 0	0 poly_in poly_out poly_recv poly_send - d x f 0 -1 0 _null_ _null_ ));
DESCR("geometric polygon '(pt1,...)'");
#define POLYGONOID		604

DATA(insert OID = 628 (	line	   PGNSP PGUID 32 f b t \054 0	701 line_in line_out line_recv line_send - d p f 0 -1 0 _null_ _null_ ));
DESCR("geometric line (not implemented)");
#define LINEOID			628
DATA(insert OID = 629 (	_line	   PGNSP PGUID -1 f b t \054 0	628 array_in array_out array_recv array_send - d x f 0 -1 0 _null_ _null_ ));

/*  OIDS 700 - 799  */

DATA(insert OID = 700 (	float4	   PGNSP PGUID 4 t b t \054 0	0 float4in float4out float4recv float4send - i p f 0 -1 0 _null_ _null_ ));
DESCR("single-precision floating point number, 4-byte storage");
#define FLOAT4OID 700

DATA(insert OID = 701 (	float8	   PGNSP PGUID 8 t b t \054 0	0 float8in float8out float8recv float8send - d p f 0 -1 0 _null_ _null_ ));
DESCR("double-precision floating point number, 8-byte storage");
#define FLOAT8OID 701

DATA(insert OID = 702 (	abstime	   PGNSP PGUID 4 t b t \054 0	0 abstimein abstimeout abstimerecv abstimesend - i p f 0 -1 0 _null_ _null_ ));
DESCR("absolute, limited-range date and time (Unix system time)");
#define ABSTIMEOID		702

DATA(insert OID = 703 (	reltime	   PGNSP PGUID 4 t b t \054 0	0 reltimein reltimeout reltimerecv reltimesend - i p f 0 -1 0 _null_ _null_ ));
DESCR("relative, limited-range time interval (Unix delta time)");
#define RELTIMEOID		703

DATA(insert OID = 704 (	tinterval	   PGNSP PGUID 12 f b t \054 0	0 tintervalin tintervalout tintervalrecv tintervalsend - i p f 0 -1 0 _null_ _null_ ));
DESCR("(abstime,abstime), time interval");
#define TINTERVALOID	704

DATA(insert OID = 705 (	unknown	   PGNSP PGUID -2 f b t \054 0	0 unknownin unknownout unknownrecv unknownsend - c p f 0 -1 0 _null_ _null_ ));
DESCR("");
#define UNKNOWNOID		705


DATA(insert OID = 718 (	circle	   PGNSP PGUID 24 f b t \054 0	0 circle_in circle_out circle_recv circle_send - d p f 0 -1 0 _null_ _null_ ));
DESCR("geometric circle '(center,radius)'");
#define CIRCLEOID		718
DATA(insert OID = 719 (	_circle	   PGNSP PGUID -1 f b t \054 0	718 array_in array_out array_recv array_send - d x f 0 -1 0 _null_ _null_ ));

DATA(insert OID = 790 (	money	   PGNSP PGUID 4 f b t \054 0	0 cash_in cash_out cash_recv cash_send - i p f 0 -1 0 _null_ _null_ ));
DESCR("monetary amounts, $d,ddd.cc");
#define CASHOID 790
DATA(insert OID = 791 (	_money	   PGNSP PGUID -1 f b t \054 0	790 array_in array_out array_recv array_send - i x f 0 -1 0 _null_ _null_ ));


/*  OIDS 800 - 899  */
DATA(insert OID = 829 (	macaddr	   PGNSP PGUID 6 f b t \054 0	0 macaddr_in macaddr_out macaddr_recv macaddr_send - i p f 0 -1 0 _null_ _null_ ));
DESCR("XX:XX:XX:XX:XX:XX, MAC address");
#define MACADDROID 829

DATA(insert OID = 869 (	inet	   PGNSP PGUID -1 f b t \054 0	0 inet_in inet_out inet_recv inet_send - i p f 0 -1 0 _null_ _null_ ));
DESCR("IP address/netmask, host address, netmask optional");
#define INETOID 869

DATA(insert OID = 650 (	cidr	   PGNSP PGUID -1 f b t \054 0	0 cidr_in cidr_out cidr_recv cidr_send - i p f 0 -1 0 _null_ _null_ ));
DESCR("network IP address/netmask, network address");
#define CIDROID 650

/*  OIDS 900 - 999  */

/*  OIDS 1000 - 1099  */
DATA(insert OID = 1000 (	_bool	   PGNSP PGUID -1 f b t \054 0	16 array_in array_out array_recv array_send - i x f 0 -1 0 _null_ _null_ ));
DATA(insert OID = 1001 (	_bytea	   PGNSP PGUID -1 f b t \054 0	17 array_in array_out array_recv array_send - i x f 0 -1 0 _null_ _null_ ));
DATA(insert OID = 1002 (	_char	   PGNSP PGUID -1 f b t \054 0	18 array_in array_out array_recv array_send - i x f 0 -1 0 _null_ _null_ ));
DATA(insert OID = 1003 (	_name	   PGNSP PGUID -1 f b t \054 0	19 array_in array_out array_recv array_send - i x f 0 -1 0 _null_ _null_ ));
DATA(insert OID = 1005 (	_int2	   PGNSP PGUID -1 f b t \054 0	21 array_in array_out array_recv array_send - i x f 0 -1 0 _null_ _null_ ));
#define INT2ARRAYOID 1005
DATA(insert OID = 1006 (	_int2vector	   PGNSP PGUID -1 f b t \054 0	22 array_in array_out array_recv array_send - i x f 0 -1 0 _null_ _null_ ));
DATA(insert OID = 1007 (	_int4	   PGNSP PGUID -1 f b t \054 0	23 array_in array_out array_recv array_send - i x f 0 -1 0 _null_ _null_ ));
#define INT4ARRAYOID		1007
DATA(insert OID = 1008 (	_regproc	   PGNSP PGUID -1 f b t \054 0	24 array_in array_out array_recv array_send - i x f 0 -1 0 _null_ _null_ ));
DATA(insert OID = 1009 (	_text	   PGNSP PGUID -1 f b t \054 0	25 array_in array_out array_recv array_send - i x f 0 -1 0 _null_ _null_ ));
#define TEXTARRAYOID		1009
DATA(insert OID = 1028 (	_oid	   PGNSP PGUID -1 f b t \054 0	26 array_in array_out array_recv array_send - i x f 0 -1 0 _null_ _null_ ));
DATA(insert OID = 1010 (	_tid	   PGNSP PGUID -1 f b t \054 0	27 array_in array_out array_recv array_send - i x f 0 -1 0 _null_ _null_ ));
DATA(insert OID = 1011 (	_xid	   PGNSP PGUID -1 f b t \054 0	28 array_in array_out array_recv array_send - i x f 0 -1 0 _null_ _null_ ));
DATA(insert OID = 1012 (	_cid	   PGNSP PGUID -1 f b t \054 0	29 array_in array_out array_recv array_send - i x f 0 -1 0 _null_ _null_ ));
DATA(insert OID = 1013 (	_oidvector	   PGNSP PGUID -1 f b t \054 0	30 array_in array_out array_recv array_send - i x f 0 -1 0 _null_ _null_ ));
DATA(insert OID = 1014 (	_bpchar	   PGNSP PGUID -1 f b t \054 0	1042 array_in array_out array_recv array_send - i x f 0 -1 0 _null_ _null_ ));
DATA(insert OID = 1015 (	_varchar	   PGNSP PGUID -1 f b t \054 0	1043 array_in array_out array_recv array_send - i x f 0 -1 0 _null_ _null_ ));
DATA(insert OID = 1016 (	_int8	   PGNSP PGUID -1 f b t \054 0	20 array_in array_out array_recv array_send - d x f 0 -1 0 _null_ _null_ ));
#define INT8ARRAYOID		1016
DATA(insert OID = 1017 (	_point	   PGNSP PGUID -1 f b t \054 0	600 array_in array_out array_recv array_send - d x f 0 -1 0 _null_ _null_ ));
DATA(insert OID = 1018 (	_lseg	   PGNSP PGUID -1 f b t \054 0	601 array_in array_out array_recv array_send - d x f 0 -1 0 _null_ _null_ ));
DATA(insert OID = 1019 (	_path	   PGNSP PGUID -1 f b t \054 0	602 array_in array_out array_recv array_send - d x f 0 -1 0 _null_ _null_ ));
DATA(insert OID = 1020 (	_box	   PGNSP PGUID -1 f b t \073 0	603 array_in array_out array_recv array_send - d x f 0 -1 0 _null_ _null_ ));
DATA(insert OID = 1021 (	_float4	   PGNSP PGUID -1 f b t \054 0	700 array_in array_out array_recv array_send - i x f 0 -1 0 _null_ _null_ ));
#define FLOAT4ARRAYOID 1021
DATA(insert OID = 1022 (	_float8	   PGNSP PGUID -1 f b t \054 0	701 array_in array_out array_recv array_send - d x f 0 -1 0 _null_ _null_ ));
#define FLOAT8ARRAYOID 1022
DATA(insert OID = 1023 (	_abstime	   PGNSP PGUID -1 f b t \054 0	702 array_in array_out array_recv array_send - i x f 0 -1 0 _null_ _null_ ));
DATA(insert OID = 1024 (	_reltime	   PGNSP PGUID -1 f b t \054 0	703 array_in array_out array_recv array_send - i x f 0 -1 0 _null_ _null_ ));
DATA(insert OID = 1025 (	_tinterval	   PGNSP PGUID -1 f b t \054 0	704 array_in array_out array_recv array_send - i x f 0 -1 0 _null_ _null_ ));
DATA(insert OID = 1027 (	_polygon	   PGNSP PGUID -1 f b t \054 0	604 array_in array_out array_recv array_send - d x f 0 -1 0 _null_ _null_ ));

DATA(insert OID = 1033 (	aclitem	   PGNSP PGUID 12 f b t \054 0	0 aclitemin aclitemout - - - i p f 0 -1 0 _null_ _null_ ));
DESCR("access control list");
#define ACLITEMOID		1033
DATA(insert OID = 1034 (	_aclitem	   PGNSP PGUID -1 f b t \054 0	1033 array_in array_out array_recv array_send - i x f 0 -1 0 _null_ _null_ ));
DATA(insert OID = 1040 (	_macaddr	   PGNSP PGUID -1 f b t \054 0	829 array_in array_out array_recv array_send - i x f 0 -1 0 _null_ _null_ ));
DATA(insert OID = 1041 (	_inet	   PGNSP PGUID -1 f b t \054 0	869 array_in array_out array_recv array_send - i x f 0 -1 0 _null_ _null_ ));
DATA(insert OID = 651 (	_cidr	   PGNSP PGUID -1 f b t \054 0	650 array_in array_out array_recv array_send - i x f 0 -1 0 _null_ _null_ ));

DATA(insert OID = 1042 (	bpchar	   PGNSP PGUID -1 f b t \054 0	0 bpcharin bpcharout bpcharrecv bpcharsend - i x f 0 -1 0 _null_ _null_ ));
DESCR("char(length), blank-padded string, fixed storage length");
#define BPCHAROID		1042

DATA(insert OID = 1043 (	varchar	   PGNSP PGUID -1 f b t \054 0	0 varcharin varcharout varcharrecv varcharsend - i x f 0 -1 0 _null_ _null_ ));
DESCR("varchar(length), non-blank-padded string, variable storage length");
#define VARCHAROID		1043

DATA(insert OID = 1082 (	date	   PGNSP PGUID 4 t b t \054 0	0 date_in date_out date_recv date_send - i p f 0 -1 0 _null_ _null_ ));
DESCR("ANSI SQL date");
#define DATEOID			1082

DATA(insert OID = 1083 (	time	   PGNSP PGUID 8 t b t \054 0	0 time_in time_out time_recv time_send - d p f 0 -1 0 _null_ _null_ ));
DESCR("hh:mm:ss, ANSI SQL time");
#define TIMEOID			1083

/*  OIDS 1100 - 1199  */
DATA(insert OID = 1114 (	timestamp	   PGNSP PGUID 8 t b t \054 0	0 timestamp_in timestamp_out timestamp_recv timestamp_send - d p f 0 -1 0 _null_ _null_ ));
DESCR("date and time");
#define TIMESTAMPOID	1114
DATA(insert OID = 1115 (	_timestamp	   PGNSP PGUID -1 f b t \054 0	1114 array_in array_out array_recv array_send - d x f 0 -1 0 _null_ _null_ ));
DATA(insert OID = 1182 (	_date	   PGNSP PGUID -1 f b t \054 0	1082 array_in array_out array_recv array_send - i x f 0 -1 0 _null_ _null_ ));
DATA(insert OID = 1183 (	_time	   PGNSP PGUID -1 f b t \054 0	1083 array_in array_out array_recv array_send - d x f 0 -1 0 _null_ _null_ ));

DATA(insert OID = 1184 (	timestamptz	   PGNSP PGUID 8 t b t \054 0	0 timestamptz_in timestamptz_out timestamptz_recv timestamptz_send - d p f 0 -1 0 _null_ _null_ ));
DESCR("date and time with time zone");
#define TIMESTAMPTZOID	1184
DATA(insert OID = 1185 (	_timestamptz	   PGNSP PGUID -1 f b t \054 0	1184 array_in array_out array_recv array_send - d x f 0 -1 0 _null_ _null_ ));

DATA(insert OID = 1186 (	interval	   PGNSP PGUID 16 f b t \054 0	0 interval_in interval_out interval_recv interval_send - d p f 0 -1 0 _null_ _null_ ));
DESCR("@ <number> <units>, time interval");
#define INTERVALOID		1186
DATA(insert OID = 1187 (	_interval	   PGNSP PGUID -1 f b t \054 0	1186 array_in array_out array_recv array_send - d x f 0 -1 0 _null_ _null_ ));

/*  OIDS 1200 - 1299  */
DATA(insert OID = 1231 (	_numeric	   PGNSP PGUID -1 f b t \054 0	1700 array_in array_out array_recv array_send - i x f 0 -1 0 _null_ _null_ ));

DATA(insert OID = 1266 (	timetz	   PGNSP PGUID 12 f b t \054 0	0 timetz_in timetz_out timetz_recv timetz_send - d p f 0 -1 0 _null_ _null_ ));
DESCR("hh:mm:ss, ANSI SQL time");
#define TIMETZOID		1266
DATA(insert OID = 1270 (	_timetz	   PGNSP PGUID -1 f b t \054 0	1266 array_in array_out array_recv array_send - d x f 0 -1 0 _null_ _null_ ));

/*  OIDS 1500 - 1599  */
DATA(insert OID = 1560 (	bit	   PGNSP PGUID -1 f b t \054 0	0 bit_in bit_out bit_recv bit_send - i x f 0 -1 0 _null_ _null_ ));
DESCR("fixed-length bit string");
#define BITOID	 1560
DATA(insert OID = 1561 (	_bit	   PGNSP PGUID -1 f b t \054 0	1560 array_in array_out array_recv array_send - i x f 0 -1 0 _null_ _null_ ));

DATA(insert OID = 1562 (	varbit	   PGNSP PGUID -1 f b t \054 0	0 varbit_in varbit_out varbit_recv varbit_send - i x f 0 -1 0 _null_ _null_ ));
DESCR("variable-length bit string");
#define VARBITOID	  1562
DATA(insert OID = 1563 (	_varbit	   PGNSP PGUID -1 f b t \054 0	1562 array_in array_out array_recv array_send - i x f 0 -1 0 _null_ _null_ ));

/*  OIDS 1600 - 1699  */

/*  OIDS 1700 - 1799  */
DATA(insert OID = 1700 (	numeric	   PGNSP PGUID -1 f b t \054 0	0 numeric_in numeric_out numeric_recv numeric_send - i m f 0 -1 0 _null_ _null_ ));
DESCR("numeric(precision, decimal), arbitrary precision number");
#define NUMERICOID		1700

DATA(insert OID = 1790 (	refcursor	   PGNSP PGUID -1 f b t \054 0	0 textin textout textrecv textsend - i x f 0 -1 0 _null_ _null_ ));
DESCR("reference cursor (portal name)");
#define REFCURSOROID	1790

/*  OIDS 2200 - 2299  */
DATA(insert OID = 2201 (	_refcursor	   PGNSP PGUID -1 f b t \054 0	1790 array_in array_out array_recv array_send - i x f 0 -1 0 _null_ _null_ ));

DATA(insert OID = 2202 (	regprocedure	   PGNSP PGUID 4 t b t \054 0	0 regprocedurein regprocedureout regprocedurerecv regproceduresend - i p f 0 -1 0 _null_ _null_ ));
DESCR("registered procedure (with args)");
#define REGPROCEDUREOID 2202

DATA(insert OID = 2203 (	regoper	   PGNSP PGUID 4 t b t \054 0	0 regoperin regoperout regoperrecv regopersend - i p f 0 -1 0 _null_ _null_ ));
DESCR("registered operator");
#define REGOPEROID		2203

DATA(insert OID = 2204 (	regoperator	   PGNSP PGUID 4 t b t \054 0	0 regoperatorin regoperatorout regoperatorrecv regoperatorsend - i p f 0 -1 0 _null_ _null_ ));
DESCR("registered operator (with args)");
#define REGOPERATOROID	2204

DATA(insert OID = 2205 (	regclass	   PGNSP PGUID 4 t b t \054 0	0 regclassin regclassout regclassrecv regclasssend - i p f 0 -1 0 _null_ _null_ ));
DESCR("registered class");
#define REGCLASSOID		2205

DATA(insert OID = 2206 (	regtype	   PGNSP PGUID 4 t b t \054 0	0 regtypein regtypeout regtyperecv regtypesend - i p f 0 -1 0 _null_ _null_ ));
DESCR("registered type");
#define REGTYPEOID		2206
DATA(insert OID = 2207 (	_regprocedure	   PGNSP PGUID -1 f b t \054 0	2202 array_in array_out array_recv array_send - i x f 0 -1 0 _null_ _null_ ));
DATA(insert OID = 2208 (	_regoper	   PGNSP PGUID -1 f b t \054 0	2203 array_in array_out array_recv array_send - i x f 0 -1 0 _null_ _null_ ));
DATA(insert OID = 2209 (	_regoperator	   PGNSP PGUID -1 f b t \054 0	2204 array_in array_out array_recv array_send - i x f 0 -1 0 _null_ _null_ ));
DATA(insert OID = 2210 (	_regclass	   PGNSP PGUID -1 f b t \054 0	2205 array_in array_out array_recv array_send - i x f 0 -1 0 _null_ _null_ ));
DATA(insert OID = 2211 (	_regtype	   PGNSP PGUID -1 f b t \054 0	2206 array_in array_out array_recv array_send - i x f 0 -1 0 _null_ _null_ ));
#define REGTYPEARRAYOID 2211

DATA(insert OID = 3251 (	nb_classification	   PGNSP PGUID -1 f c t \054 3250	0 record_in record_out record_recv record_send - d x f 0 -1 0 _null_ _null_ ));

DATA(insert OID = 3300 (	gpaotid	   PGNSP PGUID 6 f b t \054 0	0 gpaotidin gpaotidout gpaotidrecv gpaotidsend - s p f 0 -1 0 _null_ _null_ ));
DESCR("(segment file num, row number), logical location of append-only tuple");
#define AOTIDOID	3300
DATA(insert OID = 3301 (	_gpaotid	   PGNSP PGUID -1 f b t \054 0	3300 array_in array_out array_recv array_send - i x f 0 -1 0 _null_ _null_ ));

DATA(insert OID = 3310 (	gpxlogloc	   PGNSP PGUID 8 f b t \054 0	0 gpxloglocin gpxloglocout gpxloglocrecv gpxloglocsend - i p f 0 -1 0 _null_ _null_ ));
DESCR("(h/h) -- the hexadecimal xlogid and xrecoff of an XLOG location");
#define XLOGLOCOID	3310
DATA(insert OID = 3311 (	_gpxlogloc	   PGNSP PGUID -1 f b t \054 0	3310 array_in array_out array_recv array_send - i x f 0 -1 0 _null_ _null_ ));

/*  
**  pseudo-types 
**  
**  types with typtype='p' represent various special cases in the type system. 
**  
**  These cannot be used to define table columns, but are valid as function 
**  argument and result types (if supported by the function's implementation 
**  language). 
*/

DATA(insert OID = 2249 (	record	   PGNSP PGUID -1 f p t \054 0	0 record_in record_out record_recv record_send - d x f 0 -1 0 _null_ _null_ ));
#define RECORDOID		2249

DATA(insert OID = 2275 (	cstring	   PGNSP PGUID -2 f p t \054 0	0 cstring_in cstring_out cstring_recv cstring_send - c p f 0 -1 0 _null_ _null_ ));
#define CSTRINGOID		2275

DATA(insert OID = 2276 (	any	   PGNSP PGUID 4 t p t \054 0	0 any_in any_out - - - i p f 0 -1 0 _null_ _null_ ));
#define ANYOID			2276

DATA(insert OID = 2277 (	anyarray	   PGNSP PGUID -1 f p t \054 0	0 anyarray_in anyarray_out anyarray_recv anyarray_send - d x f 0 -1 0 _null_ _null_ ));
#define ANYARRAYOID		2277

DATA(insert OID = 2278 (	void	   PGNSP PGUID 4 t p t \054 0	0 void_in void_out - - - i p f 0 -1 0 _null_ _null_ ));
#define VOIDOID			2278

DATA(insert OID = 2279 (	trigger	   PGNSP PGUID 4 t p t \054 0	0 trigger_in trigger_out - - - i p f 0 -1 0 _null_ _null_ ));
#define TRIGGEROID		2279

DATA(insert OID = 2280 (	language_handler	   PGNSP PGUID 4 t p t \054 0	0 language_handler_in language_handler_out - - - i p f 0 -1 0 _null_ _null_ ));
#define LANGUAGE_HANDLEROID		2280

DATA(insert OID = 2281 (	internal	   PGNSP PGUID 4 t p t \054 0	0 internal_in internal_out - - - i p f 0 -1 0 _null_ _null_ ));
#define INTERNALOID		2281

DATA(insert OID = 2282 (	opaque	   PGNSP PGUID 4 t p t \054 0	0 opaque_in opaque_out - - - i p f 0 -1 0 _null_ _null_ ));
#define OPAQUEOID		2282

DATA(insert OID = 2283 (	anyelement	   PGNSP PGUID 4 t p t \054 0	0 anyelement_in anyelement_out - - - i p f 0 -1 0 _null_ _null_ ));
#define ANYELEMENTOID	2283

DATA(insert OID = 3053 (	anytable	   PGNSP PGUID -1 f p t \054 0	0 anytable_in anytable_out - - - d x f 0 -1 0 _null_ _null_ ));
DESCR("Represents a generic TABLE value expression");
#define ANYTABLEOID     3053

/* TIDYCAT_END_PG_TYPE_GEN */

/* 
 * These tables must be created with standardized oids because they are 
 * created during upgrades from prior versions.
 *
 * Note: For this to work correctly it requires some hackery in bootparse.y
 * When extending this list make sure to update there as well.
 */

/* toast tables - now all generated by tidycat */

/* catalog tables */
DATA(insert OID = 6437 ( pg_appendonly_alter_column	PGNSP PGUID -1 f c t \054 6110 0 record_in record_out record_recv record_send - d x f 0 -1 0 _null_ _null_ ));
#define PG_APPENDONLY_ALTER_COLUMN_OID 6437
DATA(insert OID = 6438 ( pg_filespace 	            PGNSP PGUID -1 f c t \054 5009 0 record_in record_out record_recv record_send - d x f 0 -1 0 _null_ _null_ ));
#define PG_FILESPACE_OID 6438
DATA(insert OID = 6440 ( pg_stat_last_operation	    PGNSP PGUID -1 f c t \054 6052 0 record_in record_out record_recv record_send - d x f 0 -1 0 _null_ _null_ ));
#define PG_STAT_LAST_OPERATION_OID 6440
DATA(insert OID = 6441 ( pg_stat_last_shoperation	    PGNSP PGUID -1 f c t \054 6056 0 record_in record_out record_recv record_send - d x f 0 -1 0 _null_ _null_ ));
#define PG_STAT_LAST_SHOPERATION_OID 6441

DATA(insert OID = 6447 (pg_foreign_data_wrapper PGNSP PGUID -1 f c t \054 2898 0 record_in record_out record_recv record_send - d x f 0 -1 0 _null_ _null_));
#define PG_FOREIGN_DATA_WRAPPER_OID 6447
DATA(insert OID = 6448 (pg_foreign_server PGNSP PGUID -1 f c t \054 2899 0 record_in record_out record_recv record_send - d x f 0 -1 0 _null_ _null_));
#define PG_FOREIGN_SERVER_OID 6448
DATA(insert OID = 6449 (pg_user_mapping PGNSP PGUID -1 f c t \054 2895 0 record_in record_out record_recv record_send - d x f 0 -1 0 _null_ _null_));
#define PG_USER_MAPPING_OID 6449
DATA(insert OID = 6452 (pg_foreign_table PGNSP PGUID -1 f c t \054 2879 0 record_in record_out record_recv record_send - d x f 0 -1 0 _null_ _null_));
#define PG_FOREIGN_TABLE_OID 6452

/* TIDYCAT_BEGIN_CODEGEN 
*/
/*
   WARNING: DO NOT MODIFY THE FOLLOWING SECTION: 
   Generated by ./tidycat.pl version 31
   on Tue Aug 30 14:37:51 2011
 */
/* relation id: 6059 - pg_resourcetype 20100106 */
DATA(insert OID = 6445 ( pg_resourcetype	    PGNSP PGUID -1 f c t \054 6059 0 record_in record_out record_recv record_send - d x f 0 -1 0 _null_ _null_ ));	
#define PG_RESOURCETYPE_RELTYPE_OID 6445
/* relation id: 6060 - pg_resqueuecapability 20100106 */
DATA(insert OID = 6446 ( pg_resqueuecapability	    PGNSP PGUID -1 f c t \054 6060 0 record_in record_out record_recv record_send - d x f 0 -1 0 _null_ _null_ ));	
#define PG_RESQUEUECAPABILITY_RELTYPE_OID 6446
/* relation id: 5035 - gp_san_configuration 20101104 */
DATA(insert OID = 6444 ( gp_san_configuration	    PGNSP PGUID -1 f c t \054 5035 0 record_in record_out record_recv record_send - d x f 0 -1 0 _null_ _null_ ));	
#define GP_SAN_CONFIGURATION_RELTYPE_OID 6444
/* relation id: 5039 - gp_fault_strategy 20101104 */
DATA(insert OID = 6443 ( gp_fault_strategy	    PGNSP PGUID -1 f c t \054 5039 0 record_in record_out record_recv record_send - d x f 0 -1 0 _null_ _null_ ));	
#define GP_FAULT_STRATEGY_RELTYPE_OID 6443
/* relation id: 5043 - gp_fastsequence 20101104 */
DATA(insert OID = 6453 ( gp_fastsequence	    PGNSP PGUID -1 f c t \054 5043 0 record_in record_out record_recv record_send - d x f 0 -1 0 _null_ _null_ ));	
#define GP_FASTSEQUENCE_RELTYPE_OID 6453
/* relation id: 5096 - gp_global_sequence 20101104 */
DATA(insert OID = 6995 ( gp_global_sequence	    PGNSP PGUID -1 f c t \054 5096 0 record_in record_out record_recv record_send - d x f 0 -1 0 _null_ _null_ ));	
#define GP_GLOBAL_SEQUENCE_RELTYPE_OID 6995
/* relation id: 5006 - gp_configuration_history 20101104 */
DATA(insert OID = 6434 ( gp_configuration_history	    PGNSP PGUID -1 f c t \054 5006 0 record_in record_out record_recv record_send - d x f 0 -1 0 _null_ _null_ ));	
#define GP_CONFIGURATION_HISTORY_RELTYPE_OID 6434
/* relation id: 5029 - gp_db_interfaces 20101104 */
DATA(insert OID = 6436 ( gp_db_interfaces	    PGNSP PGUID -1 f c t \054 5029 0 record_in record_out record_recv record_send - d x f 0 -1 0 _null_ _null_ ));	
#define GP_DB_INTERFACES_RELTYPE_OID 6436
/* relation id: 5030 - gp_interfaces 20101104 */
DATA(insert OID = 6433 ( gp_interfaces	    PGNSP PGUID -1 f c t \054 5030 0 record_in record_out record_recv record_send - d x f 0 -1 0 _null_ _null_ ));	
#define GP_INTERFACES_RELTYPE_OID 6433
/* relation id: 5033 - pg_filespace_entry 20101122 */
DATA(insert OID = 6439 ( pg_filespace_entry	    PGNSP PGUID -1 f c t \054 5033 0 record_in record_out record_recv record_send - d x f 0 -1 0 _null_ _null_ ));	
#define PG_FILESPACE_ENTRY_RELTYPE_OID 6439
/* relation id: 5033 - pg_filespace_entry 20101122 */
DATA(insert OID = 2907 (pg_toast_5033 TOASTNSP PGUID -1 f c t \054 2902 0 record_in record_out record_recv record_send - d x f 0 -1 0 _null_ _null_));
#define PG_FILESPACE_ENTRY_TOAST_RELTYPE_OID 2907
/* relation id: 5036 - gp_segment_configuration 20101122 */
DATA(insert OID = 6442 ( gp_segment_configuration	    PGNSP PGUID -1 f c t \054 5036 0 record_in record_out record_recv record_send - d x f 0 -1 0 _null_ _null_ ));	
#define GP_SEGMENT_CONFIGURATION_RELTYPE_OID 6442
/* relation id: 5036 - gp_segment_configuration 20101122 */
DATA(insert OID = 2906 (pg_toast_5036 TOASTNSP PGUID -1 f c t \054 2900 0 record_in record_out record_recv record_send - d x f 0 -1 0 _null_ _null_));
#define GP_SEGMENT_CONFIGURATION_TOAST_RELTYPE_OID 2906
/* relation id: 7175 - pg_extprotocol 20110526 */
DATA(insert OID = 7176 ( pg_extprotocol	    PGNSP PGUID -1 f c t \054 7175 0 record_in record_out record_recv record_send - d x f 0 -1 0 _null_ _null_ ));	
#define PG_EXTPROTOCOL_RELTYPE_OID 7176
/* relation id: 3231 - pg_attribute_encoding 20110727 */
DATA(insert OID = 3232 ( pg_attribute_encoding	    PGNSP PGUID -1 f c t \054 3231 0 record_in record_out record_recv record_send - d x f 0 -1 0 _null_ _null_ ));	
#define PG_ATTRIBUTE_ENCODING_RELTYPE_OID 3232
/* relation id: 3231 - pg_attribute_storage 20110727 */
DATA(insert OID = 3235 (pg_toast_3231 TOASTNSP PGUID -1 f c t \054 3233 0 record_in record_out record_recv record_send - d x f 0 -1 0 _null_ _null_));
#define PG_ATTRIBUTE_ENCODING_TOAST_RELTYPE_OID 3235
/* relation id: 3220 - pg_type_encoding 20110727 */
DATA(insert OID = 3221 ( pg_type_encoding	    PGNSP PGUID -1 f c t \054 3220 0 record_in record_out record_recv record_send - d x f 0 -1 0 _null_ _null_ ));	
#define PG_TYPE_ENCODING_RELTYPE_OID 3221
/* relation id: 6429 - gp_verification_history 20110609 */
DATA(insert OID = 6430 ( gp_verification_history	    PGNSP PGUID -1 f c t \054 6429 0 record_in record_out record_recv record_send - d x f 0 -1 0 _null_ _null_ ));	
#define GP_VERIFICATION_HISTORY_RELTYPE_OID 6430
/* relation id: 3220 - pg_type_encoding 20110727 */
DATA(insert OID = 3224 (pg_toast_3220 TOASTNSP PGUID -1 f c t \054 3222 0 record_in record_out record_recv record_send - d x f 0 -1 0 _null_ _null_));
#define PG_TYPE_ENCODING_TOAST_RELTYPE_OID 3224

/* relation id: 3124 - pg_proc_callback 20110829 */
DATA(insert OID = 3125 ( pg_proc_callback	    PGNSP PGUID -1 f c t \054 3124 0 record_in record_out record_recv record_send - d x f 0 -1 0 _null_ _null_ ));	
#define PG_PROC_CALLBACK_RELTYPE_OID 3125
/* relation id: 9903 - pg_partition_encoding 20110814 */
DATA(insert OID = 9904 ( pg_partition_encoding	    PGNSP PGUID -1 f c t \054 9903 0 record_in record_out record_recv record_send - d x f 0 -1 0 _null_ _null_ ));	
#define PG_PARTITION_ENCODING_RELTYPE_OID 9904
/* relation id: 9903 - pg_partition_encoding 20110814 */
DATA(insert OID = 9907 (pg_toast_9903 TOASTNSP PGUID -1 f c t \054 9905 0 record_in record_out record_recv record_send - d x f 0 -1 0 _null_ _null_));
#define PG_PARTITION_ENCODING_TOAST_RELTYPE_OID 9907

/* relation id: 2914 - pg_auth_time_constraint 20110908 */
DATA(insert OID = 2915 ( pg_auth_time_constraint        PGNSP PGUID -1 f c t \054 2914 0 record_in record_out record_recv record_send - d x f 0 -1 0 _null_ _null_ ));
#define PG_AUTH_TIME_CONSTRAINT_RELTYPE_OID 2915
/* relation id: 3056 - pg_compression 20110830 */
DATA(insert OID = 3057 ( pg_compression	    PGNSP PGUID -1 f c t \054 3056 0 record_in record_out record_recv record_send - d x f 0 -1 0 _null_ _null_ ));	
#define PG_COMPRESSION_RELTYPE_OID 3057

/* TIDYCAT_END_CODEGEN */

DATA(insert OID = 6990 (gp_persistent_relation_node PGNSP PGUID -1 f c t \054 5090 0 record_in record_out record_recv record_send - d x f 0 -1 0 _null_ _null_));
#define GP_PERSISTENT_RELATION_NODE_OID 6990
DATA(insert OID = 6991 (gp_persistent_database_node PGNSP PGUID -1 f c t \054 5091 0 record_in record_out record_recv record_send - d x f 0 -1 0 _null_ _null_));
#define GP_PERSISTENT_DATABASE_NODE_OID 6991
DATA(insert OID = 6992 (gp_persistent_tablespace_node PGNSP PGUID -1 f c t \054 5092 0 record_in record_out record_recv record_send - d x f 0 -1 0 _null_ _null_));
#define GP_PERSISTENT_TABLESPACE_NODE_OID 6992
DATA(insert OID = 6993 (gp_persistent_filespace_node PGNSP PGUID -1 f c t \054 5093 0 record_in record_out record_recv record_send - d x f 0 -1 0 _null_ _null_));
#define GP_PERSISTENT_FILESPACE_NODE_OID 6993
DATA(insert OID = 6994 (gp_relation_node PGNSP PGUID -1 f c t \054 5094 0 record_in record_out record_recv record_send - d x f 0 -1 0 _null_ _null_));
#define GP_RELATION_NODE_OID 6994


/*
 * macros
 */
#define  TYPTYPE_BASE		'b' /* base type (ordinary scalar type) */
#define  TYPTYPE_COMPOSITE	'c' /* composite (e.g., table's rowtype) */
#define  TYPTYPE_DOMAIN		'd' /* domain over another type */
#define  TYPTYPE_ENUM		'e' /* enumerated type */
#define  TYPTYPE_PSEUDO		'p' /* pseudo-type */

/* 
 * typcategory is from Postgres 9.0 catalog changes and is not actually reflected
 * anywhere in gpdb code.
 */
#if 0
# define  TYPCATEGORY_INVALID			'\0'	/* not an allowed category */
# define  TYPCATEGORY_ARRAY				'A'
# define  TYPCATEGORY_BOOLEAN			'B'
# define  TYPCATEGORY_COMPOSITE			'C'
# define  TYPCATEGORY_DATETIME			'D'
# define  TYPCATEGORY_ENUM				'E'
# define  TYPCATEGORY_GEOMETRIC			'G'
# define  TYPCATEGORY_NETWORK			'I'		/* think INET */
# define  TYPCATEGORY_NUMERIC			'N'
# define  TYPCATEGORY_PSEUDOTYPE		'P'
# define  TYPCATEGORY_STRING			'S'
# define  TYPCATEGORY_TIMESPAN			'T'
# define  TYPCATEGORY_USER				'U'
# define  TYPCATEGORY_BITSTRING			'V'		/* er ... "varbit"? */
# define  TYPCATEGORY_UNKNOWN			'X'
#endif

/* Is a type OID a polymorphic pseudotype?	(Beware of multiple evaluation) */
#define IsPolymorphicType(typid)  \
	((typid) == ANYELEMENTOID || \
	 (typid) == ANYARRAYOID)
	 //(typid) == ANYNONARRAYOID || \   /// added in pg 8.4
	// (typid) == ANYENUMOID)

/* Is a type OID suitable for describe callback functions? */
#define TypeSupportsDescribe(typid)  \
	((typid) == RECORDOID)

#ifndef FRONTEND /* don't export these to the front end */

/*
 * prototypes for functions in pg_type.c
 */
extern Oid TypeShellMake(const char *typeName, Oid typeNamespace, Oid ownerId,
						 Oid shelloid);
extern Oid TypeShellMakeWithOid(const char *typeName, Oid typeNamespace,
								Oid ownerId, Oid shelltypeOid);

extern Oid TypeCreate(const char *typeName,
		   Oid typeNamespace,
		   Oid relationOid,
		   char relationKind,
		   Oid ownerId,
		   int16 internalSize,
		   char typeType,
		   char typDelim,
		   Oid inputProcedure,
		   Oid outputProcedure,
		   Oid receiveProcedure,
		   Oid sendProcedure,
		   Oid analyzeProcedure,
		   Oid elementType,
		   Oid baseType,
		   const char *defaultTypeValue,
		   char *defaultTypeBin,
		   bool passedByValue,
		   char alignment,
		   char storage,
		   int32 typeMod,
		   int32 typNDims,
		   bool typeNotNull);
		   
extern Oid TypeCreateWithOid(const char *typeName,
		   Oid typeNamespace,
		   Oid relationOid,
		   char relationKind,
		   Oid ownerId,
		   int16 internalSize,
		   char typeType,
		   char typDelim,
		   Oid inputProcedure,
		   Oid outputProcedure,
		   Oid receiveProcedure,
		   Oid sendProcedure,
		   Oid analyzeProcedure,
		   Oid elementType,
		   Oid baseType,
		   const char *defaultTypeValue,
		   char *defaultTypeBin,
		   bool passedByValue,
		   char alignment,
		   char storage,
		   int32 typeMod,
		   int32 typNDims,
		   bool typeNotNull,
		   Oid newtypeOid,
		   Datum typoptions);

extern void GenerateTypeDependencies(Oid typeNamespace,
						 Oid typeObjectId,
						 Oid relationOid,
						 char relationKind,
						 Oid owner,
						 Oid inputProcedure,
						 Oid outputProcedure,
						 Oid receiveProcedure,
						 Oid sendProcedure,
						 Oid analyzeProcedure,
						 Oid elementType,
						 Oid baseType,
						 Node *defaultExpr,
						 bool rebuild);

extern void TypeRename(Oid typeOid, const char *newTypeName);

extern char *makeArrayTypeName(const char *typeName);
extern void add_type_encoding(Oid typid, Datum typoptions);

#endif /* !FRONTEND */

#endif   /* PG_TYPE_H */
