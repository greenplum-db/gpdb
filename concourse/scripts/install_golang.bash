#!/bin/bash
# temporary install by script
# todo change to container or AMI

set -e

VERSION="1.7.5"

print_help() {
    echo "Usage: bash goinstall.sh OPTION"
    echo -e "\nOPTIONS:"
    echo -e "  --32\t\tInstall 32-bit version"
    echo -e "  --64\t\tInstall 64-bit version"
    echo -e "  --remove\tTo remove currently installed version"
}

if [ "$1" == "--32" ]; then
    DFILE="go$VERSION.linux-386.tar.gz"
elif [ "$1" == "--64" ]; then
    DFILE="go$VERSION.linux-amd64.tar.gz"
elif [ "$1" == "--remove" ]; then
    rm -rf "$HOME/.go/"
    rm -rf "$HOME/go/"
    sed -i '/# GoLang/d' "$HOME/.bashrc"
    sed -i '/export GOROOT/d' "$HOME/.bashrc"
    sed -i '/:$GOROOT/d' "$HOME/.bashrc"
    sed -i '/export GOPATH/d' "$HOME/.bashrc"
    sed -i '/:$GOPATH/d' "$HOME/.bashrc"
    echo "Go removed."
    exit 0
elif [ "$1" == "--help" ]; then
    print_help
    exit 0
else
    print_help
    exit 1
fi

if [ -d "$HOME/.go" ] || [ -d "$HOME/go" ]; then
    echo "Installation directories already exist. Exiting."
    exit 1
fi
echo "Downloading $DFILE ..."
wget https://storage.googleapis.com/golang/$DFILE -O /tmp/go.tar.gz
if [ $? -ne 0 ]; then
    echo "Download failed! Exiting."
    exit 1
fi
echo "Extracting ..."
tar -C "$HOME" -xzf /tmp/go.tar.gz
mv "$HOME/go" "$HOME/.go"

base_path=$(pwd)
export GOPATH=${base_path}/gpdb_src/gpMgmt/go-utils
touch "$HOME/.bashrc"
{
    echo '# GoLang'
    echo "export GOPATH=$GOPATH"
    echo 'export GOROOT=$HOME/.go'
    echo 'export PATH=$PATH:$GOROOT/bin'
    echo 'export PATH=$PATH:$GOPATH/bin'
} >> "$HOME/.bashrc"

mkdir -p $HOME/go/{src,pkg,bin}
rm -f /tmp/go.tar.gz
source $HOME/.bashrc
