/*-------------------------------------------------------------------------
 *
 * binary_upgradeall.h
 *		Functions to dump Oid dispatch commands from pg_dumpall
 *
 * Portions Copyright 2017 Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/bin/pg_dump/binary_upgradeall.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef BINARY_UPGRADEALL_H
#define BINARY_UPGRADEALL_H

#include <stdio.h>
#include <stdlib.h>

#include "postgres_fe.h"
#include "libpq-fe.h"
#include "libpq/libpq-fs.h"

#define QUERY_ALLOC 8192

#define atooid(x)  ((Oid) strtoul((x), NULL, 10))

extern void dumpFilespaceOid(FILE *OPF, PGconn *conn, char *fsname);
extern void dumpTablespaceOid(FILE *OPF, PGconn *conn, char *tsname);
extern void dumpResqueueOid(FILE *OPF, PGconn *conn, int server_version, Oid rqoid, char *rsqname);
extern void dumpRoleOid(FILE *OPF, Oid roleoid, const char *rolename);
extern void dumpDatabaseOid(FILE *OPF, Oid dboid, const char *fdbname);

#endif /* BINARY_UPGRADEALL_H */
