#!/bin/bash -l
set -exo pipefail

GREENPLUM_INSTALL_DIR=/usr/local/gpdb
CWDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

function prep_env_for_centos() {
  export JAVA_HOME=/usr/lib/jvm/java-1.6.0-openjdk-1.6.0.39.x86_64
  export PATH=${JAVA_HOME}/bin:${PATH}
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
# with a fairly basic build. It is not meant to be exhaustive or include all features
# and components available in GPDB.

function build_gpdb() {
  pushd gpdb_src
    source /opt/gcc_env.sh
    CC=$(which gcc) CXX=$(which g++) ./configure --enable-mapreduce --with-perl --with-libxml \
	--disable-orca --with-python --disable-gpfdist --prefix=${GREENPLUM_INSTALL_DIR}
    # Use -j4 to speed up the build. (Doesn't seem worth trying to guess a better
    # value based on number of CPUs or anything like that. Going above -j4 wouldn't
    # make it much faster, and -j4 is small enough to not hurt too badly even on
    # a single-CPU system
    make -j4
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
