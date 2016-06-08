/*-------------------------------------------------------------------------
 *
 * namespace.h
 *	  prototypes for functions in backend/catalog/namespace.c
 *
 *
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/catalog/namespace.h,v 1.44 2007/01/05 22:19:52 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef NAMESPACE_H
#define NAMESPACE_H

#include "nodes/primnodes.h"
#include "storage/lock.h"

/*
 *	This structure holds a list of possible functions or operators
 *	found by namespace lookup.	Each function/operator is identified
 *	by OID and by argument types; the list must be pruned by type
 *	resolution rules that are embodied in the parser, not here.
 */
typedef struct _FuncCandidateList
{
	struct _FuncCandidateList *next;
	int			pathpos;		/* for internal use of namespace lookup */
	Oid			oid;			/* the function or operator's OID */
	int			nargs;			/* number of arg types returned */
	int			nvargs;			/* number of args to become variadic array */
	int         ndargs;         /* number of defaulted args */
	Oid			args[1];		/* arg types --- VARIABLE LENGTH ARRAY */
}	*FuncCandidateList;	/* VARIABLE LENGTH STRUCT */

typedef void (*RangeVarGetRelidCallback) (const RangeVar *relation, Oid relId,
										   Oid oldRelId, void *callback_arg);

extern Oid	RangeVarGetRelid(const RangeVar *relation, bool failOK);
extern Oid  RangeVarGetRelidExtended(const RangeVar *relation,
						 LOCKMODE lockmode, bool missing_ok, bool nowait,
						 RangeVarGetRelidCallback callback,
						 void *callback_arg);
extern Oid	RangeVarGetCreationNamespace(const RangeVar *newRelation);
extern Oid	RelnameGetRelid(const char *relname);
extern bool RelationIsVisible(Oid relid);

extern Oid	TypenameGetTypid(const char *typname);
extern bool TypeIsVisible(Oid typid);

extern FuncCandidateList FuncnameGetCandidates(List *names, int nargs, bool expand_variadic, bool expand_defaults);
extern bool FunctionIsVisible(Oid funcid);

extern Oid	OpernameGetOprid(List *names, Oid oprleft, Oid oprright);
extern FuncCandidateList OpernameGetCandidates(List *names, char oprkind);
extern bool OperatorIsVisible(Oid oprid);

extern Oid	OpclassnameGetOpcid(Oid amid, const char *opcname);
extern bool OpclassIsVisible(Oid opcid);

extern Oid	OpfamilynameGetOpfid(Oid amid, const char *opfname);
extern bool OpfamilyIsVisible(Oid opfid);

extern Oid	ConversionGetConid(const char *conname);
extern bool ConversionIsVisible(Oid conid);

extern void DeconstructQualifiedName(List *names,
						 char **nspname_p,
						 char **objname_p);
extern Oid	LookupExplicitNamespace(const char *nspname);

extern void DropTempTableNamespaceForResetSession(Oid namespaceOid);
extern void SetTempNamespace(Oid namespaceOid);
extern Oid  ResetTempNamespace(void);
extern bool TempNamespaceOidIsValid(void);  /* GPDB only:  used by cdbgang.c */
extern void InitTempTableNamespaceWithOids(Oid tempSchema);

extern Oid	LookupCreationNamespace(const char *nspname);
extern Oid	QualifiedNameGetCreationNamespace(List *names, char **objname_p);
extern RangeVar *makeRangeVarFromNameList(List *names);
extern char *NameListToString(List *names);
extern char *NameListToQuotedString(List *names);

extern bool isTempNamespace(Oid namespaceId);
extern bool isAnyTempNamespace(Oid namespaceId);
extern bool isOtherTempNamespace(Oid namespaceId);

extern void PushSpecialNamespace(Oid namespaceId);
extern void PopSpecialNamespace(Oid namespaceId);

extern Oid	FindConversionByName(List *conname);
extern Oid	FindDefaultConversionProc(int4 for_encoding, int4 to_encoding);

/* initialization & transaction cleanup code */
extern void InitializeSearchPath(void);
extern void AtEOXact_Namespace(bool isCommit);
extern void AtEOSubXact_Namespace(bool isCommit, SubTransactionId mySubid,
					  SubTransactionId parentSubid);

/* stuff for search_path GUC variable */
extern char *namespace_search_path;

extern List *fetch_search_path(bool includeImplicit);

#endif   /* NAMESPACE_H */
