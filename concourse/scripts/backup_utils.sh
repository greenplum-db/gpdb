#!/bin/bash

copy_backup_files() {
    source /usr/local/greenplum-db-devel/greenplum_path.sh;
    gpssh -f hostfile_all "bash -c \"\
        cd /data/gpdata
        find . -type d -name db_dumps > /tmp/db_dumps.files
        tar cf /tmp/db_dumps.tar -T /tmp/db_dumps.files
    \""
}

restore_backup_files() {
    source /usr/local/greenplum-db-devel/greenplum_path.sh;
    gpssh -f hostfile_all "bash -c \"\
        tar xf /tmp/db_dumps.tar -C /data/gpdata
    \""
}

destroy_gpdb() {
    # Setup environment
    source /usr/local/greenplum-db-devel/greenplum_path.sh;
    export PGPORT=5432;
    export MASTER_DATA_DIRECTORY=/data/gpdata/master/gpseg-1;

    yes|gpdeletesystem -f -d $MASTER_DATA_DIRECTORY
    rm -rf $GPHOME/*
}

setup_ddboost() {
    ssh centos@mdw "sudo rpm -ivh https://discuss.pivotal.io/hc/en-us/article_attachments/201458518/compat-libstdc__-33-3.2.3-69.el6.x86_64.rpm"
    ssh centos@mdw "echo \"$DD_SOURCE_HOST source-dd\" | sudo tee -a /etc/hosts"
    ssh centos@mdw "echo \"$DD_DEST_HOST dest-dd\" | sudo tee -a /etc/hosts"

    ssh centos@sdw1 "sudo rpm -ivh https://discuss.pivotal.io/hc/en-us/article_attachments/201458518/compat-libstdc__-33-3.2.3-69.el6.x86_64.rpm"
    ssh centos@sdw1 "echo \"$DD_SOURCE_HOST source-dd\" | sudo tee -a /etc/hosts"
    ssh centos@sdw1 "echo \"$DD_DEST_HOST dest-dd\" | sudo tee -a /etc/hosts"
}

netbackup_add_policy(){
    client_ip=$1
    count=0
    ret=$?
    echo "Modifying netbackup client info"
    HARDWARE="Linux"
    OS="RedHat2.6.18"
    policy_name="gpdb"
    netbackup_path="/usr/openv/netbackup/bin"

    echo "Attempting to add netbackup client at ip $client_ip to approved clients list; it may already be on list, and may say 'entity already exists'"

    cmd="ssh -t ec2-user@${NETBACKUP_SERVER} \"sudo ${netbackup_path}/admincmd/bpplclients ${policy_name} -M ${NETBACKUP_SERVER} -add ${client_ip} ${HARDWARE} ${OS}  \""
    while [ $ret -ne 226 ] && [ $count -le 3 ]; do
        eval $cmd || ret=$?
        let count+=1
    done
}

setup_netbackup_key() {
    # Setup ssh keys
    echo -n $NETBACKUP_KEY | base64 -d > ~/netbackup_server.key
    chmod 600 ~/netbackup_server.key

    eval `ssh-agent -s`
    ssh-add ~/netbackup_server.key

    ssh-keyscan $NETBACKUP_SERVER >> ~/.ssh/known_hosts
}

install_netbackup_client() {
    cat >> /tmp/install_client.sh << EOF
#!/usr/bin/expect
set clientname [exec hostname -i]
set timeout -1
spawn sudo ./install
expect "Do you wish to continue?"
send "y\n"
expect "Do you want to install the NetBackup client software for this client?"
send "y\n"
expect "Enter the name of the NetBackup master server"
send "$NETBACKUP_SERVER\n"
expect "name of the NetBackup client?"
send "n\n"
expect "Enter the name of this NetBackup client"
send "\$clientname\n"
expect eof
EOF
  source /usr/local/greenplum-db-devel/greenplum_path.sh
  gpscp -h mdw -h sdw1 -u centos /tmp/install_client.sh =:/tmp/install_client.sh
  gpssh -h mdw -h sdw1 -u centos "bash -c \"\
    sudo yum install -y expect
    cd /data/gpdata
    sudo tar -xvf NetBackup_7.7.3_CLIENTS_RHEL_2.6.18.tar.gz
    cd NetBackup_7.7.3_CLIENTS2
    sudo mv /tmp/install_client.sh .
    sudo chmod +x install_client.sh
    ./install_client.sh
  \""
}
