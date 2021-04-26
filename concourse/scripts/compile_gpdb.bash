#!/bin/bash
set -exo pipefail

CWDIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${CWDIR}/common.bash"

function prep_env() {
	OS=$(os_id)
	OS_VERSION=$(os_version)
	OUTPUT_ARTIFACT_DIR=${OUTPUT_ARTIFACT_DIR:=gpdb_artifacts}

	export CONFIGURE_FLAGS=${CONFIGURE_FLAGS}
	export GPDB_ARTIFACTS_DIR=$(pwd)/${OUTPUT_ARTIFACT_DIR}
	export BLD_ARCH=$(build_arch)

	GPDB_SRC_PATH=${GPDB_SRC_PATH:=gpdb_src}
	GPDB_EXT_PATH=${GPDB_SRC_PATH}/gpAux/ext/${BLD_ARCH}
	GPDB_BIN_FILENAME=${GPDB_BIN_FILENAME:="bin_gpdb.tar.gz"}
	GPDB_CL_FILENAME=${GPDB_CL_FILENAME:="bin_gpdb_clients.tar.gz"}

	GREENPLUM_INSTALL_DIR=/usr/local/greenplum-db-devel
	GREENPLUM_CL_INSTALL_DIR=/usr/local/greenplum-clients-devel

	mkdir -p "${GPDB_ARTIFACTS_DIR}"

	# By default, only GPDB Server binary is build.
	# Use BLD_TARGETS flag with appropriate value string to generate client, loaders
	# connectors binaries
	if [ -n "${BLD_TARGETS}" ]; then
		BLD_TARGET_OPTION=("BLD_TARGETS=\"$BLD_TARGETS\"")
	else
		BLD_TARGET_OPTION=("")
	fi
}

function link_python() {
	if [[ "${OS}" == "centos" ]] && [[ "${OS_VERSION}" == "8" ]]; then
		ln -sf /opt/python-2.7.17 "$(pwd)/${GPDB_EXT_PATH}/python-2.7.17"
	else
		ln -sf /opt/python-2.7.12 "$(pwd)/${GPDB_EXT_PATH}/python-2.7.12"
	fi
}

function generate_build_number() {
	pushd ${GPDB_SRC_PATH}
	# Only if its git repo, add commit SHA as build number
	# BUILD_NUMBER file is used by getversion file in GPDB to append to version
	if [ -d .git ]; then
		echo "commit:$(git rev-parse HEAD)" >BUILD_NUMBER
	fi
	popd
}

function build_gpdb() {
	pushd ${GPDB_SRC_PATH}/gpAux
	if [ -n "$1" ]; then
		make "$1" GPROOT=/usr/local PARALLEL_MAKE_OPTS=-j"$(nproc)" -s dist
	else
		make GPROOT=/usr/local PARALLEL_MAKE_OPTS=-j"$(nproc)" -s dist
	fi
	popd
}

function git_info() {
	pushd ${GPDB_SRC_PATH}
	if [[ -d .git ]]; then
		"${CWDIR}/git_info.bash" | tee ${GREENPLUM_INSTALL_DIR}/etc/git-info.json

		PREV_TAG=$(git describe --tags --abbrev=0 HEAD^)

		cat >${GREENPLUM_INSTALL_DIR}/etc/git-current-changelog.txt <<-EOF
			======================================================================
			Git log since previous release tag (${PREV_TAG})
			----------------------------------------------------------------------
		EOF

		git log --abbrev-commit --date=relative "${PREV_TAG}..HEAD" >>${GREENPLUM_INSTALL_DIR}/etc/git-current-changelog.txt
	fi
	popd
}

function unittest_check_gpdb() {
	pushd ${GPDB_SRC_PATH}
	source ${GREENPLUM_INSTALL_DIR}/greenplum_path.sh
	make GPROOT=/usr/local -s unittest-check
	popd
}

function include_zstd() {
	local libdir
	case "${OS}" in
	centos | sles | photon) libdir=/usr/lib64 ;;
	ubuntu) libdir=/usr/lib ;;
	*) return ;;
	esac
	pushd ${GREENPLUM_INSTALL_DIR}
	cp ${libdir}/pkgconfig/libzstd.pc lib/pkgconfig
	cp -d ${libdir}/libzstd.so* lib
	cp /usr/include/zstd*.h include
	popd
}

function include_quicklz() {
	local libdir
	case "${OS}" in
	centos | sles | photon) libdir=/usr/lib64 ;;
	ubuntu) libdir=/usr/local/lib ;;
	*) return ;;
	esac
	pushd ${GREENPLUM_INSTALL_DIR}
	cp -d ${libdir}/libquicklz.so* lib
	popd
}

function include_libstdcxx() {
	if [[ "${OS}" == "centos" ]] && [[ "${OS_VERSION}" != "8" ]]; then
		pushd /opt/gcc-6*/lib64
		for libfile in libstdc++.so.*; do
			case $libfile in
			*.py) ;;
				# we don't vendor libstdc++.so.*-gdb.py
			*)
				cp -d "$libfile" ${GREENPLUM_INSTALL_DIR}/lib
				;; # vendor everything else
			esac
		done
		popd
	fi

}

function include_libuv() {
	local includedir=/usr/include
	local libdir
	case "${OS}" in
	centos | sles | photon) libdir=/usr/lib64 ;;
	ubuntu) libdir=/usr/lib/x86_64-linux-gnu ;;
	*) return ;;
	esac
	pushd ${GREENPLUM_INSTALL_DIR}
	# need to include both uv.h and uv/*.h
	cp -a ${includedir}/uv* include
	cp -a ${libdir}/libuv.so* lib
	popd
}

function export_gpdb() {
	TARBALL="${GPDB_ARTIFACTS_DIR}/${GPDB_BIN_FILENAME}"
	local server_version
	server_version="$("${GPDB_SRC_PATH}"/getversion --short)"

	local server_build
	server_build="${GPDB_ARTIFACTS_DIR}/server-build-${server_version}-${BLD_ARCH}${RC_BUILD_TYPE_GCS}.tar.gz"

	pushd ${GREENPLUM_INSTALL_DIR}
	source greenplum_path.sh
	python -m compileall -q -x test .
	chmod -R 755 .
	tar -czf "${TARBALL}" ./*
	popd

	cp "${TARBALL}" "${server_build}"
}

function export_gpdb_extensions() {
	pushd ${GPDB_SRC_PATH}/gpAux
	if ls greenplum-*zip* 1>/dev/null 2>&1; then
		chmod 755 greenplum-*zip*
		cp greenplum-*zip* "${GPDB_ARTIFACTS_DIR}/"
	fi
	popd
}

function export_gpdb_clients() {
	TARBALL="${GPDB_ARTIFACTS_DIR}/${GPDB_CL_FILENAME}"
	pushd ${GREENPLUM_CL_INSTALL_DIR}
	source ./greenplum_clients_path.sh
	mkdir -p bin/ext/gppylib
	cp ${GREENPLUM_INSTALL_DIR}/lib/python/gppylib/__init__.py ./bin/ext/gppylib
	cp ${GREENPLUM_INSTALL_DIR}/lib/python/gppylib/gpversion.py ./bin/ext/gppylib
	# GPHOME_LOADERS and greenplum_loaders_path.sh are still requried by some users
	# So link greenplum_loaders_path.sh to greenplum_clients_path.sh for compatible
	ln -sf greenplum_clients_path.sh greenplum_loaders_path.sh
	chmod -R 755 .
	tar -czf "${TARBALL}" ./*
	popd
}

function build_xerces() {
	OUTPUT_DIR="${GPDB_EXT_PATH}"
	mkdir -p xerces_patch/concourse
	cp -r gpdb_src/src/backend/gporca/concourse/xerces-c xerces_patch/concourse
	/usr/bin/python xerces_patch/concourse/xerces-c/build_xerces.py --output_dir="${OUTPUT_DIR}"
	rm -rf build
}

function _main() {

	prep_env

	build_xerces
	link_python
	generate_build_number
	build_gpdb "${BLD_TARGET_OPTION[@]}"
	git_info

	if [[ -z "${SKIP_UNITTESTS}" ]]; then
		unittest_check_gpdb
	fi

	include_zstd
	include_quicklz
	include_libuv
	include_libstdcxx

	export_gpdb
	export_gpdb_extensions

	if echo "${BLD_TARGETS}" | grep -qwi "clients"; then
		export_gpdb_clients
	fi

}

_main "$@"
