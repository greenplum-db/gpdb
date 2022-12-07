set -e

# setup toolchain
source /opt/gcc_env.sh
source /usr/local/greenplum-db-devel/greenplum_path.sh
export MASTER_DATA_DIRECTORY=/data/gpdata/coordinator/gpseg-1

# prepare source tree for make installcheck
pushd gpdb_src
./configure --prefix=/usr/local/greenplum-db-devel --without-readline --without-zlib --disable-gpfdist --without-libcurl --disable-orca

make -C src/port
make -C contrib/pgcrypto installcheck

popd
