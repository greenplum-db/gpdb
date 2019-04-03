sudo /etc/init.d/ssh start
sleep 2
source /home/gpadmin/.bash_profile
ssh-keygen -f /home/gpadmin/.ssh/id_rsa -t rsa -N ""
echo `hostname` > /data/hostlist_singlenode
sed -i 's/hostname_of_machine/`hostname`/g' /data/gpinitsystem_singlenode

gpssh-exkeys -f /data/hostlist_singlenode

pushd /data
  gpinitsystem -ac gpinitsystem_singlenode
  echo "host all  all 0.0.0.0/0 trust" >> /data/master/gpsne-1/pg_hba.conf
  MASTER_DATA_DIRECTORY=/data/master/gpsne-1/ gpstop -a
  MASTER_DATA_DIRECTORY=/data/master/gpsne-1/ gpstart -a
popd
