#!/bin/bash
set -x
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
          if [[ $? == 1 ]]; then
                echo "ERROR : Homebrew Installation Failed, fix the failure and re-run the script."
                exit 1
          fi
fi

#Install xerces-c library
if [ ! -d ~/workspace/gp-xerces ] ; then
	echo "INFO: xerces is not installed, Installing...." 
        git clone https://github.com/greenplum-db/gp-xerces.git -v ~/workspace/gp-xerces
        mkdir ~/workspace/gp-xerces/build 
        cd ~/workspace/gp-xerces/build
        ~/workspace/gp-xerces/configure --prefix=/usr/local
        make 
        sudo make install
        cd -
fi

#brew install xerces-c #gporca
brew install bash-completion
brew install conan
brew install cmake # gporca
brew install libyaml   # enables `--enable-mapreduce`
brew install libevent # gpfdist
brew install apr # gpperfmon
brew install apr-util # gpperfmon
brew install libxml2
brew install pkg-config
brew link --force apr
brew link --force apr-util
brew link libxml2 --force

# Installing Golang
mkdir -p ~/go/src
brew install go # Or get the latest from https://golang.org/dl/

# Installing python2 libraries
brew install python2
brew install pyenv
pyenv install 2.7.18
ln -s ~/.pyenv/versions/2.7.18/bin/python2 /usr/local/bin/python2
ln -s ~/.pyenv/versions/2.7.18/bin/pip2 /usr/local/bin/pip2
ln -s /usr/local/bin/python2 /usr/local/bin/python
ln -s /usr/local/bin/pip2 /usr/local/bin/pip
pip install -r python-dependencies.txt
pip install -r python-developer-dependencies.txt
python -m pip install psutil

# Installing python3 libraries
rm -rf /opt/homebrew/bin/python3
rm -rf /opt/homebrew/bin/pip3
brew install python@3.9
python_version=$(echo `ls  /opt/homebrew/Cellar/python@3.9/`)
ln -s /opt/homebrew/Cellar/python@3.9/$python_version/bin/python3.9 /opt/homebrew/bin/python3
ln -s /opt/homebrew/Cellar/python@3.9/$python_version/bin/pip3.9 /opt/homebrew/bin/pip3
pip3 install -r python-dependencies.txt
pyenv install $python_version

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

# Step: Create GPDB destination directory
sudo mkdir /usr/local/gpdb
sudo chown $USER:admin /usr/local/gpdb

# Step: Configure
cat >> ~/.bashrc << EOF
ulimit -n 65536 65536  # Increases the number of open files
export PGHOST="$(hostname)"
eval "$(/opt/homebrew/bin/brew shellenv)"
export PATH="/opt/homebrew/bin:/opt/homebrew/sbin:\$PATH";
alias python="/usr/local/bin/python"

export LDFLAGS="-L/opt/homebrew/opt/libxml2/lib -L/opt/homebrew/opt/openssl\@3/lib"
export CPPFLAGS="-I/opt/homebrew/opt/libxml2/include -I/opt/homebrew/opt/openssl\@3/include"
export LD_LIBRARY_PATH="/Users/`whoami`/.pyenv/versions/${python_version}/lib/"
export OPENSSL_INCLUDE_DIR="$(brew --prefix openssl)/include"
export OPENSSL_LIB_DIR="$(brew --prefix openssl)/lib"
EOF
source ~/.bashrc

# Step: GOPATH for Golang
cat >> ~/.bash_profile << EOF
export GOPATH=\$HOME/go
export PATH=\$HOME/go/bin:\$PATH
EOF
source ~/.bash_profile

#Softlinks for openssl
ln -s /opt/homebrew/opt/openssl@1.0/include/openssl /usr/local/include/openssl
ln -s /opt/homebrew/opt/openssl@1.0//lib/* /usr/local/lib/

cat << EOF
===============================================================================
INFO :
Please source /usr/local/gpdb/greenplum_path.sh after compiling database, then
pip install --user -r python-dependencies.txt
===============================================================================
EOF
