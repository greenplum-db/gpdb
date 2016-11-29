cd /gpdb

sudo mkdir /usr/local/gpdb
sudo chown vagrant:vagrant /usr/local/gpdb
./configure --enable-debug --with-python --with-perl --enable-mapreduce --with-libxml --prefix=/usr/local/gpdb

make clean
make -j 4
make install

cd /gpdb/gpAux
cp -rp gpdemo /home/vagrant/
sudo chown -R vagrant:vagrant /home/vagrant/gpdemo
cat /home/vagrant/.ssh/id_rsa.pub >> /home/vagrant/.ssh/authorized_keys
# make sure ssh is not stuck asking if the host is known
ssh-keyscan -H localhost >> /home/vagrant/.ssh/known_hosts
ssh-keyscan -H 127.0.0.1 >> /home/vagrant/.ssh/known_hosts
ssh-keyscan -H debian-jessie >> /home/vagrant/.ssh/known_hosts
cd /home/vagrant/gpdemo
source /usr/local/gpdb/greenplum_path.sh
make cluster


#cd /vagrant/src/pl/plspython/tests
#make containers
#sudo -u postgres make
