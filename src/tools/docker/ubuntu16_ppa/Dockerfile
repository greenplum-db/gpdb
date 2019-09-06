# currently published as pivotaldata/gpdb5:latest

FROM ubuntu:16.04

EXPOSE 5432

# explicitly set user/group IDs
RUN groupadd -r postgres --gid=999 && useradd -m -r -g postgres --uid=999 postgres

# Install initial dependencies
RUN apt-get update \
 && apt-get install -y dirmngr software-properties-common python-software-properties less ssh sudo time libzstd1-dev

# Install GreenPlum
RUN wget -qO- https://dl.yarnpkg.com/debian/pubkey.gpg | apt-key add - \
 && add-apt-repository ppa:greenplum/db \
 && apt update \
 && apt install -y greenplum-db-oss \
 && locale-gen en_US.utf8 

# Create GreenPlum data folders and copy/edit configuration to use a single node
RUN mkdir /data \
 && mkdir /data/data1 \
 && mkdir /data/data2 \
 && mkdir /data/master \
 && . /opt/gpdb/greenplum_path.sh && cp $GPHOME/docs/cli_help/gpconfigs/gpinitsystem_singlenode /data/ \
 && sed -i 's/gpdata1/data\/data1/g' /data/gpinitsystem_singlenode \
 && sed -i 's/gpdata2/data\/data2/g' /data/gpinitsystem_singlenode \
 && sed -i 's/gpmaster/data\/master/g' /data/gpinitsystem_singlenode

# Create gpadmin user and add the user to the sudoers
RUN useradd -md /home/gpadmin/ gpadmin \
 && chown gpadmin -R /data \
 && echo "source /opt/gpdb/greenplum_path.sh" > /home/gpadmin/.bash_profile && chown gpadmin:gpadmin /home/gpadmin/.bash_profile \
 && su - gpadmin bash -c 'mkdir /home/gpadmin/.ssh' \
 && echo "gpadmin ALL=(ALL) NOPASSWD: ALL" >> /etc/sudoers \
 && echo "root ALL=NOPASSWD: ALL" >> /etc/sudoers


## Add setup and start script to run when the container starts
ADD install_and_start_gpdb.sh /home/gpadmin/install_and_start_gpdb.sh
RUN chown gpadmin:gpadmin /home/gpadmin/install_and_start_gpdb.sh \
 && chmod a+x /home/gpadmin/install_and_start_gpdb.sh

CMD sudo su - gpadmin bash -c /home/gpadmin/install_and_start_gpdb.sh && tail -f /dev/null

