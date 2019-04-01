#!/usr/bin/env bash


set -o errexit


display_ccache_statistics() {
    ccache -s
}


regain_ownership_of_working_directory() {
    sudo chown -R $(whoami) $(pwd)
}


fetch_xerces() {
    rm -rf /tmp/xerces
    git clone https://github.com/greenplum-db/gp-xerces /tmp/xerces
}


install_xerces() {
    (
      set -e
      cd /tmp/xerces
      ./configure CC='ccache gcc' \
  		  CXX='ccache g++' \
		  --disable-network
    )
    make_command -s -C /tmp/xerces
    sudo make install -C /tmp/xerces
}


fetch_and_build_xerces_c() {
    fetch_xerces
    install_xerces
}


get_orca_url() {
    sed -E -n -e '/gporca/s,.*https://github.com/greenplum-db/gporca/releases/download/v(([[:digit:]]|\.)+)/bin_orca_centos5_release.tar.gz.*,https://github.com/greenplum-db/gporca/archive/v\1.tar.gz,p' <gpAux/releng/releng.mk
}


fetch_orca() {
    local orca_code_url
    orca_code_url=$(get_orca_url)
    mkdir -p /tmp/orca
    wget --quiet --output-document - "${orca_code_url}" | \
	tar zx --strip-components=1 -C /tmp/orca
}


get_number_of_cores() {
    getconf _NPROCESSORS_ONLN
}


make_command() {
    local number_of_cores=$(get_number_of_cores)
    local target_load_average=$((number_of_cores * 4))

    make --jobs $number_of_cores \
	 --load-average $target_load_average \
	 "$@"
}


install_orca() {
    cmake -G Ninja -H/tmp/orca -B/tmp/orca/build
    local ncpu
    ncpu=$(get_number_of_cores)
    ninja -l "$((4 * ncpu))" -C /tmp/orca/build
    sudo ninja install -C /tmp/orca/build
}


fetch_and_build_orca() {
    fetch_orca
    install_orca
    sudo ldconfig
}


install_gpdb() {
    ./configure CC='ccache gcc' \
		CXX='ccache g++' \
		--prefix=${GPDB_INSTALL_DIR} \
		--with-perl \
		--with-python \
		--with-libxml \
		--with-zstd \
		--with-gssapi \
		--with-openssl \
		--enable-debug-extensions \
		--enable-cassert \
		--enable-orca \
		--enable-mapreduce \
		--enable-tap-tests \
		${CONFIGURE_FLAGS};

    make_command -s
    make_command install -s
}


install_demo_cluster() {
    source ./greenplum-db-devel/greenplum_path.sh
    make create-demo-cluster;
    source ./gpAux/gpdemo/gpdemo-env.sh
}

start_sshd() {
    /start-sshd.bash
}


run_make_task() {
    local regress_options='--print-failure-diffs'

    EXTRA_REGRESS_OPTS="${regress_options}" \
    REGRESS_OPTS="${regress_options}" \
	PGOPTIONS="${PGOPTIONS}" \
	make ${MAKE_TASK}
}


_main() {
    export GPDB_INSTALL_DIR="$PWD/greenplum-db-devel/"
    export CCACHE_DIR="$PWD/.ccache"
    # FIXME: entered here to make gpload tests pass
    export LOGNAME=$(whoami)

    time (
	echo "Regaining ownership of working directory."
	time regain_ownership_of_working_directory

	echo "Display ccache statistics"
	time display_ccache_statistics

	echo "Install xerces"
	time fetch_and_build_xerces_c

	echo "Install orca"
	time fetch_and_build_orca

	echo "Install gpdb"
	time install_gpdb
	
	echo "Display ccache statistics"
	time display_ccache_statistics

	echo "Start sshd"
	time start_sshd

	echo "Create a demo cluster"
	time install_demo_cluster

	echo "Run make task"
	time run_make_task
    )
}


_main "$@"
