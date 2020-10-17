/*-------------------------------------------------------------------------
 *
 * auth.h
 *	  Definitions for network authentication routines
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/libpq/auth.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef AUTH_H
#define AUTH_H

#include "libpq/libpq-be.h"
#include "utils/date.h"

extern char *pg_krb_server_keyfile;
extern bool pg_krb_caseins_users;
extern char *pg_krb_realm;

extern void ClientAuthentication(Port *port);
extern void FakeClientAuthentication(Port *port);  /* GPDB only */

/* Hook for plugins to get control in ClientAuthentication() */
typedef void (*ClientAuthentication_hook_type) (Port *, int);
extern PGDLLIMPORT ClientAuthentication_hook_type ClientAuthentication_hook;

/*
 * Support for time-based authentication
 *  
 * Used by auth.c for comparing current time to the contents of 
 * pg_auth_time_constraint for acl enforcement
 * Used by user.c for comparing incoming changes to the contents of
 * pg_auth_time_constraint for acl modification
 */
typedef struct authPoint
{
    int16 day;
    TimeADT time;
} authPoint;

typedef struct authInterval
{
    authPoint start;
    authPoint end;
} authInterval;

extern void timestamptz_to_point(TimestampTz in, authPoint *out);
extern int point_cmp(const authPoint *a, const authPoint *b);
extern bool interval_overlap(const authInterval *a, const authInterval *b);
extern bool interval_contains(const authInterval *interval, const authPoint *point);
extern int CheckAuthTimeConstraints(char *rolname);
extern int check_auth_time_constraints_internal(char *rolname, TimestampTz timestamp);

#endif							/* AUTH_H */
