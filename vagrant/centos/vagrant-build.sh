#!/bin/bash

sudo rm -rf /home/gpadmin/gpdb_install
sudo mkdir /home/gpadmin/gpdb_install
sudo chown -R gpadmin:gpadmin /home/gpadmin/gpdb*
cd /gpdb
./configure --prefix=/home/gpadmin/gpdb_install --enable-depend --enable-debug --with-python || exit 1
sudo make clean || exit 1
sudo make || exit 1
sudo make install
sudo chown -R gpadmin:gpadmin /home/gpadmin/gpdb_install

