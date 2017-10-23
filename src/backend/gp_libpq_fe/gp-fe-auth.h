/*-------------------------------------------------------------------------
 *
 * gp-fe-auth.h
 *
 *	  Definitions for network authentication routines
 *
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	    src/backend/gp_libpq_fe/gp-fe-auth.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef FE_AUTH_H
#define FE_AUTH_H

#include "gp-libpq-fe.h"
#include "gp-libpq-int.h"

#include "gp-libpq-use.h"

extern int	pg_fe_sendauth(AuthRequest areq, PGconn *conn);
extern char *pg_fe_getauthname(PQExpBuffer errorMessage);

#include "gp-libpq-unuse.h"

#endif   /* FE_AUTH_H */
