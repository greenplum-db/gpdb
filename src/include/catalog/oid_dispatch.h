/*-------------------------------------------------------------------------
 *
 * oid_dispatch.h
 *	  prototypes for functions in backend/catalog/oid_dispatch.c
 *
 *
 * Portions Copyright 2016 VMware, Inc. or its affiliates.
 *
 * src/include/catalog/oid_dispatch.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef OID_DISPATCH_H
#define OID_DISPATCH_H

#include "utils/relcache.h"
#include "access/htup.h"

/* Functions used in master */
extern List *GetAssignedOidsForDispatch(void);

/* Functions used in QE nodes */
extern void AddPreassignedOids(List *l);
extern void AddPreassignedOidFromBinaryUpgrade(Oid oid, Oid catalog,
			char *objname, Oid namespaceOid, Oid keyOid1, Oid keyOid2);

/* Wrapper functions for GetNewOidWithIndex(), used in QD and QE nodes */
extern Oid GetNewOidForAccessMethod(Relation relation, Oid indexId, AttrNumber oidcolumn,
									char *amname);
extern Oid GetNewOidForAccessMethodOperator(Relation relation, Oid indexId, AttrNumber oidcolumn,
											Oid amopfamily, Oid amoplefttype, Oid amoprighttype,
											Oid amopstrategy);
extern Oid GetNewOidForAccessMethodProcedure(Relation relation, Oid indexId, AttrNumber oidcolumn,
											 Oid amprocfamily, Oid amproclefttype, Oid amprocrighttype,
											 Oid amproc);
extern Oid GetNewOidForAttrDefault(Relation relation, Oid indexId, AttrNumber oidcolumn,
								   Oid adrelid, int16 adnum);
extern Oid GetNewOidForAuthId(Relation relation, Oid indexId, AttrNumber oidcolumn,
							  char *rolname);
extern Oid GetNewOidForCast(Relation relation, Oid indexId, AttrNumber oidcolumn,
							Oid castsource, Oid casttarget);
extern Oid GetNewOidForCollation(Relation relation, Oid indexId, AttrNumber oidcolumn,
								 Oid collnamespace, char *collname);
extern Oid GetNewOidForConstraint(Relation relation, Oid indexId, AttrNumber oidcolumn,
								  Oid conrelid, Oid contypid, char *conname);
extern Oid GetNewOidForConversion(Relation relation, Oid indexId, AttrNumber oidcolumn,
								  Oid connamespace, char *conname);
extern Oid GetPreassignedOidForDatabase(const char *datname);
extern void RememberAssignedOidForDatabase(const char *datname, Oid oid);
extern Oid GetPreassignedOidForEnum(Oid enumtypid, const char *enumlabel);
extern void RememberAssignedOidForEnum(Oid enumtypid, const char *enumlabel, Oid oid);
extern Oid GetPreassignedOidForRelation(Oid namespaceId, const char *relname);
extern Oid GetPreassignedOidForType(Oid namespaceId, const char *typname);
extern Oid GetNewOidForDefaultAcl(Relation relation, Oid indexId, AttrNumber oidcolumn,
								  Oid defaclrole, Oid defaclnamespace, char defaclobjtype);
extern Oid GetNewOidForExtension(Relation relation, Oid indexId, AttrNumber oidcolumn,
								 char *extname);
extern Oid GetNewOidForExtprotocol(Relation relation, Oid indexId, AttrNumber oidcolumn,
								   char *ptcname);
extern Oid GetNewOidForForeignDataWrapper(Relation relation, Oid indexId, AttrNumber oidcolumn,
										  char *fdwname);
extern Oid GetNewOidForForeignServer(Relation relation, Oid indexId, AttrNumber oidcolumn,
									 char *srvname);
extern Oid GetNewOidForLanguage(Relation relation, Oid indexId, AttrNumber oidcolumn,
								char *lanname);
extern Oid GetNewOidForNamespace(Relation relation, Oid indexId, AttrNumber oidcolumn,
								 char *nspname);
extern Oid GetNewOidForOperator(Relation relation, Oid indexId, AttrNumber oidcolumn,
								char *oprname, Oid oprleft, Oid oprright, Oid oprnamespace);
extern Oid GetNewOidForOperatorClass(Relation relation, Oid indexId, AttrNumber oidcolumn,
									 Oid opcmethod, char *opcname, Oid opcnamespace);
extern Oid GetNewOidForOperatorFamily(Relation relation, Oid indexId, AttrNumber oidcolumn,
									  Oid opfmethod, char *opfname, Oid opfnamespace);
extern Oid GetNewOidForPolicy(Relation relation, Oid indexId, AttrNumber oidcolumn,
							  Oid polrelid, char *polname);
extern Oid GetNewOidForProcedure(Relation relation, Oid indexId, AttrNumber oidcolumn,
								 char *proname, oidvector *proargtypes, Oid pronamespace);
extern Oid GetNewOidForRelation(Relation relation, Oid indexId, AttrNumber oidcolumn,
								char *relname, Oid relnamespace);
extern Oid GetNewOidForResQueue(Relation relation, Oid indexId, AttrNumber oidcolumn,
								char *rsqname);
extern Oid GetNewOidForRewrite(Relation relation, Oid indexId, AttrNumber oidcolumn,
							   Oid ev_class, char *rulename);
extern Oid GetNewOidForSubscription(Relation relation, Oid indexId, AttrNumber oidcolumn,
									Oid subdbid, char *subname);
extern Oid GetNewOidForTableSpace(Relation relation, Oid indexId, AttrNumber oidcolumn,
								  char *spcname);
extern Oid GetNewOidForTransform(Relation relation, Oid indexId, AttrNumber oidcolumn,
								 Oid trftype, Oid trflang);
extern Oid GetNewOidForTrigger(Relation relation, Oid indexId, AttrNumber oidcolumn,
							   Oid tgrelid, char *tgname, Oid tgconstraint, Oid tgfid);
extern Oid GetNewOidForTSParser(Relation relation, Oid indexId, AttrNumber oidcolumn,
								char *prsname, Oid prsnamespace);
extern Oid GetNewOidForTSDictionary(Relation relation, Oid indexId, AttrNumber oidcolumn,
									char *dictname, Oid dictnamespace);
extern Oid GetNewOidForTSTemplate(Relation relation, Oid indexId, AttrNumber oidcolumn,
								  char *tmplname, Oid tmplnamespace);
extern Oid GetNewOidForTSConfig(Relation relation, Oid indexId, AttrNumber oidcolumn,
								char *cfgname, Oid cfgnamespace);
extern Oid GetNewOidForType(Relation relation, Oid indexId, AttrNumber oidcolumn,
							char *typname, Oid typnamespace);
extern Oid GetNewOidForResGroup(Relation relation, Oid indexId, AttrNumber oidcolumn,
								char *rsgname);
extern Oid GetNewOidForUserMapping(Relation relation, Oid indexId, AttrNumber oidcolumn,
								   Oid umuser, Oid umserver);
extern Oid GetNewOidForPublication(Relation relation, Oid indexId, AttrNumber oidcolumn,
								   char *pubname);
extern Oid GetNewOidForPublicationRel(Relation relation, Oid indexId, AttrNumber oidcolumn,
									  Oid prrelid, Oid prpubid);

extern char *GetPreassignedIndexNameForChildIndex(Oid parentIdxOid, Oid childRelId);
extern void RememberPreassignedIndexNameForChildIndex(Oid parentIdxOid, Oid childRelId,
													  const char *idxname);


/* Functions used in master and QE nodes */
extern void PreserveOidAssignmentsOnCommit(void);
extern void ClearOidAssignmentsOnCommit(void);

/* Functions used in binary upgrade */
extern bool IsOidAcceptable(Oid oid);
extern void MarkOidPreassignedFromBinaryUpgrade(Oid oid);

extern void AtEOXact_DispatchOids(bool isCommit);

#endif   /* OID_DISPATCH_H */
