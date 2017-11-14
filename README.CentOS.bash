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
    perl-ExtUtils-Embed \
    python-devel \
    python-paramiko \
    python-pip \
    python-psutil \
    python-setuptools \
    readline-devel \
    xerces-c-devel \
    zlib-devel


# Install lockfile with pip because the yum package `python-pip` is too old (0.8).
sudo pip install lockfile conan
