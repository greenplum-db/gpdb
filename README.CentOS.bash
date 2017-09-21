#!/bin/bash

# TODO: This currently has non-Orca dependencies only. Please expand if you need to build Orca.

# Install needed packages. Please add to this list if you discover additional prerequisites.
sudo yum install -y apr-devel
sudo yum install -y bison
sudo yum install -y bzip2-devel
sudo yum install -y flex
sudo yum install -y gcc
sudo yum install -y libcurl-devel
sudo yum install -y libevent-devel
sudo yum install -y libkadm5
sudo yum install -y libxml2-devel
sudo yum install -y perl-ExtUtils-Embed
sudo yum install -y python-devel
sudo yum install -y python-paramiko
sudo yum install -y python-psutil
sudo yum install -y python-setuptools
sudo yum install -y readline-devel
sudo yum install -y zlib-devel

# Install pip
sudo yum install -y epel-release
sudo yum install -y python-pip

# Install lockfile with pip because the yum package `python-pip` is too old (0.8).
sudo pip install lockfile
