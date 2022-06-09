/*-------------------------------------------------------------------------
 *
 * user.h
 *	  Commands for manipulating roles (formerly called users).
 *
 *
 * src/include/commands/user.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef USER_H
#define USER_H

#include "catalog/objectaddress.h"
#include "libpq/crypt.h"
#include "nodes/parsenodes.h"

/* Hook to check passwords in CreateRole() and AlterRole() */
typedef void (*check_password_hook_type) (const char *username, const char *shadow_pass, PasswordType password_type, Datum validuntil_time, bool validuntil_null);

extern PGDLLIMPORT check_password_hook_type check_password_hook;

extern Oid	CreateRole(CreateRoleStmt *stmt);
extern Oid	AlterRole(AlterRoleStmt *stmt);
extern Oid	AlterRoleSet(AlterRoleSetStmt *stmt);
extern void DropRole(DropRoleStmt *stmt);
extern void GrantRole(GrantRoleStmt *stmt);
extern Oid	RenameRole(const char *oldname, const char *newname);
extern void DropOwnedObjects(DropOwnedStmt *stmt);
extern void ReassignOwnedObjects(ReassignOwnedStmt *stmt);
extern List *roleNamesToIds(List *memberNames);

#endif   /* USER_H */
