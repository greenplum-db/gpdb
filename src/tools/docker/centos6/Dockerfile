FROM centos:centos6

## Install some basic utilities and build tools
RUN yum -y install epel-release java-1.7.0-openjdk-devel && \
    yum -y install autoconf bison createrepo flex git gperf libtool && \
    yum -y install openssh-server rpm-build rsync sudo svn vim wget && \
    #
    # install all other software we need
    yum -y install python-paramiko python-pip python-PSI python-devel python-lockfile python-psutil python-setuptools && \
    yum -y install apr-devel bzip2-devel expat-devel libcurl-devel libevent-devel libuuid-devel && \
    yum -y install libxml2-devel libyaml-devel openssl-devel pam-devel readline-devel snappy-devel && \
    yum -y install ant-junit libicu perl-ExtUtils-Embed perl-JSON json-c-devel libgsasl-devel protobuf && \
    pip install --upgrade pip && yum clean all

# create user gpadmin since GPDB cannot run under root
RUN groupadd -g 1000 gpadmin && useradd -u 1000 -g 1000 gpadmin && \
    echo "gpadmin  ALL=(ALL)       NOPASSWD: ALL" > /etc/sudoers.d/gpadmin && \
    groupadd supergroup && usermod -a -G supergroup gpadmin && \
    #
    # setup ssh client keys for root
    ssh-keygen -t rsa -N "" -f /root/.ssh/id_rsa && \
    cat /root/.ssh/id_rsa.pub >> /root/.ssh/authorized_keys && \
    chmod 0600 /root/.ssh/authorized_keys && \
    echo -e "password\npassword" | passwd 2> /dev/null && \
    { ssh-keyscan localhost; ssh-keyscan 0.0.0.0; } >> /root/.ssh/known_hosts && \
    #
    # setup ssh client keys for gpadmin
    mkdir /home/gpadmin/.ssh && \
    ssh-keygen -t rsa -N "" -f /home/gpadmin/.ssh/id_rsa && \
    cat /home/gpadmin/.ssh/id_rsa.pub >> /home/gpadmin/.ssh/authorized_keys && \
    chmod 0600 /home/gpadmin/.ssh/authorized_keys && \
    echo -e "password\npassword" | passwd gpadmin 2> /dev/null && \
    { ssh-keyscan localhost; ssh-keyscan 0.0.0.0; } >> /home/gpadmin/.ssh/known_hosts && \
    chown -R gpadmin:gpadmin /home/gpadmin/.ssh && \
    #
    # setup system configuration
    ssh-keygen -f /etc/ssh/ssh_host_key -N '' -t rsa1 && \
    ssh-keygen -f /etc/ssh/ssh_host_rsa_key -N '' -t rsa && \
    ssh-keygen -f /etc/ssh/ssh_host_dsa_key -N '' -t dsa && \
    sed -i -e 's|Defaults    requiretty|#Defaults    requiretty|' /etc/sudoers && \
    sed -ri 's/UsePAM yes/UsePAM no/g;s/PasswordAuthentication yes/PasswordAuthentication no/g' /etc/ssh/sshd_config && \
    sed -ri 's@^HostKey /etc/ssh/ssh_host_ecdsa_key$@#&@;s@^HostKey /etc/ssh/ssh_host_ed25519_key$@#&@' /etc/ssh/sshd_config

# need newer version of gcc
RUN yum -y install centos-release-scl && yum -y install --nogpgcheck cmake3 devtoolset-6-gcc devtoolset-6-gcc-c++ && \
    yum -y install https://s3-us-west-2.amazonaws.com/pivotal-public/apache-maven-3.3.9-1.el6.noarch.rpm && \
    yum clean all && rm -rf /usr/bin/cmake && ln -s /usr/bin/cmake3 /usr/bin/cmake && \
    echo -e 'source /opt/rh/devtoolset-6/enable' >> /opt/gcc_env.sh && \
    echo -e 'source /opt/gcc_env.sh' >> /home/gpadmin/.bashrc

# setup run environment
RUN mkdir -p /usr/local/gpdb/lib/python /home/build && \
    chown -R gpadmin:gpadmin /usr/local/gpdb /home/build && \
    echo 'sudo /usr/sbin/sshd' >> /home/gpadmin/.bashrc && \
    echo 'export USER=gpadmin' >> /home/gpadmin/.bashrc && \
    echo 'export LANG=en_US.UTF-8' >> /home/gpadmin/.bashrc && \
    echo 'export PG_MODE=GPDB' >> /home/gpadmin/.bashrc && \
    echo 'export PGPORT=15432' >> /home/gpadmin/.bashrc && \
    echo 'export USER=gpadmin' >> /home/gpadmin/.bashrc && \
    echo 'export BUILD_HOME=/home/build' >> /home/gpadmin/.bashrc && \
    echo 'export GPHOME=/usr/local/gpdb' >> /home/gpadmin/.bashrc && \
    echo 'export PXF_HOME=$GPHOME/pxf' >> /home/gpadmin/.bashrc && \
    echo 'export JAVA_HOME=/etc/alternatives/java_sdk' >> /home/gpadmin/.bashrc && \
    echo 'export CONFIGURE_FLAGS="--enable-debug --with-perl --with-python --with-libxml --disable-orca --prefix=/usr/local/gpdb"' && \
    echo 'export MASTER_DATA_DIRECTORY=/home/build/gpdb/gpAux/gpdemo/datadirs/qddir/demoDataDir-1' >> /home/gpadmin/.bashrc

USER gpadmin
WORKDIR /home/build

