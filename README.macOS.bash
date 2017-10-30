#!/bin/bash

echo "Caching password..."
sudo -K
sudo true;

if hash brew 2>/dev/null; then
	  echo "Homebrew is already installed!"
else
	  echo "Installing Homebrew..."
	  ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"
fi

brew install bash-completion
brew install conan
brew install cmake # gporca
brew install xerces-c #gporca
brew install libyaml   # enables `--enable-mapreduce`
brew install libevent # gpfdist
brew install apr # gpperfmon
brew install apr-util # gpperfmon
brew link --force apr
brew link --force apr-util

# Installing Golang
mkdir -p ~/go/src
brew install go # Or get the latest from https://golang.org/dl/

# Installing python libraries
brew install python
cat >> ~/.bash_profile << EOF
export PATH=/usr/local/opt/python/libexec/bin:$PATH
EOF
source ~/.bash_profile
pip install lockfile psi paramiko pysql psutil setuptools
pip install unittest2 parse pexpect mock pyyaml
pip install git+https://github.com/behave/behave@v1.2.4
pip install pylint

#echo -e "127.0.0.1\t$HOSTNAME" | sudo tee -a /etc/hosts
echo 127.0.0.1$'\t'$HOSTNAME | sudo tee -a /etc/hosts


# OS settings
sudo sysctl -w kern.sysv.shmmax=2147483648
sudo sysctl -w kern.sysv.shmmin=1
sudo sysctl -w kern.sysv.shmmni=64
sudo sysctl -w kern.sysv.shmseg=16
sudo sysctl -w kern.sysv.shmall=524288
sudo sysctl -w net.inet.tcp.msl=60

sudo sysctl -w net.local.dgram.recvspace=262144
sudo sysctl -w net.local.dgram.maxdgram=16384
sudo sysctl -w kern.maxfiles=131072
sudo sysctl -w kern.maxfilesperproc=131072
sudo sysctl -w net.inet.tcp.sendspace=262144
sudo sysctl -w net.inet.tcp.recvspace=262144
sudo sysctl -w kern.ipc.maxsockbuf=8388608

sudo tee -a /etc/sysctl.conf << EOF
kern.sysv.shmmax=2147483648
kern.sysv.shmmin=1
kern.sysv.shmmni=64
kern.sysv.shmseg=16
kern.sysv.shmall=524288
net.inet.tcp.msl=60

net.local.dgram.recvspace=262144
net.local.dgram.maxdgram=16384
kern.maxfiles=131072
kern.maxfilesperproc=131072
net.inet.tcp.sendspace=262144
net.inet.tcp.recvspace=262144
kern.ipc.maxsockbuf=8388608
EOF

# Step: Create GPDB destination directory
sudo mkdir /usr/local/gpdb
sudo chown $USER:admin /usr/local/gpdb

# Step: Configure
cat >> ~/.bashrc << EOF
ulimit -n 65536 65536  # Increases the number of open files
export PGHOST="$(hostname)"
EOF

# Step: GOPATH for Golang
cat >> ~/.bash_profile << EOF
export GOPATH=\$HOME/go:\$HOME/workspace/gpdb/gpMgmt/go-utils
export PATH=\$GOPATH/bin:\$PATH
EOF

# Step: speed up compile time (optional)
cat >> ~/.bashrc << EOF

# This assumes that the macOS machine has 8 threads
export MAKEFLAGS='-j8'
EOF

# Step: install any optional tools
brew install gdb

