/*-------------------------------------------------------------------------
 *
 * pg_class.h
 *	  definition of the system "relation" relation (pg_class)
 *	  along with the relation's initial contents.
 *
 *
 * Portions Copyright (c) 1996-2011, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/catalog/pg_class.h
 *
 * NOTES
 *	  the genbki.pl script reads this file and generates .bki
 *	  information from the DATA() statements.
 *
 *-------------------------------------------------------------------------
 */
#ifndef PG_CLASS_H
#define PG_CLASS_H

#include "catalog/genbki.h"

/* ----------------
 *		postgres.h contains the system type definitions and the
 *		CATALOG(), BKI_BOOTSTRAP and DATA() sugar words so this file
 *		can be read by both genbki.sh and the C compiler.
 * ----------------
 */
#define RelationRelationId	1259
#define RelationRelation_Rowtype_Id  83

CATALOG(pg_class,1259) BKI_BOOTSTRAP BKI_ROWTYPE_OID(83) BKI_SCHEMA_MACRO
{
	NameData	relname;		/* class name */
	Oid			relnamespace;	/* OID of namespace containing this class */
	Oid			reltype;		/* OID of entry in pg_type for table's
								 * implicit row type */
	Oid			reloftype;		/* OID of entry in pg_type for underlying
								 * composite type */
	Oid			relowner;		/* class owner */
	Oid			relam;			/* index access method; 0 if not an index */
	Oid			relfilenode;	/* identifier of physical storage file */

	/* relfilenode == 0 means it is a "mapped" relation, see relmapper.c */
	Oid			reltablespace;	/* identifier of table space for relation */
	int4		relpages;		/* # of blocks (not always up-to-date) */
	float4		reltuples;		/* # of tuples (not always up-to-date) */
	Oid			reltoastrelid;	/* OID of toast table; 0 if none */
	Oid			reltoastidxid;	/* if toast table, OID of chunk_id index */
	bool		relhasindex;	/* T if has (or has had) any indexes */
	bool		relisshared;	/* T if shared across databases */
	char		relpersistence; /* see RELPERSISTENCE_xxx constants below */
	char		relkind;		/* see RELKIND_xxx constants below */
	char		relstorage;		/* see RELSTORAGE_xxx constants below */
	int2		relnatts;		/* number of user attributes */

	/*
	 * Class pg_attribute must contain exactly "relnatts" user attributes
	 * (with attnums ranging from 1 to relnatts) for this class.  It may also
	 * contain entries with negative attnums for system attributes.
	 */
	int2		relchecks;		/* # of CHECK constraints for class */
	bool		relhasoids;		/* T if we generate OIDs for rows of rel */
	bool		relhaspkey;		/* has (or has had) PRIMARY KEY index */
	bool		relhasrules;	/* has (or has had) any rules */
	bool		relhastriggers; /* has (or has had) any TRIGGERs */
	bool		relhassubclass; /* has (or has had) derived classes */
	TransactionId relfrozenxid; /* all Xids < this are frozen in this rel */

	/*
	 * VARIABLE LENGTH FIELDS start here.  These fields may be NULL, too.
	 *
	 * NOTE: these fields are not present in a relcache entry's rd_rel field.
	 */

	aclitem		relacl[1];		/* access permissions */
	text		reloptions[1];	/* access-method-specific options */
} FormData_pg_class;

/* GPDB added foreign key definitions for gpcheckcat. */
FOREIGN_KEY(relnamespace REFERENCES pg_namespace(oid));
FOREIGN_KEY(reltype REFERENCES pg_type(oid));
FOREIGN_KEY(relowner REFERENCES pg_authid(oid));
FOREIGN_KEY(relam REFERENCES pg_am(oid));
FOREIGN_KEY(reltablespace REFERENCES pg_tablespace(oid));
FOREIGN_KEY(reltoastrelid REFERENCES pg_class(oid));
FOREIGN_KEY(reltoastidxid REFERENCES pg_class(oid));

/* Size of fixed part of pg_class tuples, not counting var-length fields */
#define CLASS_TUPLE_SIZE \
	 (offsetof(FormData_pg_class,relfrozenxid) + sizeof(TransactionId))

/* ----------------
 *		Form_pg_class corresponds to a pointer to a tuple with
 *		the format of pg_class relation.
 * ----------------
 */
typedef FormData_pg_class *Form_pg_class;

/* ----------------
 *		compiler constants for pg_class
 * ----------------
 */

#define Natts_pg_class					27
#define Anum_pg_class_relname			1
#define Anum_pg_class_relnamespace		2
#define Anum_pg_class_reltype			3
#define Anum_pg_class_reloftype			4
#define Anum_pg_class_relowner			5
#define Anum_pg_class_relam				6
#define Anum_pg_class_relfilenode		7
#define Anum_pg_class_reltablespace		8
#define Anum_pg_class_relpages			9
#define Anum_pg_class_reltuples			10
#define Anum_pg_class_reltoastrelid		11
#define Anum_pg_class_reltoastidxid		12
#define Anum_pg_class_relhasindex		13
#define Anum_pg_class_relisshared		14
#define Anum_pg_class_relpersistence	15
#define Anum_pg_class_relkind			16
#define Anum_pg_class_relstorage		17
#define Anum_pg_class_relnatts			18
#define Anum_pg_class_relchecks			19
#define Anum_pg_class_relhasoids		20
#define Anum_pg_class_relhaspkey		21
#define Anum_pg_class_relhasrules		22
#define Anum_pg_class_relhastriggers	23
#define Anum_pg_class_relhassubclass	24
#define Anum_pg_class_relfrozenxid		25
#define Anum_pg_class_relacl			26
#define Anum_pg_class_reloptions		27

/* ----------------
 *		initial contents of pg_class
 *
 * NOTE: only "bootstrapped" relations need to be declared here.  Be sure that
 * the OIDs listed here match those given in their CATALOG macros, and that
 * the relnatts values are correct.
 * ----------------
 */

/* Note: "3" in the relfrozenxid column stands for FirstNormalTransactionId */
DATA(insert OID = 1247 (  pg_type		PGNSP 71 0 PGUID 0 0 0 0 0 0 0 f f p r h 29 0 t f f f f 3 _null_ _null_ ));
DESCR("");
DATA(insert OID = 1249 (  pg_attribute	PGNSP 75 0 PGUID 0 0 0 0 0 0 0 f f p r h 20 0 f f f f f 3 _null_ _null_ ));
DESCR("");
DATA(insert OID = 1255 (  pg_proc		PGNSP 81 0 PGUID 0 0 0 0 0 0 0 f f p r h 27 0 t f f f f 3 _null_ _null_ ));
DESCR("");
DATA(insert OID = 1259 (  pg_class		PGNSP 83 0 PGUID 0 0 0 0 0 0 0 f f p r h 27 0 t f f f f 3 _null_ _null_ ));
DESCR("");


#define		  RELKIND_RELATION		  'r'		/* ordinary table */
#define		  RELKIND_INDEX			  'i'		/* secondary index */
#define		  RELKIND_SEQUENCE		  'S'		/* sequence object */
#define		  RELKIND_TOASTVALUE	  't'		/* for out-of-line values */
#define		  RELKIND_VIEW			  'v'		/* view */
#define		  RELKIND_COMPOSITE_TYPE  'c'		/* composite type */
#define		  RELKIND_FOREIGN_TABLE   'f'		/* foreign table */
#define		  RELKIND_UNCATALOGED	  'u'		/* not yet cataloged */
#define		  RELKIND_AOSEGMENTS	  'o'		/* AO segment files and eof's */
#define		  RELKIND_AOBLOCKDIR	  'b'		/* AO block directory */
#define		  RELKIND_AOVISIMAP		  'm'		/* AO visibility map */

#define		  RELPERSISTENCE_PERMANENT	'p'		/* regular table */
#define		  RELPERSISTENCE_UNLOGGED	'u'		/* unlogged permanent table */
#define		  RELPERSISTENCE_TEMP		't'		/* temporary table */

/*
 * relstorage describes how a relkind is physically stored in the database.
 *
 * RELSTORAGE_HEAP    - stored on disk using heap storage.
 * RELSTORAGE_AOROWS  - stored on disk using append only storage.
 * RELSTORAGE_AOCOLS  - stored on dist using append only column storage.
 * RELSTORAGE_VIRTUAL - has virtual storage, meaning, relation has no
 *						data directly stored forit  (right now this
 *						relates to views and comp types).
 * RELSTORAGE_EXTERNAL-	stored externally using external tables.
 * RELSTORAGE_FOREIGN - stored in another server.  
 */
#define		  RELSTORAGE_HEAP	'h'
#define		  RELSTORAGE_AOROWS	'a'
#define 	  RELSTORAGE_AOCOLS	'c'
#define		  RELSTORAGE_PARQUET 'p'
#define		  RELSTORAGE_VIRTUAL	'v'
#define		  RELSTORAGE_EXTERNAL 'x'
#define		  RELSTORAGE_FOREIGN 'f'

static inline bool relstorage_is_heap(char c)
{
	return (c == RELSTORAGE_HEAP);
}

static inline bool relstorage_is_ao(char c)
{
	return (c == RELSTORAGE_AOROWS || c == RELSTORAGE_AOCOLS);
}

static inline bool relstorage_is_external(char c)
{
	return (c == RELSTORAGE_EXTERNAL);
}

static inline bool relstorage_is_foreign(char c)
{
	return (c == RELSTORAGE_FOREIGN);
}
#endif   /* PG_CLASS_H */
