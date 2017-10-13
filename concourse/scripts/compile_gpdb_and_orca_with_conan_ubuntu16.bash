#!/bin/bash

set -u -e -x

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

GPDB_INSTALL_DIR=/tmp/gpdb_install_dir
mkdir -p ${GPDB_INSTALL_DIR}

update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-6 50 --slave /usr/bin/g++ g++ /usr/bin/g++-6
update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-5 100 --slave /usr/bin/g++ g++ /usr/bin/g++-5

# outputs compiled bits to CWD/bin_orca
${DIR}/build_orca.py --bintrayRemote=${BINTRAY_REMOTE} --bintrayRemoteURL=${BINTRAY_REMOTE_URL}

update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-6 100 --slave /usr/bin/g++ g++ /usr/bin/g++-6
update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-5 50 --slave /usr/bin/g++ g++ /usr/bin/g++-5

${DIR}/build_gpdb.py --mode=orca --output_dir=${GPDB_INSTALL_DIR} bin_orca

tar czf ${GPDB_DST_TARBALL} -C ${GPDB_INSTALL_DIR} .
tar czf ${ORCA_DST_TARBALL} bin_orca/usr/local/include bin_orca/usr/local/lib -C bin_orca/usr/local .
