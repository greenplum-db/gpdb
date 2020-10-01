ARG BASE_IMAGE=ubuntu:18.04

FROM $BASE_IMAGE

ARG DEBIAN_FRONTEND=noninteractive
ARG BASE_IMAGE

RUN apt-get update -y && \
    apt-get install -y --no-install-recommends apt-utils openjdk-8-jdk software-properties-common && \
    add-apt-repository -y ppa:ubuntu-toolchain-r/test && apt-get update -y && \
    apt-get install -y --no-install-recommends gcc-6 g++-6 cmake && \
    update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-6 60 --slave /usr/bin/g++ g++ /usr/bin/g++-6 && \
    update-alternatives --config gcc && \
    gcc --version && g++ --version && \
    apt-get install -y --no-install-recommends ant \
    bison \
    build-essential \
    ccache \
    curl \
    dirmngr \
    flex \
    git-core \
    iputils-ping \
    iproute2 \
    jq \
    libapr1-dev \
    libbz2-dev \
    libcurl4-gnutls-dev \
    libevent-dev \
    libkrb5-dev \
    libpam-dev \
    libperl-dev \
    libreadline-dev \
    libssl-dev \
    $([ "$BASE_IMAGE" = ubuntu:16.04 ] && echo libxerces-c-dev) \
    libxml2-dev \
    libyaml-dev \
    libzstd1-dev \
    locales \
    maven \
    net-tools \
    ninja-build \
    openssh-server \
    pkg-config \
    python-dev \
    python-pip \
    python-psutil \
    python-setuptools \
    less \
    rsync \
    ssh \
    sudo \
    time \
    unzip \
    vim \
    wget \
    zlib1g-dev && \
    rm -rf /var/lib/apt/lists/*

    # setup ssh configuration
RUN locale-gen en_US.UTF-8 && \
    ssh-keygen -t rsa -N "" -f /root/.ssh/id_rsa && \
    cat /root/.ssh/id_rsa.pub >> /root/.ssh/authorized_keys && \
    chmod 0600 /root/.ssh/authorized_keys && \
    echo "root:password" | chpasswd 2> /dev/null && \
    #
    sed -i -e 's|Defaults    requiretty|#Defaults    requiretty|' /etc/sudoers && \
    sed -ri 's/UsePAM yes/UsePAM no/g;s/PasswordAuthentication yes/PasswordAuthentication no/g' /etc/ssh/sshd_config && \
    sed -ri 's@^HostKey /etc/ssh/ssh_host_ecdsa_key$@#&@;s@^HostKey /etc/ssh/ssh_host_ed25519_key$@#&@' /etc/ssh/sshd_config && \
    service ssh start && \
    { ssh-keyscan localhost; ssh-keyscan 0.0.0.0; } >> /root/.ssh/known_hosts && \
    # create user gpadmin since GPDB cannot run under root
    groupadd -g 1000 gpadmin && useradd -u 1000 -g 1000 gpadmin -s /bin/bash && \
    echo "gpadmin  ALL=(ALL)       NOPASSWD: ALL" > /etc/sudoers.d/gpadmin && \
    groupadd supergroup && usermod -a -G supergroup gpadmin && \
    mkdir -p /home/gpadmin/.ssh && \
    ssh-keygen -t rsa -N "" -f /home/gpadmin/.ssh/id_rsa && \
    cat /home/gpadmin/.ssh/id_rsa.pub >> /home/gpadmin/.ssh/authorized_keys && \
    chmod 0600 /home/gpadmin/.ssh/authorized_keys && \
    echo "gpadmin:password" | chpasswd 2> /dev/null && \
    { ssh-keyscan localhost; ssh-keyscan 0.0.0.0; } >> /home/gpadmin/.ssh/known_hosts && \
    chown -R gpadmin:gpadmin /home/gpadmin && \
    mkdir /usr/local/greenplum-db-devel && \
    chown gpadmin:gpadmin /usr/local/greenplum-db-devel

