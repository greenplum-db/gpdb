export DATA_DIR="/data"

if [ ! -f "/opt/gpdb/greenplum_path.sh" ]; then
	echo '========================> Install GPDB'
	apt update
	apt install -y \
		software-properties-common \
		python-software-properties \
		less \
		ssh \
		sudo \
		time \
		libzstd1-dev
	add-apt-repository -y ppa:greenplum/db
	apt update

	apt install -y greenplum-db-oss
	source /opt/gpdb/greenplum_path.sh
	locale-gen en_US.utf8
fi

if [ ! -d "$DATA_DIR/gpdata" ]; then
	echo '========================> Make $DATA_DIR/gpdata/'
	mkdir -p $DATA_DIR/gpdata/gpdata1
	mkdir -p $DATA_DIR/gpdata/gpdata2
	mkdir -p $DATA_DIR/gpdata/gpmaster
fi

if [ ! -f "$DATA_DIR/gpdata/gpinitsystem_singlenode" ]; then
	echo '========================> Make gpinitsystem_singlenode and hostlist_singlenode'
	echo 'ARRAY_NAME="GPDB SINGLENODE"' > $DATA_DIR/gpdata/gpinitsystem_singlenode
	echo 'MACHINE_LIST_FILE='$DATA_DIR'/gpdata/hostlist_singlenode' >> $DATA_DIR/gpdata/gpinitsystem_singlenode
	echo 'SEG_PREFIX=gpsne' >> $DATA_DIR/gpdata/gpinitsystem_singlenode
	echo 'PORT_BASE=40000' >> $DATA_DIR/gpdata/gpinitsystem_singlenode
	echo 'declare -a DATA_DIRECTORY=('$DATA_DIR'/gpdata/gpdata1 '$DATA_DIR'/gpdata/gpdata2)' >> $DATA_DIR/gpdata/gpinitsystem_singlenode
	echo 'MASTER_HOSTNAME=dwgpdb' >> $DATA_DIR/gpdata/gpinitsystem_singlenode
	echo 'MASTER_DIRECTORY='$DATA_DIR'/gpdata/gpmaster' >> $DATA_DIR/gpdata/gpinitsystem_singlenode
	echo 'MASTER_PORT=5432' >> $DATA_DIR/gpdata/gpinitsystem_singlenode
	echo 'TRUSTED_SHELL=ssh' >> $DATA_DIR/gpdata/gpinitsystem_singlenode
	echo 'CHECK_POINT_SEGMENTS=8' >> $DATA_DIR/gpdata/gpinitsystem_singlenode
	echo 'ENCODING=UNICODE' >> $DATA_DIR/gpdata/gpinitsystem_singlenode
	echo 'DATABASE_NAME=gpadmin' >> $DATA_DIR/gpdata/gpinitsystem_singlenode

	echo dwgpdb > $DATA_DIR/gpdata/hostlist_singlenode
fi

if [ ! -d "/home/gpadmin" ]; then
	echo '========================> Add gpadmin'
	useradd -md /home/gpadmin/ gpadmin
	chown gpadmin -R $DATA_DIR/gpdata
	echo  "export DATA_DIR=$DATA_DIR" >> /home/gpadmin/.profile
	echo  "export MASTER_DATA_DIRECTORY=$DATA_DIR/gpdata/gpmaster/gpsne-1" >> /home/gpadmin/.profile
	echo  "export GPHOME=/opt/gpdb" >> /home/gpadmin/.profile
	echo  "source $GPHOME/greenplum_path.sh" >> /home/gpadmin/.profile

	echo  "export DATA_DIR=$DATA_DIR" >> /home/gpadmin/.bashrc
	echo  "export MASTER_DATA_DIRECTORY=$DATA_DIR/gpdata/gpmaster/gpsne-1" >> /home/gpadmin/.bashrc
	echo  "export GPHOME=/opt/gpdb" >> /home/gpadmin/.bashrc
	echo  "source $GPHOME/greenplum_path.sh" >> /home/gpadmin/.bashrc

	chown gpadmin:gpadmin /home/gpadmin/.profile
	su - gpadmin bash -c 'mkdir /home/gpadmin/.ssh'
	ssh-keygen -f /home/gpadmin/.ssh/id_rsa -t rsa -N ""
	chown -R gpadmin:gpadmin /home/gpadmin/.ssh/*
fi

echo '========================> GPDB is starting...'
/etc/init.d/ssh start
sleep 2
chown gpadmin:gpadmin /gpdb_start.sh
chmod +x /gpdb_start.sh
su - gpadmin bash -c '/gpdb_start.sh'



