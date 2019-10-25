#include <stdarg.h>
#include <setjmp.h>
#include <stdlib.h>

#include "cmockery.h"

#include "data_checksum_mismatch.h"
#include "utilities/upgrade-helpers.h"
#include "utilities/test-helpers.h"
#include "bdd-library/bdd.h"

static void
setDataChecksum(char *binaryDirectory, char *dataDirectory, int checksumValue)
{
	char buffer[500];

	sprintf(buffer, "echo yes | %s/pg_resetxlog -k %d %s", binaryDirectory, checksumValue, dataDirectory);
	system(buffer);
}

static void
upgradeCheckFailsType1()
{
	performUpgradeCheckFailsWithError("old cluster uses data checksums but the new one does not\n");
}

static void
upgradeCheckFailsType2()
{
	performUpgradeCheckFailsWithError("old cluster does not use data checksums but the new one does\n");
}

static void
aFiveClusterWithoutChecksumsAndASixClusterWithChecksums()
{
	setDataChecksum("./gpdb5/bin", "./gpdb5-data/qddir/demoDataDir-1", 1);
	setDataChecksum("./gpdb6/bin", "./gpdb6-data/qddir/demoDataDir-1", 0);
}

static void
aFiveClusterWithChecksumsAndASixClusterWithoutChecksums()
{
	setDataChecksum("./gpdb5/bin", "./gpdb5-data/qddir/demoDataDir-1", 0);
	setDataChecksum("./gpdb6/bin", "./gpdb6-data/qddir/demoDataDir-1", 1);
}

void
test_clusters_with_different_checksum_version_cannot_be_upgraded(void ** state) {
	given(aFiveClusterWithoutChecksumsAndASixClusterWithChecksums);
	then(upgradeCheckFailsType1);
	given(aFiveClusterWithChecksumsAndASixClusterWithoutChecksums);
	then(upgradeCheckFailsType2);
}

