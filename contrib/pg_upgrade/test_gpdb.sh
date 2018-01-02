#!/bin/bash

# contrib/pg_upgrade/test_gpdb.sh
#
# Test driver for upgrading a Greenplum cluster with pg_upgrade. For test data,
# this script assumes the gpdemo cluster in gpAux/gpdemo/datadirs contains the
# end-state of an ICW test run. Performs a pg_dumpall, initializes a parallel
# gpdemo cluster and upgrades it against the ICW cluster and then performs
# another pg_dumpall. If the two dumps match then the upgrade created a new
# identical copy of the cluster.

OLD_BINDIR=
OLD_DATADIR=
NEW_BINDIR=
NEW_DATADIR=

DEMOCLUSTER_OPTS=
PGUPGRADE_OPTS=

qddir=

# The normal ICW run has a gpcheckcat call, so allow this testrunner to skip
# running it in case it was just executed to save time.
gpcheckcat=1

# gpdemo can create a cluster without mirrors, and if such a cluster should be
# upgraded then mirror upgrading must be turned off as it otherwise will report
# a failure.
mirrors=0

# Smoketesting pg_upgrade is done by just upgrading the QD without diffing the
# results. This is *NOT* a test of whether pg_upgrade can successfully upgrade
# a cluster but a test intended to catch when objects aren't properly handled
# in pg_dump/pg_upgrade wrt Oid synchronization
smoketest=0

# Not all platforms have a realpath binary in PATH, most notably macOS doesn't,
# so provide an alternative implementation. Returns an absolute path in the
# variable reference passed as the first parameter.  Code inspired by:
# http://stackoverflow.com/questions/3572030/bash-script-absolute-path-with-osx
realpath()
{
	local __ret=$1
	local path

	if [[ $2 = /* ]]; then
		path="$2"
	else
		path="$PWD/${2#./}"
	fi

	eval $__ret="'$path'"
}

restore_cluster()
{
	# Reset the pg_control files from the old cluster which were renamed
	# .old by pg_upgrade to avoid booting up an upgraded cluster.
	find ${OLD_DATADIR} -type f -name 'pg_control.old' |
	while read control_file; do
		mv "${control_file}" "${control_file%.old}"
	done

	# Remove the copied lalshell unless we're running in the gpdemo
	# directory where it's version controlled
	if ! git ls-files lalshell --error-unmatch >/dev/null 2>&1; then
		rm -f lalshell
	fi

	# Remove configuration files created by setting up the new cluster
	rm -f clusterConfigPostgresAddonsFile
	rm -f clusterConfigFile
	rm -f gpdemo-env.sh
	
	# Remove the temporary cluster
	rm -rf "$temp_root"
}

upgrade_qd()
{
	mkdir -p $1

	# Run pg_upgrade
	pushd $1
	time ${NEW_BINDIR}/pg_upgrade --old-bindir=${OLD_BINDIR} --old-datadir=$2 --new-bindir=${NEW_BINDIR} --new-datadir=$3 --dispatcher-mode ${PGUPGRADE_OPTS}
	if (( $? )) ; then
		echo "ERROR: Failure encountered in upgrading qd node"
		exit 1
	fi
	popd

	# Remember where we were when we upgraded the QD node. pg_upgrade generates
	# some files there that we need to copy to QE nodes.
	qddir=$1
}

upgrade_segment()
{
	mkdir -p $1

	# Copy the OID files from the QD to segments.
	cp $qddir/pg_upgrade_dump_*_oids.sql $1

	# Run pg_upgrade
	pushd $1
	time ${NEW_BINDIR}/pg_upgrade --old-bindir=${OLD_BINDIR} --old-datadir=$2 --new-bindir=${NEW_BINDIR} --new-datadir=$3 ${PGUPGRADE_OPTS}
	if (( $? )) ; then
		echo "ERROR: Failure encountered in upgrading node"
		exit 1
	fi
	popd
}

usage()
{
	appname=`basename $0`
	echo "$appname usage:"
	echo " -o <dir>     Directory containing old datadir"
	echo " -b <dir>     Directory containing binaries"
	echo " -s           Run smoketest only"
	echo " -C           Skip gpcheckcat test"
	echo " -k           Add checksums to new cluster"
	echo " -K           Remove checksums during upgrade"
	echo " -m           Upgrade mirrors"
	exit 0
}

# Main
temp_root=`pwd`/tmp_check

while getopts ":o:b:sCkKm" opt; do
	case ${opt} in
		o )
			realpath OLD_DATADIR "${OPTARG}"
			;;
		b )
			realpath NEW_BINDIR "${OPTARG}"
			realpath OLD_BINDIR "${OPTARG}"
			;;
		s )
			smoketest=1
			;;
		C )
			gpcheckcat=0
			;;
		k )
			add_checksums=1
			PGUPGRADE_OPTS=' -J '
			;;
		K )
			remove_checksums=1
			DEMOCLUSTER_OPTS=' -K '
			PGUPGRADE_OPTS=' -j '
			;;
		m )
			mirrors=1
			;;
		* )
			usage
			;;
	esac
done

if [ -z "${OLD_DATADIR}" ] || [ -z "${NEW_BINDIR}" ]; then
	usage
fi

if [ ! -z "${add_checksums}"] && [ ! -z "${remove_checksums}" ]; then
	echo "ERROR: adding and removing checksums are mutually exclusive"
	exit 1
fi

rm -rf "$temp_root"
mkdir -p "$temp_root"
if [ ! -d "$temp_root" ]; then
	echo "ERROR: unable to create workdir: $temp_root"
	exit 1
fi

trap restore_cluster EXIT

# The cluster should be running by now, but in case it isn't, issue a restart.
# Worst case we powercycle once for no reason, but it's better than failing
# due to not having a cluster to work with.
gpstart -a

# Run any pre-upgrade tasks to prep the cluster
if [ -f "test_gpdb_pre.sql" ]; then
	psql -f test_gpdb_pre.sql regression
fi

# Ensure that the catalog is sane before attempting an upgrade. While there is
# (limited) catalog checking inside pg_upgrade, it won't catch all issues, and
# upgrading a faulty catalog won't work.
if (( $gpcheckcat )) ; then
	gpcheckcat
		if (( $? )) ; then
		echo "ERROR: gpcheckcat reported catalog issues, fix before upgrading"
		exit 1
	fi
fi

if (( !$smoketest )) ; then
	${NEW_BINDIR}/pg_dumpall --schema-only -f "$temp_root/dump1.sql"
fi

gpstop -a

# Create a new gpdemo cluster in the temproot. Using the old datadir for the
# path to demo_cluster.sh is a bit of a hack, but since this test relies on
# gpdemo having been used for ICW it will do for now.
export MASTER_DEMO_PORT=17432
export DEMO_PORT_BASE=27432
export NUM_PRIMARY_MIRROR_PAIRS=3
export MASTER_DATADIR=${temp_root}
cp ${OLD_DATADIR}/../lalshell .
BLDWRAP_POSTGRES_CONF_ADDONS=fsync=off ${OLD_DATADIR}/../demo_cluster.sh ${DEMOCLUSTER_OPTS}

NEW_DATADIR="${temp_root}/datadirs"

export MASTER_DATA_DIRECTORY="${NEW_DATADIR}/qddir/demoDataDir-1"
export PGPORT=17432
gpstop -a
MASTER_DATA_DIRECTORY=""; unset MASTER_DATA_DIRECTORY
PGPORT=""; unset PGPORT
PGOPTIONS=""; unset PGOPTIONS

# Start by upgrading the master
upgrade_qd "${temp_root}/upgrade/qd" "${OLD_DATADIR}/qddir/demoDataDir-1/" "${NEW_DATADIR}/qddir/demoDataDir-1/"

# If this is a minimal smoketest to ensure that we are pulling the Oids across
# from the old cluster to the new, then exit here as we have now successfully
# upgraded a node (the QD).
if (( $smoketest )) ; then
	restore_cluster
	exit
fi

# Upgrade all the segments and mirrors. In a production setup the segments
# would be upgraded first and then the mirrors once the segments are verified.
# In this scenario we can cut corners since we don't have any important data
# in the test cluster and we only consern ourselves with 100% success rate.
for i in 1 2 3
do
	j=$(($i-1))
	upgrade_segment "${temp_root}/upgrade/dbfast$i" "${OLD_DATADIR}/dbfast$i/demoDataDir$j/" "${NEW_DATADIR}/dbfast$i/demoDataDir$j/"
	if (( $mirrors )) ; then
		upgrade_segment "${temp_root}/upgrade/dbfast_mirror$i" "${OLD_DATADIR}/dbfast_mirror$i/demoDataDir$j/" "${NEW_DATADIR}/dbfast_mirror$i/demoDataDir$j/"
	fi
done

. ${NEW_BINDIR}/../greenplum_path.sh

# Start the new cluster, dump it and stop it again when done. We need to bump
# the exports to the new cluster for starting it but reset back to the old
# when done. Set the same variables as gpdemo-env.sh exports. Since creation
# of that file can collide between the gpdemo clusters, perform it manually
export PGPORT=17432
export MASTER_DATA_DIRECTORY="${NEW_DATADIR}/qddir/demoDataDir-1"
gpstart -a

# Run any post-upgrade tasks to prep the cluster for diffing
if [ -f "test_gpdb_post.sql" ]; then
	psql -f test_gpdb_post.sql regression
fi

${NEW_BINDIR}/pg_dumpall --schema-only -f "$temp_root/dump2.sql"
gpstop -a
export PGPORT=15432
export MASTER_DATA_DIRECTORY="${OLD_DATADIR}/qddir/demoDataDir-1"

# Since we've used the same pg_dumpall binary to create both dumps, whitespace
# shouldn't be a cause of difference in the files but it is. Partitioning info
# is generated via backend functionality in the cluster being dumped, and not
# in pg_dump, so whitespace changes can trip up the diff.
if diff -w "$temp_root/dump1.sql" "$temp_root/dump2.sql" >/dev/null; then
	echo "Passed"
	exit 0
else
	# To aid debugging in pipelines, print the diff to stdout
	diff "$temp_root/dump1.sql" "$temp_root/dump2.sql"
	echo "Error: before and after dumps differ"
	exit 1
fi
