/*
 *  faultinjector.h
 *  
 *
 *  Copyright 2009-2010, Greenplum Inc. All rights reserved.
 *
 */

#ifndef FAULTINJECTOR_H
#define FAULTINJECTOR_H

#include "pg_config_manual.h"

#define FAULTINJECTOR_MAX_SLOTS	16

#define FAULT_NAME_MAX_LENGTH	256

/*
 *
 */
typedef enum FaultInjectorIdentifier_e {
#define FI_IDENT(id, str) id,
#include "utils/faultinjector_lists.h"
#undef FI_IDENT
	FaultInjectorIdMax
} FaultInjectorIdentifier_e;

typedef enum FaultInjectorType_e {
#define FI_TYPE(id, str) id,
#include "utils/faultinjector_lists.h"
#undef FI_TYPE
	FaultInjectorTypeMax
} FaultInjectorType_e;

/*
 *
 */
typedef enum DDLStatement_e {
#define FI_DDL_STATEMENT(id, str) id,
#include "utils/faultinjector_lists.h"
#undef FI_DDL_STATEMENT
	DDLMax
} DDLStatement_e;

/*
 *
 */
typedef enum FaultInjectorState_e {
#define FI_STATE(id, str) id,
#include "utils/faultinjector_lists.h"
#undef FI_STATE
	FaultInjectorStateMax
} FaultInjectorState_e;


/*
 *
 */
typedef struct FaultInjectorEntry_s {
	
	char						faultName[FAULT_NAME_MAX_LENGTH];

	FaultInjectorIdentifier_e	faultInjectorIdentifier;
	
	FaultInjectorType_e		faultInjectorType;
	
	int						sleepTime;
		/* in seconds, in use if fault injection type is sleep */
		
	DDLStatement_e			ddlStatement;
	
	char					databaseName[NAMEDATALEN];
	
	char					tableName[NAMEDATALEN];
	
	volatile	int			occurrence;
	volatile	 int	numTimesTriggered;
	volatile	FaultInjectorState_e	faultInjectorState;

		/* the state of the fault injection */
	char					bufOutput[2500];
	
} FaultInjectorEntry_s;


extern FaultInjectorType_e	FaultInjectorTypeStringToEnum(
									char*		faultTypeString);

extern	FaultInjectorIdentifier_e FaultInjectorIdentifierStringToEnum(
									char*			faultName);

extern DDLStatement_e FaultInjectorDDLStringToEnum(
									char*	ddlString);

extern Size FaultInjector_ShmemSize(void);

extern void FaultInjector_ShmemInit(void);

extern FaultInjectorType_e FaultInjector_InjectFaultNameIfSet(
							   const char*				 faultName,
							   DDLStatement_e			 ddlStatement,
							   const char*				 databaseName,
							   const char*				 tableName);

extern FaultInjectorType_e FaultInjector_InjectFaultIfSet(
							   FaultInjectorIdentifier_e identifier,
							   DDLStatement_e			 ddlStatement,
							   const char*				 databaseName,
							   const char*				 tableName);

extern int FaultInjector_SetFaultInjection(
							FaultInjectorEntry_s	*entry);


extern bool FaultInjector_IsFaultInjected(
							char* faultName);


#ifdef FAULT_INJECTOR
#define SIMPLE_FAULT_INJECTOR(FaultIdentifier) \
	FaultInjector_InjectFaultIfSet(FaultIdentifier, DDLNotSpecified, "", "")
#define SIMPLE_FAULT_NAME_INJECTOR(FaultName) \
	FaultInjector_InjectFaultNameIfSet(FaultName, DDLNotSpecified, "", "")
#else
#define SIMPLE_FAULT_INJECTOR(FaultIdentifier)
#define SIMPLE_FAULT_NAME_INJECTOR(FaultName)
#endif

#endif	/* FAULTINJECTOR_H */

