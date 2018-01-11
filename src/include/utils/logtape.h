/*-------------------------------------------------------------------------
 *
 * logtape.h
 *	  Management of "logical tapes" within temporary files.
 *
 * See logtape.c for explanations.
 *
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/utils/logtape.h,v 1.18 2009/01/01 17:24:02 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */

#ifndef LOGTAPE_H
#define LOGTAPE_H

#include "utils/workfile_mgr.h"

typedef struct LogicalTapePos
{
	int64 blkNum;
	int64 offset;

} LogicalTapePos;

/* LogicalTapeSet and LogicalTape are opaque types whose details are not known outside logtape.c. */
typedef struct LogicalTape LogicalTape;
typedef struct LogicalTapeSet LogicalTapeSet;

/*
 * prototypes for functions in logtape.c
 */

extern LogicalTape *LogicalTapeCreate(LogicalTapeSet *lts, LogicalTape *lt); 
extern LogicalTapeSet *LogicalTapeSetCreate(int ntapes, bool del_on_close);
extern LogicalTapeSet *LogicalTapeSetCreate_File(ExecWorkFile *ewfile, int ntapes);
extern LogicalTapeSet *LoadLogicalTapeSetState(ExecWorkFile *pfile, ExecWorkFile *tapefile);

extern void LogicalTapeSetClose(LogicalTapeSet *lts, workfile_set *workset);
extern void LogicalTapeSetForgetFreeSpace(LogicalTapeSet *lts);

extern size_t LogicalTapeRead(LogicalTapeSet *lts, LogicalTape *lt, void *ptr, size_t size);
extern void LogicalTapeWrite(LogicalTapeSet *lts, LogicalTape *lt, void *ptr, size_t size);
extern void LogicalTapeFlush(LogicalTapeSet *lts, LogicalTape *lt, ExecWorkFile *pstatefile);
extern void LogicalTapeRewind(LogicalTapeSet *lts, LogicalTape *lt, bool forWrite);
extern void LogicalTapeFreeze(LogicalTapeSet *lts, LogicalTape *lt);
extern bool LogicalTapeBackspace(LogicalTapeSet *lts, LogicalTape *lt, size_t size);
extern bool LogicalTapeSeek(LogicalTapeSet *lts, LogicalTape *lt, LogicalTapePos *pos); 
extern void LogicalTapeTell(LogicalTapeSet *lts, LogicalTape *lt, LogicalTapePos *pos);
extern void LogicalTapeUnfrozenTell(LogicalTapeSet *lts, LogicalTape *lt, LogicalTapePos *pos);

extern long LogicalTapeSetBlocks(LogicalTapeSet *lts);
extern void LogicalTapeSetForgetFreeSpace(LogicalTapeSet *lts);

extern LogicalTape *LogicalTapeSetGetTape(LogicalTapeSet *lts, int tapenum);
extern LogicalTape *LogicalTapeSetDuplicateTape(LogicalTapeSet *lts, LogicalTape *lt);

#endif   /* LOGTAPE_H */
