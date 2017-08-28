#!/bin/bash -l

set -exo pipefail

CWDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source "${CWDIR}/common.bash"

function gen_env(){
	cat > /home/gpadmin/run_regression_test.sh <<-EOF
	set -exo pipefail

	source /opt/gcc_env.sh
	source /usr/local/greenplum-db-devel/greenplum_path.sh

	cd "\${1}/gpdb_src/gpAux"
	source gpdemo/gpdemo-env.sh

	if [ "$overwrite_pxf" = "true" ]
	then
		cd "\${1}/gpdb_src/gpAux/extensions/pxf"
		make install
	fi

	cd "\${1}/gpdb_src/gpAux/extensions/pxf"
	make installcheck USE_PGXS=1

	[ -s regression.diffs ] && cat regression.diffs && exit 1

	export HADOOP_HOME=\${1}/singlecluster/hadoop
	cd "\${1}/gpdb_src/gpAux/extensions/pxf/regression/integrate"
	HADOOP_HOST=localhost HADOOP_PORT=8020 ./generate_hdfs_data.sh

	cd "\${1}/gpdb_src/gpAux/extensions/pxf/regression"
	GP_HADOOP_TARGET_VERSION=cdh4.1 HADOOP_HOST=localhost HADOOP_PORT=8020 ./run_pxf_regression.sh

	exit 0
	EOF

	chown -R gpadmin:gpadmin $(pwd)
	chown gpadmin:gpadmin /home/gpadmin/run_regression_test.sh
	chmod a+x /home/gpadmin/run_regression_test.sh
}

function run_regression_test() {
	su gpadmin -c "bash /home/gpadmin/run_regression_test.sh $(pwd)"
}

function setup_gpadmin_user() {
	./gpdb_src/concourse/scripts/setup_gpadmin_user.bash "$TARGET_OS"
}

unpack_tarball() {
	local tarball=$1
	echo "Unpacking tarball: $(ls ${tarball})"
	tar xfp ${tarball} --strip-components=1
}

function setup_singlecluster() {
	pushd singlecluster && if [ -f ./*.tar.gz ]; then \
		unpack_tarball ./*.tar.gz; \
	fi && popd
	install_pxf ${1}/singlecluster

	pushd singlecluster/bin
	# set Standalone PXF mode without Hadoop
	export PXFDEMO=true
	export SLAVES=1
	./init-gphd.sh
	./init-pxf.sh
	./start-hdfs.sh
	./start-pxf.sh
	popd
}

install_pxf() {
	local hdfsrepo=$1
	if [ -d pxf_tarball ]; then
		echo "======================================================================"
		echo "                            Install PXF"
		echo "======================================================================"
		pushd pxf_tarball > /dev/null
		unpack_tarball ./*.tar.gz
		for X in distributions/pxf-*.tar.gz; do
			tar -xvzf ${X}
		done
		mkdir -p ${hdfsrepo}/pxf/conf
		mv pxf-*/pxf-*.jar ${hdfsrepo}/pxf
		mv pxf-*/pxf.war ${hdfsrepo}/pxf
		mv pxf-*/conf/{pxf-public.classpath,pxf-profiles.xml,pxf-private.classpath} ${hdfsrepo}/pxf/conf
		popd > /dev/null
		pushd ${hdfsrepo}/pxf && for X in pxf-*-[0-9]*.jar; do \
			ln -s ${X} $(echo ${X} | sed -e 's/-[a-zA-Z0-9.]*.jar/.jar/'); \
		done
		popd > /dev/null
	fi
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
	time make_cluster
	time gen_env

	time setup_singlecluster $(pwd)
	time run_regression_test
}

_main "$@"
