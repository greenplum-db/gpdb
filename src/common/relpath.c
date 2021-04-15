/*-------------------------------------------------------------------------
 * relpath.c
 *		Shared frontend/backend code to compute pathnames of relation files
 *
 * This module also contains some logic associated with fork names.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/common/relpath.c
 *
 *-------------------------------------------------------------------------
 */
#ifndef FRONTEND
#include "postgres.h"
#include "utils/elog.h"
#else
#include "postgres_fe.h"
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "catalog/pg_tablespace_d.h"
#include "common/file_perm.h"
#include "common/relpath.h"
#include "pgtar.h"
#include "storage/backendid.h"


#ifndef FRONTEND
#define pg_log_info(...) elog(LOG, __VA_ARGS__)
#else
#include "common/logging.h"
#endif

/*
 * Lookup table of fork name by fork number.
 *
 * If you add a new entry, remember to update the errhint in
 * forkname_to_number() below, and update the SGML documentation for
 * pg_relation_size().
 */
const char *const forkNames[] = {
	"main",						/* MAIN_FORKNUM */
	"fsm",						/* FSM_FORKNUM */
	"vm",						/* VISIBILITYMAP_FORKNUM */
	"init"						/* INIT_FORKNUM */
};

/*
 * forkname_to_number - look up fork number by name
 *
 * In backend, we throw an error for no match; in frontend, we just
 * return InvalidForkNumber.
 */
ForkNumber
forkname_to_number(const char *forkName)
{
	ForkNumber	forkNum;

	for (forkNum = 0; forkNum <= MAX_FORKNUM; forkNum++)
		if (strcmp(forkNames[forkNum], forkName) == 0)
			return forkNum;

#ifndef FRONTEND
	ereport(ERROR,
			(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			 errmsg("invalid fork name"),
			 errhint("Valid fork names are \"main\", \"fsm\", "
					 "\"vm\", and \"init\".")));
#endif

	return InvalidForkNumber;
}

/*
 * forkname_chars
 *		We use this to figure out whether a filename could be a relation
 *		fork (as opposed to an oddly named stray file that somehow ended
 *		up in the database directory).  If the passed string begins with
 *		a fork name (other than the main fork name), we return its length,
 *		and set *fork (if not NULL) to the fork number.  If not, we return 0.
 *
 * Note that the present coding assumes that there are no fork names which
 * are prefixes of other fork names.
 */
int
forkname_chars(const char *str, ForkNumber *fork)
{
	ForkNumber	forkNum;

	for (forkNum = 1; forkNum <= MAX_FORKNUM; forkNum++)
	{
		int			len = strlen(forkNames[forkNum]);

		if (strncmp(forkNames[forkNum], str, len) == 0)
		{
			if (fork)
				*fork = forkNum;
			return len;
		}
	}
	if (fork)
		*fork = InvalidForkNumber;
	return 0;
}


/*
 * GetDatabasePath - construct path to a database directory
 *
 * Result is a palloc'd string.
 *
 * XXX this must agree with GetRelationPath()!
 */
char *
GetDatabasePath(Oid dbNode, Oid spcNode)
{
	if (spcNode == GLOBALTABLESPACE_OID)
	{
		/* Shared system relations live in {datadir}/global */
		Assert(dbNode == 0);
		return pstrdup("global");
	}
	else if (spcNode == DEFAULTTABLESPACE_OID)
	{
		/* The default tablespace is {datadir}/base */
		return psprintf("base/%u", dbNode);
	}
	else
	{
		/* All other tablespaces are accessed via symlinks */
		return psprintf("pg_tblspc/%u/%s/%u",
						spcNode, GP_TABLESPACE_VERSION_DIRECTORY, dbNode);
	}
}

/*
 * GetRelationPath - construct path to a relation's file
 *
 * Result is a palloc'd string.
 *
 * Note: ideally, backendId would be declared as type BackendId, but relpath.h
 * would have to include a backend-only header to do that; doesn't seem worth
 * the trouble considering BackendId is just int anyway.
 *
 * In PostgreSQL, the 'backendid' is embedded in the filename of temporary
 * relations. In GPDB, however, temporary relations are just prefixed with
 * "t_*", without the backend id. For compatibility with upstream code, this
 * function still takes 'backendid' as argument, but we only care whether
 * it's InvalidBackendId or not. If you need to construct the path of a
 * temporary relation, but don't know the real backend ID, pass
 * TempRelBackendId.
 */
char *
GetRelationPath(Oid dbNode, Oid spcNode, Oid relNode,
				int backendId, ForkNumber forkNumber)
{
	char	   *path;

	if (spcNode == GLOBALTABLESPACE_OID)
	{
		/* Shared system relations live in {datadir}/global */
		Assert(dbNode == 0);
		Assert(backendId == InvalidBackendId);
		if (forkNumber != MAIN_FORKNUM)
			path = psprintf("global/%u_%s",
							relNode, forkNames[forkNumber]);
		else
			path = psprintf("global/%u", relNode);
	}
	else if (spcNode == DEFAULTTABLESPACE_OID)
	{
		/* The default tablespace is {datadir}/base */
		if (backendId == InvalidBackendId)
		{
			if (forkNumber != MAIN_FORKNUM)
				path = psprintf("base/%u/%u_%s",
								dbNode, relNode,
								forkNames[forkNumber]);
			else
				path = psprintf("base/%u/%u",
								dbNode, relNode);
		}
		else
		{
			if (forkNumber != MAIN_FORKNUM)
				path = psprintf("base/%u/t_%u_%s",
								dbNode, relNode,
								forkNames[forkNumber]);
			else
				path = psprintf("base/%u/t_%u",
								dbNode, relNode);
		}
	}
	else
	{
		/* All other tablespaces are accessed via symlinks */
		if (backendId == InvalidBackendId)
		{
			if (forkNumber != MAIN_FORKNUM)
				path = psprintf("pg_tblspc/%u/%s/%u/%u_%s",
								spcNode, GP_TABLESPACE_VERSION_DIRECTORY,
								dbNode, relNode,
								forkNames[forkNumber]);
			else
				path = psprintf("pg_tblspc/%u/%s/%u/%u",
								spcNode, GP_TABLESPACE_VERSION_DIRECTORY,
								dbNode, relNode);
		}
		else
		{
			if (forkNumber != MAIN_FORKNUM)
				path = psprintf("pg_tblspc/%u/%s/%u/t_%u_%s",
								spcNode, GP_TABLESPACE_VERSION_DIRECTORY,
								dbNode, relNode,
								forkNames[forkNumber]);
			else
				path = psprintf("pg_tblspc/%u/%s/%u/t_%u",
								spcNode, GP_TABLESPACE_VERSION_DIRECTORY,
								dbNode, relNode);
		}
	}
	return path;
}

/*
 * Create a symlink to the segment's data directory in the specified subdir,
 * as an aid in debugging.  It helps map a randomly generated tablespace
 * subdir to the Greenplum segment that created it.
 */
void
link_gp_segment(const char *tblspcdir, const char *datadir)
{
	char *linkloc = psprintf("%s/%s", tblspcdir, GP_SEGMENT_LINK);
	if (symlink(datadir, linkloc) < 0)
	{
#ifndef FRONTEND
		elog(ERROR, "could not create symbolic link \"%s\": %m", linkloc);
#else
		pg_log_error("could not create symbolic link \"%s\": %m", linkloc);
		exit(EXIT_FAILURE);
#endif
	}
	pfree(linkloc);
}

void
unlink_gp_segment(const char *tblspcdir)
{
	char *linkloc = psprintf("%s/%s", tblspcdir, GP_SEGMENT_LINK);
	if (unlink(linkloc) < 0)
	{
#ifndef FRONTEND
		elog(LOG, "could not remove symbolic link \"%s\": %m", linkloc);
#else
		pg_log_info("could not remove symbolic link \"%s\": %m", linkloc);
#endif
	}
	pfree(linkloc);
}

/*
 * Create a unique segment-specific subdirectory under tablespace location
 *
 * Use the strong random number generation facility to create a unique
 * subdirectory under user-specified tablespace location.  It is possible that
 * two or more segments deployed on the same host choose the same name.  Such
 * conflicts are handled by generating another random number.  If all attempts
 * to resolve conflicts fail, NULL is returned.
 *
 * The generated subdir name is appended to the location.  The symbolic link
 * under pg_tblspc/ would have this path as its target.  The subdir name is 8
 * characters long.  False is returned non-conflicting subdir name cannog be
 * genenerated.
 */
bool
create_unique_subdir(char *location, const char *datadir)
{
	static bool seeded = false;
	bool done = false;
	uint32 randomNum;
	char subdirname[GP_TABLESPACE_DIR_LEN+1];
	int attemptsLeft = 5;
	int len = strlen(location);

	if (len + sizeof(subdirname) > MAX_TARABLE_SYMLINK_PATH_LENGTH)
	{
#ifndef FRONTEND
		elog(ERROR, "tablspace location %s is longer than %d characters",
			 location,
			 MAX_TARABLE_SYMLINK_PATH_LENGTH - (uint32) sizeof(subdirname));
#else
		pg_log_error("tablspace location %s is longer than %d characters",
					 location,
					 MAX_TARABLE_SYMLINK_PATH_LENGTH - (uint32) sizeof(subdirname));
		exit(EXIT_FAILURE);
#endif
	}

	while (attemptsLeft && !done)
	{
		if (!pg_strong_random(&randomNum, sizeof(randomNum)))
		{
			if (!seeded)
			{
				/* seed random number generator with PID */
				srandom(getpid());
				seeded = true;
			}
			randomNum = random();
		}
		snprintf(subdirname, sizeof(subdirname), "%08X", randomNum);
		join_path_components(location, location, subdirname);
		if (mkdir(location, pg_dir_create_mode) == 0)
		{
			done = true;
			link_gp_segment(location, datadir);
		}
		else
		{
			if (errno != EEXIST)
			{
#ifndef FRONTEND
				elog(ERROR, "could not create directory %s: %m",
					 location);
#else
				pg_log_error("could not create directory %s: %m",
							 location);
				exit(EXIT_FAILURE);
#endif
			}

			pg_log_info("subdir %s already exists, retrying", subdirname);
			/* Discard the conflicting name */
			get_parent_directory(location);
			attemptsLeft--;
		}
	}
	return done;
}
