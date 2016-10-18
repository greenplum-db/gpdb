#!/bin/bash
## ======================================================================
## Execute bugbuster setup steps.
## ======================================================================

##
## Required for gpperfmon suite
##

$GPHOME/bin/gpperfmon_install --enable --port 49999 > /dev/null
gpconfig -c gpperfmon_port -v 49999 --masteronly
gpconfig -c gp_enable_gpperfmon -v on

cat >> $MASTER_DATA_DIRECTORY/gpperfmon/conf/gpperfmon.conf <<EOF

##
## Added by bugbuster_setup.sh $( date )
## 

qamode=1
min_query_time=0.5
min_detailed_query_time=0.5
harvest_interval=30

EOF

## Adding pg_hba entry for memquota_role1
echo 'host     all         memquota_role1             0.0.0.0/0 trust' >> $MASTER_DATA_DIRECTORY/pg_hba.conf
echo 'local     all        memquota_role1            trust' >> $MASTER_DATA_DIRECTORY/pg_hba.conf

gpstop -ar



