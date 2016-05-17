FROM centos:6

RUN yum -y update
RUN yum -y groupinstall "Development tools"
RUN yum -y install ed
RUN yum -y install readline-devel
RUN yum -y install zlib-devel
RUN yum -y install curl-devel
RUN yum -y install bzip2-devel
RUN yum -y install python-devel
RUN yum -y install apr-devel
RUN yum -y install libevent-devel
RUN yum -y install openssl-libs openssl-devel
RUN yum -y install libyaml libyaml-devel
RUN yum -y install epel-release
RUN yum -y install htop
RUN yum -y install ccache
RUN yum -y install libffi-devel
RUN yum -y install iproute
RUN yum -y install net-tools
RUN yum -y install openssh-server
RUN yum -y install perl-devel
RUN yum -y install perl-CPAN
RUN yum -y install perl-ExtUtils-Embed
RUN yum -y install libxml2-devel

ADD https://bootstrap.pypa.io/get-pip.py /
RUN python /get-pip.py
RUN pip install psutil lockfile paramiko setuptools epydoc
RUN rm /get-pip.py

# Misc
RUN yum -y install vim mc psmisc

# cmake 3.0
WORKDIR /root

ADD http://www.cmake.org/files/v3.0/cmake-3.0.0.tar.gz ./
RUN tar -zxvf cmake-3.0.0.tar.gz
WORKDIR cmake-3.0.0
RUN ./bootstrap
RUN make
RUN make install
ENV PATH=/usr/local/bin:$PATH

# compile GPDB

WORKDIR /workspace

ADD . gpdb/
ADD ./docker/dotfiles/root.bashrc /root/.bashrc
ADD ./docker/dotfiles/gpadmin.bashrc /home/gpadmin/.bashrc

WORKDIR gpdb

RUN ./configure --with-python --with-perl --enable-mapreduce --with-libxml --prefix=/usr/local/gpdb
RUN time make -j4
RUN make install

RUN ./docker/prep_for_ssh.bash

RUN chown -R gpadmin:gpadmin /home/gpadmin

RUN chmod u+s `which ping`
