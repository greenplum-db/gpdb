#!/bin/bash -l
set -exo pipefail

GREENPLUM_INSTALL_DIR=/usr/local/gpdb
CWDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

function prep_env_for_centos() {
  export JAVA_HOME=/usr/lib/jvm/java-1.6.0-openjdk-1.6.0.39.x86_64
  export PATH=${JAVA_HOME}/bin:${PATH}
  source /opt/gcc_env.sh
  install_system_deps
}

function install_system_deps() {
  deps_list=" sudo 
             passwd 
             openssh-server 
             ed 
             readline-devel 
             zlib-devel 
             curl-devel 
             bzip2-devel 
             python-devel 
             apr-devel 
             libevent-devel 
             openssl-libs 
             openssl-devel 
             libyaml 
             libyaml-devel 
             epel-release 
             htop 
             perl-Env 
             perl-ExtUtils-Embed 
             libxml2-devel 
             libxslt-devel 
             libffi-devel "
  for dep in "$deps_list";
  do
    yum install -y $dep
  done
}

# This is a canonical way to build GPDB. The intent is to validate that GPDB compiles 
# with a fairly basic build. It is not meant to be exhasustive or include all features
# and components available in GPDB.

function build_gpdb() {
  pushd gpdb_src
    ./configure --enable-mapreduce --with-perl --with-libxml --with-python --disable-gpfdist \
        --prefix=${GREENPLUM_INSTALL_DIR} --enable-codegen --with-codegen-prefix=/opt/llvm-3.7.1
    make
    make install
  popd
}

function unittest_check_gpdb() {
  pushd gpdb_src/src/backend
    make -s unittest-check
  popd
}

function _main() {
  prep_env_for_centos
  build_gpdb
  unittest_check_gpdb
}

_main "$@"
