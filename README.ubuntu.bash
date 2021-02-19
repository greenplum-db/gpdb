#!/bin/bash

apt-get update
apt-get install -y \
  bison \
  ccache \
  cmake \
  curl \
  flex \
  git-core \
  gcc \
  g++ \
  inetutils-ping \
  krb5-kdc \
  krb5-admin-server \
  libapr1-dev \
  libbz2-dev \
  libcurl4-gnutls-dev \
  libevent-dev \
  libkrb5-dev \
  libpam-dev \
  libperl-dev \
  libreadline-dev \
  libssl-dev \
  libxerces-c-dev \
  libxml2-dev \
  libyaml-dev \
  libzstd-dev \
  locales \
  net-tools \
  ninja-build \
  openssh-client \
  openssh-server \
  openssl \
  python3-dev \
  python3-pip \
  python3-psutil \
  python3-pygresql \
  python3-yaml \
  zlib1g-dev

pip3 install conan
