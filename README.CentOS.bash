#!/bin/bash

# Install needed packages. Please add to this list if you discover additional prerequisites.
sudo yum install -y epel-release
sudo yum install -y \
    apr-devel \
    bison \
    bzip2-devel \
    cmake3 \
    flex \
    gcc \
    gcc-c++ \
    krb5-devel \
    libcurl-devel \
    libevent-devel \
    libkadm5 \
    libyaml-devel \
    libxml2-devel \
    libzstd-devel \
    openssl-devel \
    perl-ExtUtils-Embed \
    python3-devel \
    python3-pip \
    readline-devel \
    xerces-c-devel \
    zlib-devel

# Needed for pygresql, or you can source greenplum_path.sh after compiling database and installing python-dependencies then
sudo yum install -y \
    postgresql \
    postgresql-devel

sudo pip3 install conan
sudo pip3 install -r python-dependencies.txt
