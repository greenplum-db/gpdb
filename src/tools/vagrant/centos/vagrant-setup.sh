#!/bin/bash

set -x

# install packages needed to build and run GPDB
sudo yum -y groupinstall "Development tools"
sudo yum -y install ed
sudo yum -y install readline-devel
sudo yum -y install zlib-devel
sudo yum -y install curl-devel
sudo yum -y install bzip2-devel
sudo yum -y install python-devel
sudo yum -y install apr-devel
sudo yum -y install libevent-devel
sudo yum -y install openssl-libs openssl-devel
sudo yum -y install libyaml libyaml-devel
sudo yum -y install epel-release
sudo yum -y install htop
sudo yum -y install perl-Env
sudo yum -y install ccache
sudo yum -y install libffi-devel
sudo yum -y install libxml2-devel
wget https://bootstrap.pypa.io/get-pip.py
sudo python get-pip.py
sudo pip install psutil lockfile paramiko setuptools
rm get-pip.py

# Misc
sudo yum -y install vim mc psmisc

# cmake 3.0
pushd ~
  wget http://www.cmake.org/files/v3.0/cmake-3.0.0.tar.gz
  tar -zxvf cmake-3.0.0.tar.gz
  pushd cmake-3.0.0
    ./bootstrap
    make
    make install
    export PATH=/usr/local/bin:$PATH
  popd
popd

sudo chown -R vagrant:vagrant /usr/local
