#!/bin/bash -l

set -exo pipefail

CWDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source "${CWDIR}/common.bash"

HADOOP_TARGET_VERSION=${HADOOP_TARGET_VERSION:-mpr}
MAPR_HOST=${MAPR_HOST:-"35.232.9.248"}

function gen_env(){
	cat > /home/gpadmin/run_regression_test.sh <<-EOF
	set -exo pipefail

	trap look4diffs ERR
	dir=\${1}

	function look4diffs() {

	    diff_files=\`find \${dir}/gpdb_src/gpAux/extensions/gphdfs/regression -name regression.diffs\`

	    for diff_file in \${diff_files}; do
		if [ -f "\${diff_file}" ]; then
		    cat <<-FEOF

					======================================================================
					DIFF FILE: \${diff_file}
					----------------------------------------------------------------------

					\$(cat "\${diff_file}")

				FEOF
		fi
	    done
	    exit 1
	}

	source /opt/gcc_env.sh
	source /usr/local/greenplum-db-devel/greenplum_path.sh

	cd "\${1}/gpdb_src/gpAux"
	source gpdemo/gpdemo-env.sh

    if [ "$HADOOP_TARGET_VERSION" != "mpr" ]; then
		wget -P /tmp http://archive.apache.org/dist/hadoop/common/hadoop-2.7.3/hadoop-2.7.3.tar.gz
		tar zxf /tmp/hadoop-2.7.3.tar.gz -C /tmp
		export HADOOP_HOME=/tmp/hadoop-2.7.3

		wget -O \${HADOOP_HOME}/share/hadoop/common/lib/parquet-hadoop-bundle-1.7.0.jar http://central.maven.org/maven2/org/apache/parquet/parquet-hadoop-bundle/1.7.0/parquet-hadoop-bundle-1.7.0.jar
		cat > "\${HADOOP_HOME}/etc/hadoop/core-site.xml" <<-EOFF
			<configuration>
				<property>
					<name>fs.defaultFS</name>
					<value>hdfs://localhost:9000/</value>
				</property>
			</configuration>
		EOFF

		\${HADOOP_HOME}/bin/hdfs namenode -format -force
		\${HADOOP_HOME}/sbin/start-dfs.sh
	else		
		export HADOOP_HOME=/opt/mapr/hadoop/hadoop-2.7.0
	fi

	cd "\${1}/gpdb_src/gpAux/extensions/gphdfs/regression/integrate"
	./generate_gphdfs_data.sh

	cd "\${1}/gpdb_src/gpAux/extensions/gphdfs/regression"
	GP_HADOOP_TARGET_VERSION=$HADOOP_TARGET_VERSION HADOOP_HOST=localhost HADOOP_PORT=9000 ./run_gphdfs_regression.sh

	exit 0
EOF

	chown gpadmin:gpadmin /home/gpadmin/run_regression_test.sh
	chmod a+x /home/gpadmin/run_regression_test.sh
}

function install_mapr_client() {
	if [ "$HADOOP_TARGET_VERSION" == "mpr" ]; then
		cat > "/etc/yum.repos.d/maprtech.repo" <<-EOFMAPR
			[maprtech]
			name=MapR Technologies
			baseurl=http://package.mapr.com/releases/v5.2.0/redhat
			enabled=1
			gpgcheck=0
			protect=1
		EOFMAPR
		yum install -y mapr-client.x86_64
		/opt/mapr/server/configure.sh -N mapr -c -C $MAPR_HOST:7222
	fi
}

function run_regression_test() {
	su - gpadmin -c "bash /home/gpadmin/run_regression_test.sh $(pwd)"
}

function setup_gpadmin_user() {
	./gpdb_src/concourse/scripts/setup_gpadmin_user.bash "$TARGET_OS"
}

function _main() {
	if [ -z "$TARGET_OS" ]; then
		echo "FATAL: TARGET_OS is not set"
		exit 1
	fi

	if [ "$TARGET_OS" != "centos" -a "$TARGET_OS" != "sles" ]; then
		echo "FATAL: TARGET_OS is set to an invalid value: $TARGET_OS"
		echo "Configure TARGET_OS to be centos or sles"
		exit 1
	fi

	time install_mapr_client
	time configure
	sed -i s/1024/unlimited/ /etc/security/limits.d/90-nproc.conf
	time install_gpdb
	time setup_gpadmin_user
	time make_cluster
	time gen_env
	time run_regression_test
}

_main "$@"
