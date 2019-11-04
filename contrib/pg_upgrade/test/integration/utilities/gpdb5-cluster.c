#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "gpdb5-cluster.h"

void
loadGpdbFiveExtensionOldSyntax(const char *extension)
{
	char	*buf;
	size_t	 len = 0;
	int		 result;

	len += strlen(". $PWD/gpdb5/greenplum_path.sh; ");
	len += strlen("export PGPORT=50000; ");
	len += strlen("export MASTER_DATA_DIRECTORY=$PWD/gpdb5-data/qddir/demoDataDir-1; ");
	len += strlen("$PWD/gpdb5/bin/psql -d postgres -f ");
	len += strlen(extension);
	len += 1;

	buf = malloc(len);
	snprintf(buf, len, ""
		   ". $PWD/gpdb5/greenplum_path.sh; "
		   "export PGPORT=50000; "
		   "export MASTER_DATA_DIRECTORY=$PWD/gpdb5-data/qddir/demoDataDir-1; "
		   "$PWD/gpdb5/bin/psql -d postgres -f %s",
		   extension
	);

	result = system(buf);
	if (result != 0)
	{
		fprintf(stderr, "Failed to load extension: %s\n", extension);
		fprintf(stderr, "Issued command was: %s\n", buf);
		_Exit(2);		/* no exit() here, consult man 3 exit() for more */
	}
	free(buf);
}

void
startGpdbFiveCluster(void)
{
	system(""
		   ". $PWD/gpdb5/greenplum_path.sh; "
		   "export PGPORT=50000; "
		   "export MASTER_DATA_DIRECTORY=$PWD/gpdb5-data/qddir/demoDataDir-1; "
		   "$PWD/gpdb5/bin/gpstart -a --skip_standby_check --no_standby"
		);
}

void
stopGpdbFiveCluster(void)
{
	system(""
		   ". $PWD/gpdb5/greenplum_path.sh; \n"
		   "export PGPORT=50000; \n"
		   "export MASTER_DATA_DIRECTORY=$PWD/gpdb5-data/qddir/demoDataDir-1; \n"
		   "$PWD/gpdb5/bin/gpstop -af"
		);
}
