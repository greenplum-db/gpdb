sudo apt-get update
sudo apt-get -y install apt-transport-https
sudo apt-key adv --keyserver hkp://p80.pool.sks-keyservers.net:80 --recv-keys 58118E89F3A912897C070ADBF76221572C52609D
sudo bash -c 'echo "deb https://apt.dockerproject.org/repo debian-jessie main" > /etc/apt/sources.list.d/docker.list'
sudo apt-get update
sudo apt-get install wget
sudo apt-get -y install docker-engine build-essential libreadline6 \
libreadline6-dev zlib1g-dev bison flex git-core libcurl4-openssl-dev \
python-dev libxml2-dev pkg-config vim libbz2-dev  python-pip \
libapr1-dev libevent-dev libyaml-dev libperl-dev libffi-dev \
python-setuptools-whl libssl-dev

echo locales locales/locales_to_be_generated multiselect     de_DE ISO-8859-1, de_DE ISO-8859-15, de_DE.UTF-8 UTF-8, de_DE@euro ISO-8859-15, en_GB ISO-8859-1, en_GB ISO-8859-15, en_GB.ISO-8859-15 ISO-8859-15, en_GB.UTF-8 UTF-8, en_US ISO-8859-1, en_US ISO-8859-15, en_US.ISO-8859-15 ISO-8859-15, en_US.UTF-8 UTF-8 | debconf-set-selections
echo locales locales/default_environment_locale      select  en_US.UTF-8 | debconf-set-selections
dpkg-reconfigure locales -f noninteractive


su vagrant -c "ssh-keygen -t rsa -f .ssh/id_rsa -q -N ''"

pip install --upgrade pip
pip install cffi --upgrade
pip install lockfile
pip install paramiko --upgrade
pip install setuptools --upgrade
pip install --pre psutil
pip install cryptography --upgrade

sudo service docker start
sudo useradd postgres
sudo usermod -aG docker postgres
sudo usermod -aG docker vagrant
