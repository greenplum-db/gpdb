#!/bin/bash -l

set -exo pipefail

CWDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source "${CWDIR}/common.bash"
GPHOME="/usr/local/greenplum-db-devel"
PXF_HOME="${GPHOME}/pxf"

function run_regression_test() {
	cat > /home/gpadmin/run_regression_test.sh <<-EOF
	set -exo pipefail

	source /opt/gcc_env.sh
	source ${GPHOME}/greenplum_path.sh

	cd "\${1}/gpdb_src/gpAux"
	source gpdemo/gpdemo-env.sh

	cd "\${1}/gpdb_src/gpAux/extensions/pxf"
	make installcheck USE_PGXS=1

	[ -s regression.diffs ] && cat regression.diffs && exit 1

	exit 0
	EOF

	chown gpadmin:gpadmin /home/gpadmin/run_regression_test.sh
	chmod a+x /home/gpadmin/run_regression_test.sh

	su gpadmin -c "bash /home/gpadmin/run_regression_test.sh $(pwd)"
}

function run_pxf_automation() {
	cat > /home/gpadmin/run_pxf_automation_test.sh <<-EOF
	set -exo pipefail

	source ${GPHOME}/greenplum_path.sh
	source \${1}/gpdb_src/gpAux/gpdemo/gpdemo-env.sh

	# set variables needed by PXF Automation and Parot to run in GPDB mode with SingleCluster and standalone PXF
	export PG_MODE=GPDB
	export GPHD_ROOT=$1
	export PXF_HOME=${PXF_HOME}

	# Copy PSI package from system python to GPDB as automation test requires it
	psi_dir=\$(find /usr/lib64 -name psi | sort -r | head -1)
	cp -r \${psi_dir} ${GPHOME}/lib/python
	psql -d template1 -c "CREATE EXTENSION PXF"
	cd \${1}/pxf_automation_src
	make GROUP=gpdb

	exit 0
	EOF

	chown gpadmin:gpadmin /home/gpadmin/run_pxf_automation_test.sh
	chmod a+x /home/gpadmin/run_pxf_automation_test.sh
	su gpadmin -c "bash /home/gpadmin/run_pxf_automation_test.sh $(pwd)"
}

function setup_gpadmin_user() {
	./gpdb_src/concourse/scripts/setup_gpadmin_user.bash "$TARGET_OS"
}

function unpack_tarball() {
	local tarball=$1
	echo "Unpacking tarball: $(ls ${tarball})"
	tar xfp ${tarball} --strip-components=1
}

function setup_singlecluster() {
	pushd singlecluster && if [ -f ./*.tar.gz ]; then \
		unpack_tarball ./*.tar.gz; \
	fi && popd

	pushd singlecluster/bin
	export SLAVES=1
	./init-gphd.sh
	./start-hdfs.sh
	./start-yarn.sh
	./start-hive.sh
	./start-zookeeper.sh
	./start-hbase.sh
	popd
}

function install_hadoop_client_rpms() {
	local hdfsrepo=$1
	local yum_repofile_url=$2

	pushd /etc/yum.repos.d > /dev/null
	# download yum repo definition file for the vendor stack
	wget ${yum_repofile_url}
	# install required RPMs
	yum install -y hadoop-client
	yum install -y hive
	yum install -y hbase
	# copy cluster configuration files from single cluster
	mkdir -p /etc/hadoop/conf
	cp ${hdfsrepo}/hadoop/etc/hadoop/core-site.xml /etc/hadoop/conf/
	cp ${hdfsrepo}/hadoop/etc/hadoop/hdfs-site.xml /etc/hadoop/conf/
	mkdir -p /etc/hive/conf
	cp ${hdfsrepo}/hive/conf/hive-site.xml /etc/hive/conf
	mkdir -p /etc/hbase/conf
	cp ${hdfsrepo}/hbase/conf/hbase-site.xml /etc/hbase/conf
	popd
}

function setup_hadoop_client() {
	local hdfsrepo=$1

	case ${HADOOP_CLIENT} in
		CDH)
			install_hadoop_client_rpms ${hdfsrepo} "https://archive.cloudera.com/cdh5/redhat/6/x86_64/cdh/cloudera-cdh5.repo"
			;;
		HDP)
			install_hadoop_client_rpms ${hdfsrepo} "http://public-repo-1.hortonworks.com/HDP/centos6/2.x/updates/2.6.2.0/hdp.repo"
			;;
		TAR)
			# TAR-based setup, edit the properties in pxf-env.sh to correct value
			sed -i -e "s|^[[:blank:]]*export HADOOP_DISTRO=.*$|export HADOOP_DISTRO=TAR|g" ${PXF_HOME}/conf/pxf-env.sh
			sed -i -e "s|^[[:blank:]]*export HADOOP_HOME=.*$|export HADOOP_HOME=${hdfsrepo}/hadoop|g" ${PXF_HOME}/conf/pxf-env.sh
			sed -i -e "s|^[[:blank:]]*export HIVE_HOME=.*$|export HIVE_HOME=${hdfsrepo}/hive|g" ${PXF_HOME}/conf/pxf-env.sh
			sed -i -e "s|^[[:blank:]]*export HBASE_HOME=.*$|export HBASE_HOME=${hdfsrepo}/hbase|g" ${PXF_HOME}/conf/pxf-env.sh
			;;
		*)
			echo "FATAL: Unknown HADOOP_CLIENT=${HADOOP_CLIENT} parameter value"
			exit 1
			;;
	esac

	echo "Contents of ${PXF_HOME}/conf/pxf-env.sh :"
	cat ${PXF_HOME}/conf/pxf-env.sh
}

function start_pxf() {
	local hdfsrepo=$1
	pushd ${PXF_HOME} > /dev/null
	su gpadmin -c "bash ./bin/pxf init"
	su gpadmin -c "bash ./bin/pxf start"
	popd > /dev/null
}

function _main() {
	if [ -z "$TARGET_OS" ]; then
		echo "FATAL: TARGET_OS is not set"
		exit 1
	fi

	if [ "$TARGET_OS" != "centos" -a "$TARGET_OS" != "sles" ]; then
		echo "FATAL: TARGET_OS is set to an unsupported value: $TARGET_OS"
		echo "Configure TARGET_OS to be centos or sles"
		exit 1
	fi

	time configure
	time install_gpdb
	time setup_gpadmin_user

	# setup hadoop before making GPDB cluster to use system python for yum install
	time setup_singlecluster
	time setup_hadoop_client $(pwd)/singlecluster

	time make_cluster
	time start_pxf $(pwd)/singlecluster
	chown -R gpadmin:gpadmin $(pwd)
	time run_regression_test
	time run_pxf_automation $(pwd)/singlecluster
}

_main "$@"
