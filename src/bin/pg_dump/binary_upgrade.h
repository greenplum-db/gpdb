/*-------------------------------------------------------------------------
 *
 * binary_upgrade.h
 *		Functions to create ArchiveEntries for Oid preassignment in dumps
 *
 * Portions Copyright 2017 Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/bin/pg_dump/binary_upgrade.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef BINARY_UPGRADE_H
#define BINARY_UPGRADE_H

#define QUERY_ALLOC 8192

#include "postgres_fe.h"
#include "pg_backup_archiver.h"
#include "libpq/libpq-fs.h"

extern void dumpTableOid(PGconn *conn, Archive *fout, Archive *AH, TableInfo *info);
extern void dumpIndexOid(PGconn *conn, Archive *AH, IndxInfo *info);
extern void dumpAttrDefsOid(Archive *AH, AttrDefInfo *info);
extern void dumpConversionOid(PGconn *conn, Archive *AH, ConvInfo *info);
extern void dumpOperatorOid(Archive *AH, OprInfo *info);
extern void dumpOpFamilyOid(PGconn *conn, Archive *AH, OpfamilyInfo *info);
extern void dumpOpClassOid(PGconn *conn, Archive *AH, OpclassInfo *info);
extern void dumpExternalProtocolOid(Archive *AH, ExtProtInfo *info);
extern void dumpProcedureOid(Archive *AH, FuncInfo *info);
extern void dumpShellTypeOid(PGconn *conn, Archive *fout, Archive *AH, ShellTypeInfo *info);
extern void dumpTypeOid(PGconn *conn, Archive *fout, Archive *AH, TypeInfo *info);
extern void dumpNamespaceOid(Archive *AH, NamespaceInfo *info);
extern void dumpRuleOid(Archive *AH, RuleInfo *info);
extern void dumpConstraintOid(PGconn *conn, Archive *AH, ConstraintInfo *info);
extern void dumpProcLangOid(PGconn *conn, Archive *fout, Archive *AH, ProcLangInfo *info);
extern void dumpCastOid(Archive *AH, CastInfo *info);
extern void dumpTSObjectOid(Archive *AH, DumpableObject *info);
extern void dumpExtensionOid(Archive *AH, ExtensionInfo *info);

#endif /* BINARY_UPGRADE_H */
