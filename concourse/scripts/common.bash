#!/bin/bash -l

## ----------------------------------------------------------------------
## General purpose functions
## ----------------------------------------------------------------------

function set_env() {
	export TERM=xterm-256color
	export TIMEFORMAT=$'\e[4;33mIt took %R seconds to complete this step\e[0m'
}

function os_id() {
	if [[ -f "/etc/redhat-release" ]]; then
		echo "centos"
	else
		echo "$(
			. /etc/os-release
			echo "${ID}"
		)"
	fi
}

function os_version() {
	if [[ -f "/etc/redhat-release" ]]; then
		echo "$(sed </etc/redhat-release 's/.*release *//' | cut -d. -f1)"
	else
		echo "$(
			. /etc/os-release
			echo "${VERSION_ID}"
		)"
	fi
}

function build_arch() {
	local id=$(os_id)
	local version=$(os_version)
	# BLD_ARCH expects rhel{6,7,8}_x86_64 || rocky8_x86_64 || photon3_x86_64 || sles12_x86_64 || ubuntu18.04_x86_64 || ol8_x86_64
	case ${id} in
	photon | sles | rocky | ol ) version=$(os_version | cut -d. -f1) ;;
	centos) id="rhel" ;;
	*) ;;
	esac

	echo "${id}${version}_x86_64"
}

## ----------------------------------------------------------------------
## Test functions
## ----------------------------------------------------------------------

function install_gpdb() {
	[ ! -d /usr/local/greenplum-db-devel ] && mkdir -p /usr/local/greenplum-db-devel
	tar -xzf bin_gpdb/bin_gpdb.tar.gz -C /usr/local/greenplum-db-devel
}

function setup_configure_vars() {
	# We need to add GPHOME paths for configure to check for packaged
	# libraries (e.g. ZStandard).
	source /usr/local/greenplum-db-devel/greenplum_path.sh
	export LDFLAGS="-L${GPHOME}/lib"
	export CPPFLAGS="-I${GPHOME}/include"
}

function configure() {
	pushd gpdb_src
	# The full set of configure options which were used for building the
	# tree must be used here as well since the toplevel Makefile depends
	# on these options for deciding what to test. Since we don't ship
	./configure --prefix=/usr/local/greenplum-db-devel --with-perl --with-python --with-libxml --with-uuid=e2fs --enable-mapreduce --enable-orafce --enable-tap-tests --disable-orca --with-openssl ${CONFIGURE_FLAGS}

	popd
}

function install_and_configure_gpdb() {
	install_gpdb
	setup_configure_vars
	configure
}

function make_cluster() {
	source /usr/local/greenplum-db-devel/greenplum_path.sh
	export BLDWRAP_POSTGRES_CONF_ADDONS=${BLDWRAP_POSTGRES_CONF_ADDONS}
	export STATEMENT_MEM=250MB
	pushd gpdb_src/gpAux/gpdemo
	su gpadmin -c "source /usr/local/greenplum-db-devel/greenplum_path.sh; make create-demo-cluster WITH_MIRRORS=${WITH_MIRRORS:-true}"

	if [[ "$MAKE_TEST_COMMAND" =~ gp_interconnect_type=proxy ]]; then
		# generate the addresses for proxy mode
		su gpadmin -c bash -- -e <<EOF
			source /usr/local/greenplum-db-devel/greenplum_path.sh
			source $PWD/gpdemo-env.sh

			delta=-3000

			psql -tqA -d postgres -P pager=off -F: -R, \
					-c "select dbid, content, address, port+\$delta as port
								from gp_segment_configuration
								order by 1" \
			| xargs -rI'{}' \
				gpconfig --skipvalidation -c gp_interconnect_proxy_addresses -v "'{}'"

			# also have to enlarge gp_interconnect_tcp_listener_backlog
			gpconfig -c gp_interconnect_tcp_listener_backlog -v 1024

			gpstop -u
EOF
	fi

	popd
}

function run_test() {
	su gpadmin -c "bash /opt/run_test.sh $(pwd)"
}
