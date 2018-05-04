export DATA_DIR="/data"
export GPHOME=/opt/gpdb
SHELL=/bin/bash
source $GPHOME/greenplum_path.sh
MASTER_DATA_DIRECTORY=$DATA_DIR/gpdata/gpmaster/gpsne-1/


gpssh-exkeys -f $DATA_DIR/gpdata/hostlist_singlenode

if [ -f "$DATA_DIR/gpdata/gpmaster/gpsne-1/pg_hba.conf" ]; then
	echo '========================> Simple start GPDB in progress...'
	gpstart -a
else
	echo '========================> Init and start GPDB in progress...'
	gpinitsystem -ac $DATA_DIR/gpdata/gpinitsystem_singlenode
	sleep 2
	echo "ALTER USER gpadmin WITH PASSWORD 'gppassword';" | psql
	echo "host all  all 0.0.0.0/0 password" >> $DATA_DIR/gpdata/gpmaster/gpsne-1/pg_hba.conf
	gpstop -ra
fi

echo '========================> GPDB starting process has completed, check the result above or try to connect'
