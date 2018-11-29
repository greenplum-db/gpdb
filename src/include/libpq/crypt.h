/*-------------------------------------------------------------------------
 *
 * crypt.h
 *	  Interface to libpq/crypt.c
 *
 * Portions Copyright (c) 1996-2014, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/libpq/crypt.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef PG_CRYPT_H
#define PG_CRYPT_H

#include "c.h"

#include "libpq/libpq-be.h"
#include "libpq/md5.h"

extern int hashed_passwd_verify(const Port *port, const char *role,
				 char *client_pass, char **logdetail);

#endif
