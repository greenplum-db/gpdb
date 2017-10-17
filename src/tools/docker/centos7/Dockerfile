FROM centos:centos7

# Install some basic utilities and build tools
RUN yum makecache && \
    rpm --import http://mirror.centos.org/centos/RPM-GPG-KEY-CentOS-7 && \
    yum -y install epel-release java-1.8.0-openjdk-devel && \
    yum -y install createrepo jq git iproute net-tools openssh-server rpm-build rsync sudo svn vim wget \
        ant-junit autoconf bison cmake flex gcc gcc-c++ gperf indent lcov libtool links make maven && \
    yum clean all

# install all software we need
RUN yum makecache && \
    yum -y install python2-paramiko python-devel python-lockfile python-psutil python-setuptools \
        apache-ivy libicu perl-ExtUtils-Embed perl-Env perl-JSON python2-pip && \
    yum -y install apr-devel bzip2-devel expat-devel libcurl-devel libevent-devel libuuid-devel \
        libxml2-devel libyaml-devel openssl-devel pam-devel protobuf-devel readline-devel snappy-devel && \
    pip install --upgrade pip && pip install psi && \
    yum clean all

# create user gpadmin since GPDB cannot run under root
RUN groupadd -g 1000 gpadmin && useradd -u 1000 -g 1000 gpadmin && \
    echo "gpadmin  ALL=(ALL)       NOPASSWD: ALL" > /etc/sudoers.d/gpadmin && \
    groupadd supergroup && usermod -a -G supergroup gpadmin && \
    # setup ssh client keys for root
    ssh-keygen -t rsa -N "" -f /root/.ssh/id_rsa && \
    cat /root/.ssh/id_rsa.pub >> /root/.ssh/authorized_keys && \
    chmod 0600 /root/.ssh/authorized_keys && \
    echo -e "password\npassword" | passwd 2> /dev/null && \
    { ssh-keyscan localhost; ssh-keyscan 0.0.0.0; } >> /root/.ssh/known_hosts && \
    # setup ssh client keys for gpadmin
    mkdir /home/gpadmin/.ssh && \
    ssh-keygen -t rsa -N "" -f /home/gpadmin/.ssh/id_rsa && \
    cat /home/gpadmin/.ssh/id_rsa.pub >> /home/gpadmin/.ssh/authorized_keys && \
    chmod 0600 /home/gpadmin/.ssh/authorized_keys && \
    echo -e "password\npassword" | passwd gpadmin 2> /dev/null && \
    { ssh-keyscan localhost; ssh-keyscan 0.0.0.0; } >> /home/gpadmin/.ssh/known_hosts && \
    chown -R gpadmin:gpadmin /home/gpadmin/.ssh && \
    # setup system configuration
    ssh-keygen -f /etc/ssh/ssh_host_key -N '' -t rsa1 && \
    ssh-keygen -f /etc/ssh/ssh_host_rsa_key -N '' -t rsa && \
    ssh-keygen -f /etc/ssh/ssh_host_dsa_key -N '' -t dsa && \
    sed -i -e 's|Defaults    requiretty|#Defaults    requiretty|' /etc/sudoers && \
    sed -ri 's/UsePAM yes/UsePAM no/g;s/PasswordAuthentication yes/PasswordAuthentication no/g' /etc/ssh/sshd_config && \
    sed -ri 's@^HostKey /etc/ssh/ssh_host_ecdsa_key$@#&@;s@^HostKey /etc/ssh/ssh_host_ed25519_key$@#&@' /etc/ssh/sshd_config

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
