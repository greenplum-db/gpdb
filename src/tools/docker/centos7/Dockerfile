FROM centos:centos7

# Install some basic utilities and build tools
RUN yum makecache && \
    rpm --import http://mirror.centos.org/centos/RPM-GPG-KEY-CentOS-7 && \
    yum -y install epel-release java-1.8.0-openjdk-devel && \
    yum -y install createrepo jq git iproute net-tools openssh-server rpm-build rsync sudo svn vim wget \
        ant-junit autoconf bison cmake flex gcc gperf indent lcov libtool links make maven && \
    yum clean all

# install all software we need
RUN yum makecache && \
    yum -y install python2-paramiko python-devel python-lockfile python-psutil python-setuptools \
        apache-ivy libicu perl-ExtUtils-Embed perl-Env perl-JSON python2-pip && \
    yum -y install apr-devel bzip2-devel expat-devel libcurl-devel libevent-devel libuuid-devel \
        libxml2-devel libyaml-devel openssl-devel pam-devel protobuf-devel readline-devel snappy-devel && \
    pip install --upgrade pip && pip install psi && \
    yum clean all

# setup ssh configuration
RUN ssh-keygen -t rsa -N "" -f /root/.ssh/id_rsa && \
    cat /root/.ssh/id_rsa.pub >> /root/.ssh/authorized_keys && \
    chmod 0600 /root/.ssh/authorized_keys && \
    echo -e "password\npassword" | passwd 2> /dev/null && \
    { ssh-keyscan localhost; ssh-keyscan 0.0.0.0; } >> /root/.ssh/known_hosts && \
    #
    ssh-keygen -f /etc/ssh/ssh_host_key -N '' -t rsa1 && \
    ssh-keygen -f /etc/ssh/ssh_host_rsa_key -N '' -t rsa && \
    ssh-keygen -f /etc/ssh/ssh_host_dsa_key -N '' -t dsa && \
    sed -i -e 's|Defaults    requiretty|#Defaults    requiretty|' /etc/sudoers && \
    sed -ri 's/UsePAM yes/UsePAM no/g;s/PasswordAuthentication yes/PasswordAuthentication no/g' /etc/ssh/sshd_config && \
    sed -ri 's@^HostKey /etc/ssh/ssh_host_ecdsa_key$@#&@;s@^HostKey /etc/ssh/ssh_host_ed25519_key$@#&@' /etc/ssh/sshd_config

# newer version of gcc and run environment for gpdb
RUN yum -y install centos-release-scl && yum -y install --nogpgcheck devtoolset-6-gcc devtoolset-6-gcc-c++ && yum clean all && \
    echo -e 'source /opt/rh/devtoolset-6/enable' >> /opt/gcc_env.sh && \
    echo -e 'source /opt/gcc_env.sh' >> /root/.bashrc && \
    echo -e 'export JAVA_HOME=/etc/alternatives/java_sdk' >> /root/.bashr && \
    mkdir -p /usr/local/gpdb/lib/python
