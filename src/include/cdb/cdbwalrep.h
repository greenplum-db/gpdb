/*
 * cdbwalrep.h
 *
 * Copyright (c) 2017-Present Pivotal Software, Inc.
 *
 */

#ifndef CDBWALREP_H
#define CDBWALREP_H
typedef enum PrimaryWalRepState
{
	PRIMARYWALREP_ARCHIVING = 0, /* (a) Synchronized mirror is offline */
	PRIMARYWALREP_CATCHUP, /* (c) Synchronized mirror is online and catching up */
	PRIMARYWALREP_STREAMING, /* (s) Synchronized mirror is online and in-sync */
	PRIMARYWALREP_SHUTDOWN, /* System is shutting down */
	PRIMARYWALREP_FAULT,
	PRIMARYWALREP_UNKNOWN
} PrimaryWalRepState;

extern void primaryWalRepStateShmemInit(void);
extern PrimaryWalRepState getPrimaryWalRepState(void);
extern PrimaryWalRepState getPrimaryWalRepStateFromWalSnd(void);
extern const char *getPrimaryWalRepStateLabel(PrimaryWalRepState state);
extern bool isPrimaryWalRepStateConsistent(void);
extern void setPrimaryWalRepState(PrimaryWalRepState state);
extern void WalWaitForSegmentConfigurationChange(void);
extern void WalUpdateStandbyState(PrimaryWalRepState state);
#endif
