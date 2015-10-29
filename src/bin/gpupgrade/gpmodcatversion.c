/*
 * gpmodcatversion
 *
 * Reads and modifies pg_control data.
 *
 * This is not used for 4.3, but we may be back to this in later versions.
 *
 * Copyright (c) 2007-2008, Greenplum inc
 * Copyright (c) 2007 PostgreSQL Global Development Group
 *
 * $PostgreSQL$
 */
#include "postgres.h"

#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

#include "catalog/catversion.h"
#include "catalog/pg_control.h"

struct verarray
{
	char *gpversion;
	uint32 vernum;
};

static struct verarray versions[] =
{
	{"4.3", CATALOG_VERSION_NO},
	{NULL, 0}
};

static void
usage(const char *progname)
{
	printf(_("%s modifies control information of a PostgreSQL database cluster.\n\n"), progname);
	printf(_("WARNING! This tool should only be used to upgrade/downgrade \n"
			 "Greenplum 3.0 and above systems!\n\n"));
	printf
		(
		 _(
		   "Usage:\n"
		   "  %s [OPTION] [DATADIR]\n\n"
		   "Options:\n"
		   "  --catversion [ CURRENT VERSION ]  replace catalog version with <version>\n"
		   "  --help                    show this help, then exit\n"
		   "  --version                 output version information, then exit\n"
		   ),
		 progname
		);
	printf(_("\nIf no data directory (DATADIR) is specified, "
			 "the environment variable PGDATA\nis used.\n\n"));
}


static const char *
dbState(DBState state)
{
	switch (state)
	{
		case DB_STARTUP:
			return _("starting up");
		case DB_SHUTDOWNED:
			return _("shut down");
		case DB_SHUTDOWNING:
			return _("shutting down");
		case DB_IN_CRASH_RECOVERY:
			return _("in crash recovery");
		case DB_IN_STANDBY_MODE:
			return _("in standby mode");
		case DB_IN_STANDBY_PROMOTED:
			return _("in standby mode (promoted)");
		case DB_IN_STANDBY_NEW_TLI_SET:
			return _("in standby mode (new tli set)");
		case DB_IN_PRODUCTION:
			return _("in production");
	}
	return _("unrecognized status code");
}


int
main(int argc, char *argv[])
{
	ControlFileData ControlFile;
	int			fd;
	char		ControlFilePath[MAXPGPATH];
	char	   *DataDir;
	pg_crc32	crc;
	const char *progname;
	int argno = 1;
	int targetversion = -1;
	uint32 tover;

	set_pglocale_pgservice(argv[0], "gpmodcatversion");

	progname = get_progname(argv[0]);

	if (argc > 1)
	{
		argno = 1;

		if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-?") == 0)
		{
			usage(progname);
			exit(0);
		}
		if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-V") == 0)
		{
			puts("gpmodcatversion (Greenplum Database) " PG_VERSION);
			exit(0);
		}
		if (strcmp(argv[1], "--catversion") == 0)
		{
			if (argc < 2)
			{
				/* we didn't get the version number */
				printf(_("\nA catalog version must be specified.\n\n"));
				usage(progname);
				exit(1);
			}
			else
			{
				char found = 0;

				for (targetversion = 0;
					 versions[targetversion].gpversion;
					 targetversion++)
				{
					if (strcmp(argv[2], versions[targetversion].gpversion) == 0)
					{
						found = 1;
						break;
					}
				}
				
				if (!found)
				{
					printf(_("\n\"%s\" is not a valid version to "
							 "upgrade/downgrade to.\n\n"), argv[2]);
					usage(progname);
					exit(1);
				}
				argno = 3;
			}
		}
	}

	if (argc > argno)
		DataDir = argv[argno];
	else
		DataDir = getenv("PGDATA");
	
	if (DataDir == NULL)
	{
		fprintf(stderr, _("%s: no data directory specified\n"), progname);
		fprintf(stderr, _("Try \"%s --help\" for more information.\n"), progname);
		usage(progname);
		exit(1);
	}

	snprintf(ControlFilePath, MAXPGPATH, "%s/global/pg_control", DataDir);

#define FLAGS (O_RDWR | O_EXCL | PG_BINARY)
	if ((fd = open(ControlFilePath, FLAGS, S_IRUSR | S_IWUSR)) == -1)
	{
		fprintf(stderr, _("%s: could not open file \"%s\" for reading: %s\n"),
				progname, ControlFilePath, strerror(errno));
		exit(2);
	}

	if (read(fd, &ControlFile, sizeof(ControlFileData)) != sizeof(ControlFileData))
	{
		fprintf(stderr, _("%s: could not read file \"%s\": %s\n"),
				progname, ControlFilePath, strerror(errno));
		exit(2);
	}

	/* Check the CRC. */
	INIT_CRC32C(crc);
	COMP_CRC32C(crc, &ControlFile, offsetof(ControlFileData, crc));
	FIN_CRC32C(crc);

	if (!EQ_LEGACY_CRC32(crc, ControlFile.crc))
	{
		/* Check the CRC using old algorithm. */
		INIT_LEGACY_CRC32(crc);
		COMP_LEGACY_CRC32(crc,
				   (char *) &ControlFile,
				   offsetof(ControlFileData, crc));
		FIN_LEGACY_CRC32(crc);

		if (!EQ_LEGACY_CRC32(crc, ControlFile.crc))
			printf(_("WARNING: Calculated CRC checksum does not match value stored in file.\n"
					 "Either the file is corrupt, or it has a different layout than this program\n"
					 "is expecting.  The results below are untrustworthy.\n\n"));
	}

	printf(_("\nCatalog version number: %u\n"),
		   ControlFile.catalog_version_no);

	tover = versions[targetversion].vernum;

	if (tover)
	{
		if (ControlFile.state != DB_SHUTDOWNED)
		{
			/* only upgrade shutdown systems */
			printf(_("\n\nFATAL ERROR\n\n"
					 "Invalid database state for upgrade: %s\n"
					 "Action: shutdown the database and try again.\n"),
				   dbState(ControlFile.state));
			close(fd);
			exit(1);
		}
		else if (ControlFile.catalog_version_no == tover)
		{
			/* must be something wrong if we want to do this */
			printf(_("\n\nWarning\n\n"
					 "Catalog version number is already set to %u\n\n"),
				   tover);
			close(fd);
			return 0;
		}
		
		printf(_("New catalog version number: %u\n"), tover);

		ControlFile.catalog_version_no = tover;

		/* recalcualte the CRC. */
		INIT_CRC32C(crc);
		COMP_CRC32C(crc, &ControlFile, offsetof(ControlFileData, crc));
		FIN_CRC32C(crc);
		/*
		INIT_LEGACY_CRC32(crc);
		COMP_LEGACY_CRC32(crc,
				   (char *) &ControlFile,
				   offsetof(ControlFileData, crc));
		FIN_LEGACY_CRC32(crc);
		*/
		ControlFile.crc = crc;

		printf(_("Setting version number to: %u\n"), tover);

		errno = 0;
		if (lseek(fd, 0, SEEK_SET) < 0)
		{
			printf(_("\n\nFATAL ERROR\n"
					 "could not seek control file: %s\n"),
					 strerror(errno));
			close(fd);
			exit(1);

		}
		if (write(fd, &ControlFile, sizeof(ControlFile)) != sizeof(ControlFile))
		{
			/* if write didn't set errno, assume problem is no disk space */
			if (errno == 0)
				errno = ENOSPC;

			printf(_("\n\nFATAL ERROR\n"
					 "could not write to control file: %s\n"),
					 strerror(errno));
			close(fd);
			exit(1);
		}

		if (close(fd))
		{
			printf(_("\n\nFATAL ERROR\n"
					 "could not close control file: %s\n"),
				   strerror(errno));
			exit(1);
		}
	}
	return 0;
}
