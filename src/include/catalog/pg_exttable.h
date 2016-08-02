/*-------------------------------------------------------------------------
*
* pg_exttable.h
*	  definitions for system wide external relations
*
* Copyright (c) 2007-2010, Greenplum inc
*
*-------------------------------------------------------------------------
*/

#ifndef PG_EXTTABLE_H
#define PG_EXTTABLE_H

#include "catalog/genbki.h"
#include "nodes/pg_list.h"

/*
 * pg_exttable definition.
 */

/* ----------------
 *		pg_exttable definition.  cpp turns this into
 *		typedef struct FormData_pg_exttable
 * ----------------
 */
#define ExtTableRelationId	6040

CATALOG(pg_exttable,6040) BKI_WITHOUT_OIDS
{
	Oid		reloid;				/* refers to this relation's oid in pg_class  */
	text	location[1];		/* array of URI strings */
	char	fmttype;			/* 't' (text) or 'c' (csv) */
	text	fmtopts;			/* the data format options */
	text	command;			/* the command string to EXECUTE */
	int4	rejectlimit;		/* error count reject limit per segment */
	char	rejectlimittype;	/* 'r' (rows) or 'p' (percent) */
	Oid		fmterrtbl;			/* the data format error table oid in pg_class */
	int4	encoding;			/* character encoding of this external table */
	bool	writable;			/* 't' if writable, 'f' if readable */
} FormData_pg_exttable;

/* GPDB added foreign key definitions for gpcheckcat. */
FOREIGN_KEY(reloid REFERENCES pg_class(oid));
FOREIGN_KEY(fmterrtbl REFERENCES pg_class(oid));

/* ----------------
 *		Form_pg_exttable corresponds to a pointer to a tuple with
 *		the format of pg_exttable relation.
 * ----------------
 */
typedef FormData_pg_exttable *Form_pg_exttable;


/* ----------------
 *		compiler constants for pg_exttable
 * ----------------
 */
#define Natts_pg_exttable					10
#define Anum_pg_exttable_reloid				1
#define Anum_pg_exttable_location			2
#define Anum_pg_exttable_fmttype			3
#define Anum_pg_exttable_fmtopts			4
#define Anum_pg_exttable_command			5
#define Anum_pg_exttable_rejectlimit		6
#define Anum_pg_exttable_rejectlimittype	7
#define Anum_pg_exttable_fmterrtbl			8
#define Anum_pg_exttable_encoding			9
#define Anum_pg_exttable_writable			10


/*
 * Descriptor of a single AO relation.
 * For now very similar to the catalog row itself but may change in time.
 */
typedef struct ExtTableEntry
{
	List*	locations;
	char	fmtcode;
	char*	fmtopts;
	char*	command;
	int		rejectlimit;
	char	rejectlimittype;
	Oid		fmterrtbl;
    int		encoding;
    bool	iswritable;
    bool	isweb;		/* extra state, not cataloged */
} ExtTableEntry;

/* No initial contents. */

extern void InsertExtTableEntry(Oid 	tbloid,
					bool 	iswritable,
					bool 	isweb,
					bool	issreh,
					char	formattype,
					char	rejectlimittype,
					char*	commandString,
					int		rejectlimit,
					Oid		fmtErrTblOid,
					int		encoding,
					Datum	formatOptStr,
					Datum	locationExec,
					Datum	locationUris);

extern ExtTableEntry *GetExtTableEntry(Oid relid);
extern ExtTableEntry *GetExtTableEntryIfExists(Oid relid);

extern void RemoveExtTableEntry(Oid relid);

#define fmttype_is_custom(c) (c == 'b' || c == 'a' || c == 'p')
#define fmttype_is_avro(c) (c == 'a')
#define fmttype_is_parquet(c) (c == 'p')
#define fmttype_is_text(c)   (c == 't')
#define fmttype_is_csv(c)    (c == 'c')

#endif /* PG_EXTTABLE_H */
