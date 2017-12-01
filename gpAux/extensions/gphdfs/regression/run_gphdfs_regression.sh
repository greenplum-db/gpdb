#!/bin/bash

set -euo pipefail

# set variables if they are unset or null
export HADOOP_HOME=${HADOOP_HOME:-/usr/hdp/current/hadoop-client}
export GP_HADOOP_TARGET_VERSION=${GP_HADOOP_TARGET_VERSION:-hadoop2}
export HADOOP_HOST=${HADOOP_HOST:-localhost}
export HADOOP_PORT=${HADOOP_PORT:-8020}

export JAVA_HOME=/usr/lib/jvm/java-1.7.0-openjdk.x86_64/jre

override_core_site() {
	cat > "${HADOOP_HOME}/etc/hadoop/core-site.xml" <<-EOF
	<configuration>
	<property>
	<name>fs.defaultFS</name>
	<value>hdfs://${HADOOP_HOST}:${HADOOP_PORT}/</value>
	</property>
	</configuration>
	EOF
}

allow_hadoop_user_to_connect() {
	echo "local    all     _hadoop_perm_test_role    trust" >> ${MASTER_DATA_DIRECTORY}/pg_hba.conf
	gpstop -u
}

build_test_jar() {
	source ${CURDIR}/set_hadoop_classpath.sh

	pushd $CURDIR/legacy
	javac -cp .:$CLASSPATH:$GPHOME/lib/hadoop/${GP_HADOOP_TARGET_VERSION}-gnet-1.2.0.0.jar javaclasses/*.java
	jar cf maptest.jar javaclasses/*.class
	popd
}

create_runcmd() {
	local CMDPATH="${CURDIR}/runcmd"

	cat > "${CMDPATH}" <<-EOF
	source ${GPHOME}/greenplum_path.sh
	export HADOOP_HOME=${HADOOP_HOME}
	export JAVA_HOME=${JAVA_HOME}
	export GP_HADOOP_TARGET_VERSION=${GP_HADOOP_TARGET_VERSION}
	export HADOOP_PORT=${HADOOP_PORT}
	export PATH=$PATH:$HADOOP_HOME/sbin:$HADOOP_HOME/bin
	source ${CURDIR}/set_hadoop_classpath.sh
	java -cp .:\${CLASSPATH}:${GPHOME}/lib/hadoop/${GP_HADOOP_TARGET_VERSION}-gnet-1.2.0.0.jar:${CURDIR}/legacy/maptest.jar -Dhdfshost=${HADOOP_HOST} -Ddatanodeport=${HADOOP_PORT} -Djobtrackerhost=${HADOOP_HOST} -Djobtrackerport=${HADOOP_PORT} "\$@"
	EOF

	chmod +x "${CMDPATH}"
}

_main() {
	allow_hadoop_user_to_connect
	override_core_site

	local CURDIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
	local PGREGRESS=$GPHOME/lib/postgresql/pgxs/src/test/regress/pg_regress
	local HADOOPCMD=$HADOOP_HOME/bin/hadoop

	build_test_jar

	rm -rf $CURDIR/source_replaced/
	mkdir -p $CURDIR/source_replaced/input
	mkdir -p $CURDIR/source_replaced/output

	cp $CURDIR/input/*.source $CURDIR/source_replaced/input/
	cp $CURDIR/output/*.source $CURDIR/source_replaced/output/

	for f in $(ls $CURDIR/source_replaced/input);do
		echo -e  "--start_ignore\n\!%HADOOP_HOME%/bin/hadoop fs -rm -r /mapreduce/*\n\!%HADOOP_HOME%/bin/hadoop fs -rm -r /mapred/*\n--end_ignore" >> "$CURDIR/source_replaced/input/$f"
		sed -i "s|gpfdist://%localhost%:%gpfdistPort%|gphdfs://${HADOOP_HOST}:${HADOOP_PORT}/plaintext|g" "$CURDIR/source_replaced/input/$f"
		sed -i "s|%cmdstr%|${CURDIR}/runcmd|g" "$CURDIR/source_replaced/input/$f"
		sed -i "s|%HADOOP_HOST%|${HADOOP_HOST}:${HADOOP_PORT}|g" "$CURDIR/source_replaced/input/$f"
		sed -i "s|%HDFSaddr%|${HADOOP_HOST}:${HADOOP_PORT}|g" "$CURDIR/source_replaced/input/$f"
		sed -i "s|%HADOOP_HOME%|${HADOOP_HOME}|g" "$CURDIR/source_replaced/input/$f"
		sed -i "s|%MYD%|${CURDIR}/source_replaced/input|g" "$CURDIR/source_replaced/input/$f"
		sed -i "s|%HADOOP_FS%|${HADOOPCMD}|g" "$CURDIR/source_replaced/input/$f"
	done

	cp $CURDIR/input/parsefile.py  $CURDIR/source_replaced/input/

	create_runcmd

	$HADOOPCMD fs -rm -f -r /extwrite/*
	$HADOOPCMD fs -rm -f -r /mapreduce/*
	$HADOOPCMD fs -rm -f -r /mapred/*

	# gphdfs_regress_schedule
	PGOPTIONS="-c optimizer=off -c codegen=off -c gp_hadoop_home=${HADOOP_HOME} -c gp_hadoop_target_version=${GP_HADOOP_TARGET_VERSION}" ${PGREGRESS} --psqldir=$GPHOME/bin/ --init-file=$CURDIR/gphdfs_init_file --schedule=$CURDIR/gphdfs_regress_schedule --inputdir=$CURDIR/source_replaced --outputdir=.
}

_main "$@"
