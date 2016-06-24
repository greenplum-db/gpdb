sudo apt-get update
sudo apt-get -y install apt-transport-https
sudo apt-key adv --keyserver hkp://p80.pool.sks-keyservers.net:80 --recv-keys 58118E89F3A912897C070ADBF76221572C52609D
sudo bash -c 'echo "deb https://apt.dockerproject.org/repo debian-jessie main" > /etc/apt/sources.list.d/docker.list'
sudo apt-get update
sudo apt-get install wget
sudo apt-get -y install docker-engine build-essential libreadline6 \
libreadline6-dev zlib1g-dev bison flex git-core libcurl4-openssl-dev \
python-dev libxml2-dev pkg-config vim libbz2-dev  python-pip \
libapr1-dev libevent-dev

su vagrant -c "ssh-keygen -t rsa -f .ssh/id_rsa -q -N ''"

pip install lockfile
pip install paramiko
pip install setuptools
pip install epydoc
pip install --pre psutil

sudo service docker start
sudo useradd postgres
sudo usermod -aG docker postgres
sudo usermod -aG docker vagrant
