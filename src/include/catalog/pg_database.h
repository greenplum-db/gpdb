/*-------------------------------------------------------------------------
 *
 * pg_database.h
 *	  definition of the system "database" relation (pg_database)
 *	  along with the relation's initial contents.
 *
 *
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/catalog/pg_database.h,v 1.49 2009/01/01 17:23:57 momjian Exp $
 *
 * NOTES
 *	  the genbki.sh script reads this file and generates .bki
 *	  information from the DATA() statements.
 *
 *-------------------------------------------------------------------------
 */
#ifndef PG_DATABASE_H
#define PG_DATABASE_H

#include "catalog/genbki.h"

/* ----------------
 *		pg_database definition.  cpp turns this into
 *		typedef struct FormData_pg_database
 * ----------------
 */
#define DatabaseRelationId	1262

CATALOG(pg_database,1262) BKI_SHARED_RELATION
{
	NameData	datname;		/* database name */
	Oid			datdba;			/* owner of database */
	int4		encoding;		/* character encoding */
	NameData	datcollate;		/* LC_COLLATE setting */
	NameData	datctype;		/* LC_CTYPE setting */
	bool		datistemplate;	/* allowed as CREATE DATABASE template? */
	bool		datallowconn;	/* new connections allowed? */
	int4		datconnlimit;	/* max connections allowed (-1=no limit) */
	Oid			datlastsysoid;	/* highest OID to consider a system OID */
	TransactionId datfrozenxid; /* all Xids < this are frozen in this DB */
	Oid			dattablespace;	/* default table space for this DB */
	text		datconfig[1];	/* database-specific GUC (VAR LENGTH) */
	aclitem		datacl[1];		/* access permissions (VAR LENGTH) */
} FormData_pg_database;

/* GPDB added foreign key definitions for gpcheckcat. */
FOREIGN_KEY(datdba REFERENCES pg_authid(oid));
FOREIGN_KEY(dattablespace REFERENCES pg_tablespace(oid));

/* ----------------
 *		Form_pg_database corresponds to a pointer to a tuple with
 *		the format of pg_database relation.
 * ----------------
 */
typedef FormData_pg_database *Form_pg_database;

/* ----------------
 *		compiler constants for pg_database
 * ----------------
 */
#define Natts_pg_database				13
#define Anum_pg_database_datname		1
#define Anum_pg_database_datdba			2
#define Anum_pg_database_encoding		3
#define Anum_pg_database_datcollate		4
#define Anum_pg_database_datctype		5
#define Anum_pg_database_datistemplate	6
#define Anum_pg_database_datallowconn	7
#define Anum_pg_database_datconnlimit	8
#define Anum_pg_database_datlastsysoid	9
#define Anum_pg_database_datfrozenxid	10
#define Anum_pg_database_dattablespace	11
#define Anum_pg_database_datconfig		12
#define Anum_pg_database_datacl			13

DATA(insert OID = 1 (  template1 PGUID ENCODING "LC_COLLATE" "LC_CTYPE" t t -1 0 0 1663 _null_ _null_));
SHDESCR("default template database");
#define TemplateDbOid			1

#define Schema_pg_database \
{ 1262, {"datname"}       ,   19,  -1 , NAMEDATALEN,  1, 0, -1, -1 , false, 'p', 'i',  true, false, false, true, 0}, \
{ 1262, {"datdba"}        ,   26,  -1 ,           4,  2, 0, -1, -1 ,  true, 'p', 'i',  true, false, false, true, 0}, \
{ 1262, {"encoding"}      ,   23,  -1 ,           4,  3, 0, -1, -1 ,  true, 'p', 'i',  true, false, false, true, 0}, \
{ 1262, {"datcollate"}    ,   19,  -1 , NAMEDATALEN,  4, 0, -1, -1 , false, 'p', 'i',  true, false, false, true, 0}, \
{ 1262, {"datctype"}      ,   19,  -1 , NAMEDATALEN,  5, 0, -1, -1 , false, 'p', 'i',  true, false, false, true, 0}, \
{ 1262, {"datistemplate"} ,   16,  -1 ,           1,  6, 0, -1, -1 ,  true, 'p', 'c',  true, false, false, true, 0}, \
{ 1262, {"datallowconn"}  ,   16,  -1 ,           1,  7, 0, -1, -1 ,  true, 'p', 'c',  true, false, false, true, 0}, \
{ 1262, {"datconnlimit"}  ,   23,  -1 ,           4,  8, 0, -1, -1 ,  true, 'p', 'i',  true, false, false, true, 0}, \
{ 1262, {"datlastsysoid"} ,   26,  -1 ,           4,  9, 0, -1, -1 ,  true, 'p', 'i',  true, false, false, true, 0}, \
{ 1262, {"datfrozenxid"}  ,   28,  -1 ,           4, 10, 0, -1, -1 ,  true, 'p', 'i',  true, false, false, true, 0}, \
{ 1262, {"dattablespace"} ,   26,  -1 ,           4, 11, 0, -1, -1 ,  true, 'p', 'i',  true, false, false, true, 0}, \
{ 1262, {"datconfig"}     , 1009,  -1 ,          -1, 12, 1, -1, -1 , false, 'x', 'i', false, false, false, true, 0}, \
{ 1262, {"datacl"}        , 1034,  -1 ,          -1, 13, 1, -1, -1 , false, 'x', 'i', false, false, false, true, 0}

#endif   /* PG_DATABASE_H */
