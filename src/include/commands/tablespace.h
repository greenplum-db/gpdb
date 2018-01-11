/*-------------------------------------------------------------------------
 *
 * tablespace.h
 *		Tablespace management commands (create/drop tablespace).
 *
 *
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/commands/tablespace.h,v 1.20 2009/01/01 17:23:58 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef TABLESPACE_H
#define TABLESPACE_H

#include "access/xlog.h"
#include "nodes/parsenodes.h"
#include "storage/dbdirnode.h"

/* XLOG stuff */
#define XLOG_TBLSPC_CREATE		0x00
#define XLOG_TBLSPC_DROP		0x10

typedef struct xl_tblspc_create_rec
{
	Oid			ts_id;
	char		ts_path[1];		/* VARIABLE LENGTH STRING */
} xl_tblspc_create_rec;

typedef struct xl_tblspc_drop_rec
{
	Oid			ts_id;
	char		ts_path[1];		/* VARIABLE LENGTH STRING */
} xl_tblspc_drop_rec;


extern void CreateTableSpace(CreateTableSpaceStmt *stmt);
extern void DropTableSpace(DropTableSpaceStmt *stmt);
extern void RenameTableSpace(const char *oldname, const char *newname);
extern void AlterTableSpaceOwner(const char *name, Oid newOwnerId);

extern void TablespaceCreateDbspace(Oid spcNode, Oid dbNode, bool isRedo);

extern Oid	GetDefaultTablespace(bool forTemp);

extern void PrepareTempTablespaces(void);

extern Oid	get_tablespace_oid(const char *tablespacename, bool missing_ok);
extern char *get_tablespace_name(Oid spc_oid);

extern bool directory_is_empty(const char *path);

extern void tblspc_redo(XLogRecPtr beginLoc, XLogRecPtr lsn, XLogRecord *rptr);
extern void tblspc_desc(StringInfo buf, XLogRecPtr beginLoc, XLogRecord *record);
extern void set_short_version(const char *path, DbDirNode *dbDirNode,
							  bool mirror);

#endif   /* TABLESPACE_H */
