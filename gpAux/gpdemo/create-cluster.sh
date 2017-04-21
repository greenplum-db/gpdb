#!/usr/bin/env bash

set -x

pushd ../..
make clean
make distclean
./configure --prefix `pwd`/greenplum-dev --enable-debug CC="ccache gcc"
make -j8
make install
popd
./demo_cluster.sh
echo "Did you set the operating system values after restart? e.g. source ~/workspace/greenplum_os_config.sh"

