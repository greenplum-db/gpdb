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
    python-devel \
    python-pip \
    readline-devel \
    xerces-c-devel \
    zlib-devel

sudo pip install conan
sudo pip install -r python-dependencies.txt
sudo pip install -r python-developer-dependencies.txt
