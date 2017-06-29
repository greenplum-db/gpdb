/*-------------------------------------------------------------------------
 *
 * vmem_tracker.h
 *	  This file contains declarations for vmem tracking functions.
 *
 * Copyright (c) 2014, Pivotal Inc.
 */
#ifndef VMEMTRACKER_H
#define VMEMTRACKER_H

#include "nodes/nodes.h"

/*
 * The cleanupCountdown in the SessionState determines how many
 * processes we need to cleanup to declare a session clean. If it
 * reaches 0, we mark the session clean. However, -1 indicates
 * that the session is either done cleaning previous runaway event
 * or it never started a cleaning.
 */
#define CLEANUP_COUNTDOWN_BEFORE_RUNAWAY -1

typedef enum MemoryAllocationStatus
{
	MemoryAllocation_Success,
	MemoryFailure_VmemExhausted,
	MemoryFailure_SystemMemoryExhausted,
	MemoryFailure_QueryMemoryExhausted,
	MemoryFailure_ResourceGroupMemoryExhausted
} MemoryAllocationStatus;

typedef int64 EventVersion;

extern int runaway_detector_activation_percent;

extern int32 VmemTracker_ConvertVmemChunksToMB(int chunks);
extern int32 VmemTracker_ConvertVmemMBToChunks(int mb);
extern int64 VmemTracker_ConvertVmemChunksToBytes(int chunks);
extern int32 VmemTracker_GetReservedVmemChunks(void);
extern int64 VmemTracker_GetReservedVmemBytes(void);
extern int64 VmemTracker_GetMaxReservedVmemChunks(void);
extern int64 VmemTracker_GetMaxReservedVmemMB(void);
extern int64 VmemTracker_GetMaxReservedVmemBytes(void);
extern int64 VmemTracker_GetVmemLimitBytes(void);
extern int32 VmemTracker_GetVmemLimitChunks(void);
extern int32 VmemTracker_GetChunkSizeInBits(void);
extern int32 VmemTracker_GetAvailableVmemMB(void);
extern int64 VmemTracker_GetAvailableVmemBytes(void);
extern int32 VmemTracker_GetAvailableQueryVmemMB(void);
extern void VmemTracker_ShmemInit(void);
extern void VmemTracker_Init(void);
extern void VmemTracker_Shutdown(void);
extern void VmemTracker_ResetMaxVmemReserved(void);
extern MemoryAllocationStatus VmemTracker_ReserveVmem(int64 newly_requested);
extern void VmemTracker_ReleaseVmem(int64 to_be_freed_requested);
extern void VmemTracker_RequestWaiver(int64 waiver_bytes);
extern void VmemTracker_ResetWaiver(void);
extern int64 VmemTracker_Fault(int32 reason, int64 arg);

extern int32 RedZoneHandler_GetRedZoneLimitChunks(void);
extern int32 RedZoneHandler_GetRedZoneLimitMB(void);
extern bool RedZoneHandler_IsVmemRedZone(void);
extern void RedZoneHandler_DetectRunawaySession(void);
extern void RunawayCleaner_RunawayCleanupDoneForSession(void);
extern void RunawayCleaner_RunawayCleanupDoneForProcess(bool ignoredCleanup);
extern void RedZoneHandler_LogVmemUsageOfAllSessions(void);

extern void IdleTracker_ActivateProcess(void);
extern void IdleTracker_DeactivateProcess(void);

#endif   /* VMEMTRACKER_H */
