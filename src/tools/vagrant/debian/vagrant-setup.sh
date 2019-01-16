set -e

# this entire file runs as sudo
sudo apt-get update
sudo apt-get upgrade -y
sudo apt-get -y install apt-transport-https
sudo apt-key adv --recv-key --keyserver hkp://p80.pool.sks-keyservers.net:80 58118E89F3A912897C070ADBF76221572C52609D ||
sudo apt-key adv --recv-key --keyserver hkp://pgp.mit.edu:80 58118E89F3A912897C070ADBF76221572C52609D ||
sudo apt-key adv --recv-key --keyserver hkp://keyserver.ubuntu.com:80 58118E89F3A912897C070ADBF76221572C52609D
sudo bash -c 'echo "deb https://apt.dockerproject.org/repo debian-jessie main" > /etc/apt/sources.list.d/docker.list'
sudo apt-get update
sudo apt-get -y install docker-engine build-essential libreadline6 \
libreadline6-dev zlib1g-dev bison flex git-core libcurl4-openssl-dev \
python-dev libxml2-dev pkg-config vim libbz2-dev python-pip \
libapr1-dev libevent-dev libyaml-dev libperl-dev libffi-dev \
python-setuptools-whl libssl-dev ccache htop

echo locales locales/locales_to_be_generated multiselect     de_DE ISO-8859-1, de_DE ISO-8859-15, de_DE.UTF-8 UTF-8, de_DE@euro ISO-8859-15, en_GB ISO-8859-1, en_GB ISO-8859-15, en_GB.ISO-8859-15 ISO-8859-15, en_GB.UTF-8 UTF-8, en_US ISO-8859-1, en_US ISO-8859-15, en_US.ISO-8859-15 ISO-8859-15, en_US.UTF-8 UTF-8 | debconf-set-selections
echo locales locales/default_environment_locale      select  en_US.UTF-8 | debconf-set-selections
dpkg-reconfigure locales -f noninteractive

su vagrant -c "ssh-keygen -t rsa -f .ssh/id_rsa -q -N ''"

pip install setuptools --upgrade
pip install cffi --upgrade
pip install lockfile
pip install paramiko --upgrade
pip install --pre psutil
pip install cryptography --upgrade
pip install enum34

sudo service docker start
sudo useradd postgres
sudo usermod -aG docker postgres
sudo usermod -aG docker vagrant

sudo chown -R vagrant:vagrant /usr/local
# make sure system can handle enough semiphores
echo 'kernel.sem = 500 1024000 200 4096' | sudo tee -a /etc/sysctl.d/gpdb.conf >/dev/null
sudo sysctl -p /etc/sysctl.d/gpdb.conf

# install a version of cmake > 3.1.0 (debian jessie doesn't offer it)
# and zstd header files
if [[ $1 == --setup-gporca ]]; then
  cat <<EOF >>/etc/apt/sources.list
  deb http://ftp.debian.org/debian jessie-backports main
  deb http://ftp.debian.org/debian jessie-backports-sloppy main
EOF
 apt-get update
 apt-get -yt jessie-backports install cmake
 apt-get -yt jessie-backports-sloppy install libzstd-dev
fi
