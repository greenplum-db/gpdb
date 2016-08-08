#!/bin/bash

mygphome=${GPHOME:-../../product/greenplum-db}
source ${mygphome}/greenplum_path.sh

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

GPMGMT_BIN=$(SRC_DIR)/gpMgmt/bin
EXT=${GPMGMT_BIN}/pythonSrc/ext
EPYDOC_DIR=epydoc-3.0.1
EPYDOC_PYTHONPATH=${EXT}:${EXT}/${EPYDOC_DIR}/build/lib/

pushd $DIR/..

source tinc_env.sh

make -C ${GPMGMT_BIN} epydoc

echo "Generating documentation for TINC ..."
PYTHONPATH=${PYTHONPATH}:${EPYDOC_PYTHONPATH} python ${EXT}/${EPYDOC_DIR}/build/scripts-*/epydoc -v --graph umlclasstree --no-private --config=.epydoc.config

# now if tincrepo is there, make epydocs for that also
if [ "x${TINCREPOHOME}" != "x" ]; then
    pushd ${TINCREPOHOME}
    echo "Generating documentation for TINCREPO ..."
    PYTHONPATH=${PYTHONPATH}:${EPYDOC_PYTHONPATH} python ${EXT}/${EPYDOC_DIR}/build/scripts-*/epydoc -v --graph umlclasstree --no-private --config=.epydoc.config
    popd
fi

echo "Documentation for TINC can be found at $TINCHOME/epydocs"
echo "Documentation for TINCREPO can be found at $TINCREPOHOME/epydocs"
popd
