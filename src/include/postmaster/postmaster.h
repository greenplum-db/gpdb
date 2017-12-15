/*-------------------------------------------------------------------------
 *
 * postmaster.h
 *	  Exports from postmaster/postmaster.c.
 *
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/postmaster/postmaster.h,v 1.20 2009/05/05 19:59:00 tgl Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef _POSTMASTER_H
#define _POSTMASTER_H

/* GUC options */
extern bool EnableSSL;
extern bool SilentMode;
extern int	ReservedBackends;
extern int	PostPortNumber;
extern int	Unix_socket_permissions;
extern char *Unix_socket_group;
extern char *UnixSocketDir;
extern char *ListenAddresses;
extern bool ClientAuthInProgress;
extern int	PreAuthDelay;
extern int	AuthenticationTimeout;
extern bool Log_connections;
extern bool log_hostname;
extern char *bonjour_name;

#ifdef WIN32
extern HANDLE PostmasterHandle;
#else
extern int	postmaster_alive_fds[2];

/*
 * Constants that represent which of postmaster_alive_fds is held by
 * postmaster, and which is used in children to check for postmaster death.
 */
#define POSTMASTER_FD_WATCH		0		/* used in children to check for
										 * postmaster death */
#define POSTMASTER_FD_OWN		1		/* kept open by postmaster only */
#endif

#define POSTMASTER_IN_STARTUP_MSG "the database system is starting up"
#define POSTMASTER_IN_RECOVERY_MSG "the database system is in recovery mode"

extern const char *progname;

/* stack base pointer, defined in postgres.c */
extern char *stack_base_ptr;

extern int	PostmasterMain(int argc, char *argv[]);
extern void ClosePostmasterPorts(bool am_syslogger);

extern int	MaxLivePostmasterChildren(void);

extern bool AreWeAMirror;

#ifdef EXEC_BACKEND
extern pid_t postmaster_forkexec(int argc, char *argv[]);
extern int	SubPostmasterMain(int argc, char *argv[]);

extern Size ShmemBackendArraySize(void);
extern void ShmemBackendArrayAllocation(void);
#endif

/* CDB */
typedef int (PMSubStartCallback)(void);
extern bool GPIsSegmentDatabase(void);
extern bool GPAreFileReplicationStructuresRequired(void);
extern int PostmasterGetMppLocalProcessCounter(void);

extern void StartMasterOrPrimaryPostmasterProcesses(void);
extern void SignalShutdownFilerepProcess(void);
extern void SignalShutdownFilerepBackendProcesses(void);
extern bool IsFilerepBackendsDoneShutdown(void);
extern void NotifyProcessesOfFilerepStateChange(void);
extern void StartFilerepProcesses(void);
extern bool IsFilerepProcessRunning(void);
extern void SetFilerepPeerResetResult(bool success);
extern bool IsDatabaseInRunMode(void);
extern char *processTransitionRequest_faultInject(
	void * inputBuf, int *offsetPtr, int length);

#endif   /* _POSTMASTER_H */
