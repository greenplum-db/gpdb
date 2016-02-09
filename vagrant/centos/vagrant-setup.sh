#!/bin/bash

set -x

# install packages needed to build and run GPDB
sudo yum -y update
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
wget https://bootstrap.pypa.io/get-pip.py
sudo python get-pip.py
sudo pip install psi lockfile paramiko setuptools epydoc
rm get-pip.py

# Create gpadmin user
sudo useradd gpadmin -d /home/gpadmin -s /bin/bash
echo changeme | sudo passwd gpadmin --stdin

# Misc
sudo yum -y install vim mc psmisc
