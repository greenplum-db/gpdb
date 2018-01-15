#!/bin/bash

set -ex

# clone the repos
rm -fr gpdb

pushd ~
  git clone https://github.com/greenplum-db/gpdb
popd

export CC="ccache cc"
export CXX="ccache c++"
export PATH=/usr/local/bin:$PATH

# this is necessary so that configure can find the orca libraries
export LD_LIBRARY_PATH=/usr/local/lib

rm -rf /usr/local/gpdb
pushd ~/gpdb
  ./configure --prefix=/usr/local/gpdb $@
  make clean
  make -j4 -s && make install
popd

# generate ssh key to avoid typing password all the time during gpdemo make
rm -f ~/.ssh/id_rsa
rm -f ~/.ssh/id_rsa.pub
ssh-keygen -t rsa -N "" -f "$HOME/.ssh/id_rsa"
cat ~/.ssh/id_rsa.pub >> ~/.ssh/authorized_keys
chmod 600 ~/.ssh/authorized_keys

# BUG: fix the LD_LIBRARY_PATH to find installed GPOPT libraries
echo export LD_LIBRARY_PATH=/usr/local/lib:\$LD_LIBRARY_PATH >> /usr/local/gpdb/greenplum_path.sh

# use gpdemo to start the cluster
pushd ~/gpdb/gpAux/gpdemo
  source /usr/local/gpdb/greenplum_path.sh
  make
popd
