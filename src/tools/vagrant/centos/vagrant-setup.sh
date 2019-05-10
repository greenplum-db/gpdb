#!/bin/bash

set -x

# packages needed to build and run GPDB
dependencies=(
  ed
  readline-devel
  zlib-devel
  curl-devel
  bzip2-devel
  python-devel
  python-pip
  apr-devel
  libevent-devel
  openssl-libs openssl-devel
  libyaml libyaml-devel
  htop
  ccache
  libffi-devel
  libzstd-devel
  libxml2-devel
  xerces-c-devel
  perl-Env perl-ExtUtils-Embed
  cmake3
  net-tools # gives us netstat
  vim mc psmisc # miscellaneous
  git2u # requires ius-release
)

yum -y groupinstall 'Development tools'
# epel-release needed first in order to get cmake3, htop, and ccache
yum -y install epel-release
yum -y remove git
rpm -U https://centos7.iuscommunity.org/ius-release.rpm
yum -y install "${dependencies[@]}"

# so we can call cmake
ln -s /usr/bin/cmake{3,}

pip install psutil lockfile setuptools

chown -R vagrant:vagrant /usr/local
