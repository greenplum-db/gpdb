/*-------------------------------------------------------------------------
 *
 * extprotocolcmds.h
 *	  prototypes for extprotocolcmds.c.
 *
 *-------------------------------------------------------------------------
 */

#ifndef EXTPROTOCOLCMDS_H
#define EXTPROTOCOLCMDS_H

#include "nodes/parsenodes.h"

extern void DefineExtProtocol(List *name, List *parameters, bool trusted);
extern void RemoveExtProtocols(DropStmt *dtop);
extern void RemoveExtProtocolById(Oid protOid);
extern void AlterExtProtocolOwner(const char *name, Oid newOwnerId);
extern void RenameExtProtocol(const char *oldname, const char *newname);


#endif   /* EXTPROTOCOLCMDS_H */
