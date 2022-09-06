/*-------------------------------------------------------------------------
 *
 * md.h
 *	  magnetic disk storage manager public interface declarations.
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/storage/md.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef MD_H
#define MD_H

#include "storage/block.h"
#include "storage/relfilenode.h"
#include "storage/smgr.h"
#include "storage/sync.h"

/* md storage manager functionality */
extern void mdinit(void);
extern void mdclose(SMgrRelation reln, ForkNumber forknum);
extern void mdcreate(SMgrRelation reln, ForkNumber forknum, bool isRedo);
extern void mdcreate_ao(RelFileNodeBackend rnode, int32 segmentFileNum, bool isRedo);
extern bool mdexists(SMgrRelation reln, ForkNumber forknum);
extern void mdunlink(RelFileNodeBackend rnode, ForkNumber forkNum, bool isRedo, char relstorage);
extern void mdextend(SMgrRelation reln, ForkNumber forknum,
					 BlockNumber blocknum, char *buffer, bool skipFsync);
extern void mdprefetch(SMgrRelation reln, ForkNumber forknum,
					   BlockNumber blocknum);
extern void mdread(SMgrRelation reln, ForkNumber forknum, BlockNumber blocknum,
				   char *buffer);
extern void mdwrite(SMgrRelation reln, ForkNumber forknum,
					BlockNumber blocknum, char *buffer, bool skipFsync);
extern BlockNumber mdnblocks(SMgrRelation reln, ForkNumber forknum);
extern void mdtruncate(SMgrRelation reln, ForkNumber forknum,
					   BlockNumber nblocks);
extern void mdimmedsync(SMgrRelation reln, ForkNumber forknum);

extern void ForgetDatabaseSyncRequests(Oid dbid);
extern void DropRelationFiles(RelFileNodePendingDelete *delrels, int ndelrels, bool isRedo);

/* md sync callbacks */
extern int	mdsyncfiletag(const FileTag *ftag, char *path);
extern int	mdunlinkfiletag(const FileTag *ftag, char *path);
extern bool mdfiletagmatches(const FileTag *ftag, const FileTag *candidate);

extern void mdpreckpt(void);
extern void mdsync(void);
extern void mdpostckpt(void);

extern void SetForwardFsyncRequests(void);
extern void RememberFsyncRequest(RelFileNode rnode, ForkNumber forknum,
                                        BlockNumber segno, bool is_ao_segno);
extern void ForgetRelationFsyncRequests(RelFileNode rnode, ForkNumber forknum);
extern void ForgetDatabaseFsyncRequests(Oid dbid);
extern void DropRelationFiles(RelFileNodePendingDelete *delrels, int ndelrels, bool isRedo);

extern int	aosyncfiletag(const FileTag *ftag, char *path);
#endif							/* MD_H */
