#!/bin/bash -l
set -exo pipefail

CWDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
GPDB_SRC_PATH=${GPDB_SRC_PATH:=gpdb_src}

function make_gphdfs_dist() {
  pushd gpdb_src/gpAux/extensions/gphdfs
    ant download-ivy
    make USE_PGXS=1 dist
    mv gnet-1.2-javadoc.tar dist
  popd
}

function _main() {
  # Untar the gpdb5 artifacts tarball to output dir
  mkdir -p gpAux_ext/ext
  tar xzf gpdb_artifacts/*.tar.gz -C gpAux_ext/ext

  make_gphdfs_dist

  # Move hdfs output directory to output dir
  mv ${GPDB_SRC_PATH}/gpAux/extensions/gphdfs/dist gphdfs_dist/
  mv /root/.ant gphdfs_dist/

}

_main "$@"
