#!/bin/bash
set -ex

# this entire file runs as sudo
apt-get update

# docker-engine prep
apt-get upgrade -y
apt-get -y install apt-transport-https dirmngr
KEY=F76221572C52609D
# sometimes sks-keyservers doesn't respond, so use others as backup
for server in p80.pool.sks-keyservers.net pgp.mit.edu keyserver.ubuntu.com; do
  apt-key adv --no-tty --recv-key --keyserver "hkp://${server}:80" "$KEY" && break
done

echo 'deb https://apt.dockerproject.org/repo debian-stretch main' > /etc/apt/sources.list.d/docker.list
echo 'deb http://ftp.debian.org/debian stretch-backports main' >>/etc/apt/sources.list
apt-get update

dependencies=(
bison
build-essential
ccache
docker-engine
flex
git-core
htop
libapr1-dev
libbz2-dev
libcurl4-openssl-dev
libevent-dev
libffi-dev
libperl-dev
libreadline-dev
libssl-dev
libxerces-c-dev
libxml2-dev
libyaml-dev
pkg-config
python-dev
python-pip
python-setuptools
vim
zlib1g-dev
)
# gporca: install a version of cmake > 3.1.0
[[ $1 == --setup-gporca ]] && dependencies+=(cmake)
apt-get -y install "${dependencies[@]}"

# need a backported libzstd to avoid implicit
# declaration of functions warning/errors
apt-get -yt stretch-backports install libzstd-dev

echo locales locales/locales_to_be_generated multiselect     de_DE ISO-8859-1, de_DE ISO-8859-15, de_DE.UTF-8 UTF-8, de_DE@euro ISO-8859-15, en_GB ISO-8859-1, en_GB ISO-8859-15, en_GB.ISO-8859-15 ISO-8859-15, en_GB.UTF-8 UTF-8, en_US ISO-8859-1, en_US ISO-8859-15, en_US.ISO-8859-15 ISO-8859-15, en_US.UTF-8 UTF-8 | debconf-set-selections
echo locales locales/default_environment_locale      select  en_US.UTF-8 | debconf-set-selections
dpkg-reconfigure locales -f noninteractive

pip install setuptools --upgrade
pip install cffi --upgrade
pip install lockfile
pip install --pre psutil
pip install cryptography --upgrade

service docker start
useradd postgres
usermod -aG docker postgres
usermod -aG docker vagrant

chown -R vagrant:vagrant /usr/local
# make sure system can handle enough semiphores
echo 'kernel.sem = 500 1024000 200 4096' >> /etc/sysctl.d/gpdb.conf
sysctl -p /etc/sysctl.d/gpdb.conf
