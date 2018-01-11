FROM pivotaldata/gpdb-dev:centos6

# create user gpadmin since GPDB cannot run under root
RUN groupadd -g 1000 gpadmin && useradd -u 1000 -g 1000 gpadmin && \
    echo "gpadmin  ALL=(ALL)       NOPASSWD: ALL" > /etc/sudoers.d/gpadmin && \
    groupadd supergroup && usermod -a -G supergroup gpadmin && \
    #
    mkdir /home/gpadmin/.ssh && \
    ssh-keygen -t rsa -N "" -f /home/gpadmin/.ssh/id_rsa && \
    cat /home/gpadmin/.ssh/id_rsa.pub >> /home/gpadmin/.ssh/authorized_keys && \
    chmod 0600 /home/gpadmin/.ssh/authorized_keys && \
    echo -e "password\npassword" | passwd gpadmin 2> /dev/null && \
    { ssh-keyscan localhost; ssh-keyscan 0.0.0.0; } >> /home/gpadmin/.ssh/known_hosts && \
    chown -R gpadmin:gpadmin /home/gpadmin/.ssh && \
    echo -e 'source /opt/gcc_env.sh' >> /home/gpadmin/.bashrc && \
    echo 'sudo /usr/sbin/sshd' >> /home/gpadmin/.bashrc && \
    echo -e 'export JAVA_HOME=/etc/alternatives/java_sdk' >> /home/gpadmin/.bashrc && \
    echo -e 'export GPHOME=/usr/local/gpdb' >> /home/gpadmin/.bashrc && \
    chown -R gpadmin:gpadmin /usr/local/gpdb

USER gpadmin
