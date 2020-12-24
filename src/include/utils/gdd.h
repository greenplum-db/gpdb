/*-------------------------------------------------------------------------
 *
 * gdd.h
 *	  TODO file description
 *
 *
 * Copyright (c) 2018-Present VMware, Inc. or its affiliates.
 *
 *
 *-------------------------------------------------------------------------
 */

#ifndef GDD_H
#define GDD_H

extern int gp_global_deadlock_detector_period;

extern bool GlobalDeadLockDetectorStartRule(Datum main_arg);
extern void GlobalDeadLockDetectorMain(Datum main_arg);

#endif   /* GDD_H */

