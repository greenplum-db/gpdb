sudo /etc/init.d/ssh start
sleep 2
source /home/gp/.bash_profile
ssh-keygen -f /home/gp/.ssh/id_rsa -t rsa -N ""
echo `hostname` > /gpdata/hostlist_singlenode
sed -i 's/hostname_of_machine/`hostname`/g' /gpdata/gpinitsystem_singlenode

gpssh-exkeys -f /gpdata/hostlist_singlenode

pushd /gpdata
  gpinitsystem -ac gpinitsystem_singlenode
  echo "host all  all 0.0.0.0/0 trust" >> /gpdata/gpmaster/gpsne-1/pg_hba.conf
  MASTER_DATA_DIRECTORY=/gpdata/gpmaster/gpsne-1/ gpstop -a
  MASTER_DATA_DIRECTORY=/gpdata/gpmaster/gpsne-1/ gpstart -a
popd
