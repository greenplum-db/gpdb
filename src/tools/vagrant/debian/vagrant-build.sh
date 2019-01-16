set -ex

GPDB_SRC=/home/vagrant/workspace/gpdb
cd "$GPDB_SRC"

export CC="ccache cc"
export CXX="ccache c++"
export PATH=/usr/local/bin:$PATH

rm -rf /usr/local/gpdb
mkdir /usr/local/gpdb
./configure --prefix=/usr/local/gpdb "$@"

make clean
make -j 4
make install

cd ${GPDB_SRC}/gpAux
cp -rp gpdemo /home/vagrant/
cat /home/vagrant/.ssh/id_rsa.pub >> /home/vagrant/.ssh/authorized_keys
# make sure ssh is not stuck asking if the host is known
ssh-keyscan -H localhost >> /home/vagrant/.ssh/known_hosts
ssh-keyscan -H 127.0.0.1 >> /home/vagrant/.ssh/known_hosts
ssh-keyscan -H jessie >> /home/vagrant/.ssh/known_hosts

# BUG: fix the LD_LIBRARY_PATH to find installed GPOPT libraries
echo export LD_LIBRARY_PATH=/usr/local/lib:\$LD_LIBRARY_PATH >> /usr/local/gpdb/greenplum_path.sh

cd /home/vagrant/gpdemo
source /usr/local/gpdb/greenplum_path.sh
make

#cd /vagrant/src/pl/plspython/tests
#make containers
#sudo -u postgres make
