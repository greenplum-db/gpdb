cd /gpdb
./configure --enable-debug --with-python --with-java --with-libxml --prefix=/usr/local/gpdb

make clean
make -j4  
sudo make install

cd /gpdb/gpAux
cp -rp gpdemo /home/vagrant/
sudo chown -R vagrant:vagrant /home/vagrant/gpdemo
cat /home/vagrant/.ssh/id_rsa.pub >> /home/vagrant/.ssh/authorized_keys
cd /home/vagrant/gpdemo
source /usr/local/gpdb/greenplum_path.sh
make cluster


#cd /vagrant/src/pl/plspython/tests
#make containers
#sudo -u postgres make
