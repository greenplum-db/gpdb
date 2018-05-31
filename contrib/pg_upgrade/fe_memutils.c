/*-------------------------------------------------------------------------
 *
 * fe_memutils.c
 *	  memory management support for frontend code
 *
 * Portions Copyright (c) 1996-2013, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/common/fe_memutils.c
 *
 *-------------------------------------------------------------------------
 */

/*
 * GPDB_93_MERGE_FIXME: Remove this when 8396447cdbdff0b62 is merged. At that
 * point the frontend library for util functions is introduced which include
 * fe_memutils.c et.al. This is a hacked-up version only for pg_upgrade until
 * we reach 9.3.
 */

#ifndef FRONTEND
#error "This file is not expected to be compiled for backend code"
#endif

#include "postgres_fe.h"
#include "pg_upgrade.h"

void *
pg_malloc(size_t size)
{
	void	   *tmp;

	/* Avoid unportable behavior of malloc(0) */
	if (size == 0)
		size = 1;
	tmp = malloc(size);
	if (!tmp)
	{
		fprintf(stderr, _("out of memory\n"));
		exit(EXIT_FAILURE);
	}
	return tmp;
}

void *
pg_malloc0(size_t size)
{
	void	   *tmp;

	tmp = pg_malloc(size);
	MemSet(tmp, 0, size);
	return tmp;
}

void *
pg_realloc(void *ptr, size_t size)
{
	void	   *tmp;

	/* Avoid unportable behavior of realloc(NULL, 0) */
	if (ptr == NULL && size == 0)
		size = 1;
	tmp = realloc(ptr, size);
	if (!tmp)
	{
		fprintf(stderr, _("out of memory\n"));
		exit(EXIT_FAILURE);
	}
	return tmp;
}

/*
 * "Safe" wrapper around strdup().
 */
char *
pg_strdup(const char *in)
{
	char	   *tmp;

	if (!in)
	{
		fprintf(stderr,
				_("cannot duplicate null pointer (internal error)\n"));
		exit(EXIT_FAILURE);
	}
	tmp = strdup(in);
	if (!tmp)
	{
		fprintf(stderr, _("out of memory\n"));
		exit(EXIT_FAILURE);
	}
	return tmp;
}

void
pg_free(void *ptr)
{
	if (ptr != NULL)
		free(ptr);
}
