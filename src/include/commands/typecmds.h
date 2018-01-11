/*-------------------------------------------------------------------------
 *
 * typecmds.h
 *	  prototypes for typecmds.c.
 *
 *
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/commands/typecmds.h,v 1.25 2009/01/01 17:23:58 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef TYPECMDS_H
#define TYPECMDS_H

#include "catalog/dependency.h"
#include "nodes/parsenodes.h"


#define DEFAULT_TYPDELIM		','

extern void DefineType(List *names, List *parameters);
extern void RemoveTypes(DropStmt *drop);
extern void RemoveTypeById(Oid typeOid);
extern void DefineDomain(CreateDomainStmt *stmt);
extern void DefineEnum(CreateEnumStmt *stmt);
extern Oid	DefineCompositeType(const RangeVar *typevar, List *coldeflist);
extern Oid	AssignTypeArrayOid(Oid namespace, const char *arrayname);

extern void AlterDomainDefault(List *names, Node *defaultRaw);
extern void AlterDomainNotNull(List *names, bool notNull);
extern void AlterDomainAddConstraint(List *names, Node *constr);
extern void AlterDomainDropConstraint(List *names, const char *constrName,
						  DropBehavior behavior);

extern List *GetDomainConstraints(Oid typeOid);

extern void RenameType(List *names, const char *newTypeName);
extern void AlterTypeOwner(List *names, Oid newOwnerId);
extern void AlterTypeOwnerInternal(Oid typeOid, Oid newOwnerId,
					   bool hasDependEntry);
extern void AlterTypeNamespace(List *names, const char *newschema);
extern Oid  AlterTypeNamespace_oid(Oid typeOid, Oid nspOid, ObjectAddresses *objsMoved);
extern Oid  AlterTypeNamespaceInternal(Oid typeOid, Oid nspOid,
									   bool isImplicitArray,
									   bool errorOnTableType,
									   ObjectAddresses *objsMoved);
extern void AlterType(AlterTypeStmt *stmt);
extern void AlterType(AlterTypeStmt *stmt);

#endif   /* TYPECMDS_H */
