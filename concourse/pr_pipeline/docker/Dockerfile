FROM ubuntu:18.04


ENV DEBIAN_FRONTEND=noninteractive


#
# Install necessary dependencies for GPDB testing
#
RUN apt update && apt install -y \
    dirmngr \
    less \
    ssh \
    sudo \
    time \
    iproute2 \
    libzstd1-dev \
    bison \
    ccache \
    cmake \
    curl \
    flex \
    git-core \
    gcc \
    g++ \
    krb5-admin-server \
    krb5-kdc \
    inetutils-ping \
    libapr1-dev \
    libbz2-dev \
    libcurl4-gnutls-dev \
    libevent-dev \
    libkrb5-dev \
    libpam-dev \
    libperl-dev \
    libreadline-dev \
    libssl-dev \
    libxml2-dev \
    libyaml-dev \
    libzstd-dev \
    locales \
    net-tools \
    ninja-build \
    openssh-client \
    openssh-server \
    openssl \
    python-dev \
    python-lockfile \
    python-paramiko \
    python-pip \
    python-psutil \
    python-yaml \
    zlib1g-dev \
    cpanminus

#
# Prepare debugging environment
#
RUN apt install -y \
    ctags \
    emacs-nox \
    vim \
    gdb

#
# Root specific steps
#
RUN useradd -md /home/gpadmin/ gpadmin

RUN echo "gpadmin ALL=(root) NOPASSWD:ALL" > /etc/sudoers.d/gpadmin && \
    chmod 0440 /etc/sudoers.d/gpadmin

RUN locale-gen en_US.UTF-8

RUN printf "#!/bin/bash\nsudo service ssh start" > /start-sshd.bash && \
    chmod +x /start-sshd.bash

ADD /cpanfile /cpanfile

RUN cpanm --installdeps /

#
#  All user specific steps
#
USER gpadmin
    
RUN ssh-keygen -f ~/.ssh/id_rsa -N '' \
    && cp ~/.ssh/id_rsa.pub ~/.ssh/authorized_keys

RUN sudo service ssh start && \
    ssh-keyscan -H localhost >> ~/.ssh/known_hosts

