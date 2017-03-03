#!/bin/bash

source /usr/local/gpdb/greenplum_path.sh

if [ ! -e /tmp/demo_sem ]; then
  touch /tmp/demo_sem

  pushd /workspace/gpdb/gpAux/gpdemo
    make cluster
  popd
fi

source /workspace/gpdb/gpAux/gpdemo/gpdemo-env.sh
