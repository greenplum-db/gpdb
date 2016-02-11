#!/bin/bash

set -x

cd ~
rm ~/.bashrc
printf '#!/bin/bash\n\n'                                     >> ~/.bashrc
printf '. /etc/bashrc\n\n'                                   >> ~/.bashrc
printf '\n# GPDB environment variables\nexport GPDB=/gpdb\n' >> ~/.bashrc
printf 'export GPHOME=/usr/local/greenplum-db\n'             >> ~/.bashrc
printf 'export GPDATA=/data\n'                               >> ~/.bashrc
printf 'if [ -e $GPHOME/greenplum_path.sh ]; then\n\t'       >> ~/.bashrc
printf 'source $GPHOME/greenplum_path.sh\nfi\n'              >> ~/.bashrc
source ~/.bashrc

killall postgres
sleep 5
killall postgres -s SIGKILL
sudo rm -rf $GPDATA
sudo mkdir -p $GPDATA
sudo chown -R vagrant:vagrant $GPDATA
mkdir $GPDATA/master
mkdir $GPDATA/primary

gpssh-exkeys -h `hostname`
hostname > $GPDATA/hosts

GPCFG=$GPDATA/gpinitsystem_config
rm -f $GPCFG
printf "declare -a DATA_DIRECTORY=($GPDATA/primary $GPDATA/primary)\n" >> $GPCFG
printf "MASTER_HOSTNAME=`hostname`\n"                                  >> $GPCFG
printf "MACHINE_LIST_FILE=$GPDATA/hosts\n"                             >> $GPCFG
printf "MASTER_DIRECTORY=$GPDATA/master\n"                             >> $GPCFG
printf "ARRAY_NAME=\"GPDB\" \n"                                        >> $GPCFG
printf "SEG_PREFIX=gpseg\n"                                            >> $GPCFG
printf "PORT_BASE=40000\n"                                             >> $GPCFG
printf "MASTER_PORT=5432\n"                                            >> $GPCFG
printf "TRUSTED_SHELL=ssh\n"                                           >> $GPCFG
printf "CHECK_POINT_SEGMENTS=8\n"                                      >> $GPCFG
printf "ENCODING=UNICODE\n"                                            >> $GPCFG

$GPHOME/bin/gpinitsystem -a -c $GPDATA/gpinitsystem_config
printf '# Remember the master data directory location\n' >> ~/.bashrc
printf 'export MASTER_DATA_DIRECTORY=$GPDATA/master/gpseg-1\n' >> ~/.bashrc
source ~/.bashrc
