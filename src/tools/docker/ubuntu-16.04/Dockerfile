# currently published as pivotaldata/pgadmin:gpdb-5

FROM ubuntu:16.04

EXPOSE 5432

# explicitly set user/group IDs
RUN groupadd -r postgres --gid=999 && useradd -m -r -g postgres --uid=999 postgres
RUN apt-get update && apt-get install -y dirmngr

# grab gosu for easy step-down from root
ENV GOSU_VERSION 1.7
RUN set -x \
  && apt-get update && apt-get install -y --no-install-recommends ca-certificates wget && rm -rf /var/lib/apt/lists/* \
  && wget -O /usr/local/bin/gosu "https://github.com/tianon/gosu/releases/download/$GOSU_VERSION/gosu-$(dpkg --print-architecture)" \
  && wget -O /usr/local/bin/gosu.asc "https://github.com/tianon/gosu/releases/download/$GOSU_VERSION/gosu-$(dpkg --print-architecture).asc" \
  && export GNUPGHOME="$(mktemp -d)" \
  && gpg --keyserver ha.pool.sks-keyservers.net --recv-keys B42F6819007F00F88E364FD4036A9C25BF357DD4 \
  && gpg --batch --verify /usr/local/bin/gosu.asc /usr/local/bin/gosu \
  && rm -r "$GNUPGHOME" /usr/local/bin/gosu.asc \
  && chmod +x /usr/local/bin/gosu \
  && gosu nobody true

RUN wget -qO- https://dl.yarnpkg.com/debian/pubkey.gpg | apt-key add -

RUN apt update
RUN apt install -y software-properties-common python-software-properties less
RUN add-apt-repository ppa:greenplum/db
RUN apt update
RUN apt install -y greenplum-db-oss
RUN . /opt/gpdb/greenplum_path.sh

RUN locale-gen en_US.utf8
RUN mkdir /gpdata
RUN mkdir /gpdata/gpdata1
RUN mkdir /gpdata/gpdata2
RUN mkdir /gpdata/gpmaster
RUN . /opt/gpdb/greenplum_path.sh && cp $GPHOME/docs/cli_help/gpconfigs/gpinitsystem_singlenode /gpdata/
RUN sed -i 's/gpdata1/gpdata\/gpdata1/g' /gpdata/gpinitsystem_singlenode
RUN sed -i 's/gpdata2/gpdata\/gpdata2/g' /gpdata/gpinitsystem_singlenode
RUN sed -i 's/gpmaster/gpdata\/gpmaster/g' /gpdata/gpinitsystem_singlenode
RUN apt install -y ssh

RUN useradd -md /home/gp/ gp
RUN chown gp -R /gpdata
RUN echo "source /opt/gpdb/greenplum_path.sh" > /home/gp/.bash_profile && chown gp:gp /home/gp/.bash_profile
RUN su - gp bash -c 'mkdir /home/gp/.ssh'

ADD install_and_start_gpdb.sh /home/gp/install_and_start_gpdb.sh
RUN chown gp:gp /home/gp/install_and_start_gpdb.sh
RUN chmod a+x /home/gp/install_and_start_gpdb.sh
RUN apt-get install -y sudo
RUN echo "gp ALL=(ALL) NOPASSWD: ALL" >> /etc/sudoers
RUN echo "root ALL=NOPASSWD: ALL" >> /etc/sudoers


CMD sudo su - gp bash -c /home/gp/install_and_start_gpdb.sh && tail -f /dev/null

