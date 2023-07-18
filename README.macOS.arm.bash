#!/bin/bash
echo "Caching password..."
sudo -K
sudo true;

if [ ! -d ~/workspace ] ; then
   mkdir ~/workspace && cd ~/workspace
fi

sudo xcode-select -s /Library/Developer/CommandLineTools

if hash brew 2>/dev/null; then
	echo "Homebrew is already installed!"
else
          echo "Installing Homebrew..."
          echo | /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
          if [ $? -eq 1 ]; then
                echo "ERROR : Homebrew Installation Failed, fix the failure and re-run the script."
                exit 1
          fi
fi

#Install xerces-c  library
if [ ! -d ~/workspace/gp-xerces ] ; then
	echo "INFO: xerces is not installed, Installing...."
	git clone https://github.com/greenplum-db/gp-xerces.git -v ~/workspace/gp-xerces
	mkdir ~/workspace/gp-xerces/build
	cd ~/workspace/gp-xerces/build
	~/workspace/gp-xerces/configure --prefix=/usr/local
	make
	sudo make install
        cd -
fi
#brew install xerces-c #gporca
brew install bash-completion
brew install cmake # gporca
brew install libyaml
brew install libevent # gpfdist
brew install apr # gpfdist
brew install apr-util # gpfdist
brew install zstd
brew install libxml2
brew install pkg-config
brew install perl
brew link --force apr
brew link --force apr-util
brew link --force libxml2

# Needed for pygresql, or you can source greenplum_path.sh after compiling database and installing python-dependencies then
brew install postgresql

# Installing python3 libraries
brew install python@3.9
python_version=$(echo `ls  /opt/homebrew/Cellar/python@3.9/`)
ln -s /opt/homebrew/Cellar/python@3.9/$python_version/bin/python3.9 /opt/homebrew/bin/python3
ln -s /opt/homebrew/Cellar/python@3.9/$python_version/bin/pip3.9 /opt/homebrew/bin/pip3
pip3 install -r python-dependencies.txt
brew install pyenv
pyenv install $python_version

# Due to recent update on OS net-tools package. Mac doesn't have support for ss and ip by default.
# Hence as a workaround installing iproute2mac for ip support and creating soft link for ss support
brew install iproute2mac
sudo ln -s /usr/sbin/netstat /usr/local/bin/ss

echo 127.0.0.1$'\t'$(hostname) | sudo tee -a /etc/hosts

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

# Create GPDB destination directory
sudo mkdir /usr/local/gpdb
sudo chown $USER:admin /usr/local/gpdb

# Configure
cat >> ~/.bashrc << EOF
ulimit -n 65536 65536  # Increases the number of open files
export PGHOST="$(hostname)"
export LC_CTYPE="en_US.UTF-8"

eval "$(/opt/homebrew/bin/brew shellenv)"
export PATH="/opt/homebrew/bin:/opt/homebrew/sbin:\$PATH"
export LDFLAGS="-L/opt/homebrew/opt/libxml2/lib -L/opt/homebrew/opt/openssl\@3/lib"
export CPPFLAGS="-I/opt/homebrew/opt/libxml2/include -I/opt/homebrew/opt/openssl\@3/include"
export LD_LIBRARY_PATH="/Users/`whoami`/.pyenv/versions/$python_version/lib/"
export OPENSSL_INCLUDE_DIR="$(brew --prefix openssl)/include"
export OPENSSL_LIB_DIR="$(brew --prefix openssl)/lib"
EOF
source ~/.bashrc

cat << EOF

===============================================================================
INFO :
Please source /usr/local/gpdb/greenplum_path.sh after compiling database, then
pip3 install --user -r python-dependencies.txt
===============================================================================
EOF
