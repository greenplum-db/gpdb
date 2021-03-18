# Setting up a Docker Development Environment

This guide shows how to use native Docker for Mac to compile GPDB5 with your Mac’s folders mounted into a CentOS container. Although compiling GPDB5 on Mac is supported, Linux is required for some GPDB5 features like resource groups.

These instructions should be applicable to any Docker host OS. We have seen it work on Mac OS X El Capitan and Ubuntu.

## Installing Docker for Mac OS X

Follow instructions below and select Docker for Mac for native support. You must have OS X 10.10.3 Yosemite or newer.
https://docs.docker.com/engine/installation/mac/

After installing, in Docker preferences, you may want to increase the CPUs/memory allocated to Docker.


## Pull and Run the Docker Image

Note that the -v switch is used to mount the OS X’s gpdb folder into the container. The privileged and seccomp flags are used to allow gdb to work in Docker.

To make Greenplum accessible outside of the docker container use the -p switch (add before the -i switch) in the run command with the following: `127.0.0.1:<port_on_local_machine>:<greenplum_port_in_docker>`

This will proxy connections / requests made to the `<port_on_local_machine>` across to `<greenplum_port_in_docker>` which will allow Greenplum within docker to receive them.

```bash
docker run -t -v ~/workspace/gpdb:/home/gpadmin/gpdb_src --privileged --security-opt seccomp:unconfined -i pivotaldata/gpdb6-centos7-build:latest bash
```

## Set up Container Dependencies and the gpadmin User

***The following steps run inside the container***

```bash
# Set up gpadmin user and SSH
cd /home/gpadmin
gpdb_src/concourse/scripts/setup_gpadmin_user.bash
echo "/usr/sbin/sshd" >> /root/.bashrc

su - gpadmin
pip install --user psutil
cat >> ~/.bash_profile <<EOF
export PS1='\n\w\n$ '
source /opt/gcc_env.sh
export CC=\`which gcc\`
export CXX=\`which g++\`
EOF
source ~/.bash_profile
```

## Set up rsync (optional)

Compiling and running GPDB from the mounted OSX `gpdb_src` folder is quite slow and sometimes buggy due to the constant handshake between OSX and the Docker container. To get around this issue, we can use rsync to sync the files from `~/gpdb_src` to a local `~/gpdb` folder.

```
rsync -az --exclude=.git --exclude=gpAux/client/ --exclude=gpAux/docs/ /home/gpadmin/gpdb_src/ /home/gpadmin/gpdb/
```

To make the resync a little faster after the initial sync, you can specify to sync only the `gpdb_src/src` directory if you only make modifications to that particular directory.

Another helpful trick is to make it so that running make in the `gpdb_src/` folder will run the rsync command either by using an alias for make as below or modifying the gpdb Makefile.

```bash
alias make="rsync [...] ; /bin/make -j8"
```

## Configure cgroups for GPDB (optional)

Follow the instructions in the “Prerequisites” section from below:
https://gpdb.docs.pivotal.io/500/admin_guide/workload_mgmt_resgroups.html

And run the below command:

```bash
echo "cgconfigparser -l /etc/cgconfig.d/gpdb.conf" >> /root/.bashrc
```

## Configure the GPDB Build Environment
Run the `./configure` command as described in [README.md](README.md).

NOTE: make sure that the gpdb folder on the Mac is clean, so that no object files get mounted into Docker! This can be done with `make distclean` or `git clean -xdff` (the latter will delete *all* untracked files, so be careful).

```bash
su - gpadmin
cd gpdb_src #or gpdb if you are using rsync
./configure --with-perl --with-python --with-libxml --disable-orca --prefix=/home/gpadmin/gpdb_install
```

## Commit the Container (optional)

Exit the docker container and run the below from the host machine:

```bash
docker ps -a
#copy the container name and rename to gpdb (any name is okay)
docker rename <container name> gpdb
#this will save base image if you ever mess up later on
docker commit gpdb gpdb-image:base
docker start gpdb
docker attach gpdb
```

## Creating Multiple Login Sessions to the Container

Note that you can run `docker exec -i -t [container name] /bin/bash` to create additional login shells where `[container name]` can be found with `docker ps`.

## Compile and Install GPDB

```bash
su - gpadmin
cd ~/gpdb_src
make -j8 all install
```

## Starting a gpdemo Cluster

Follow the below steps. If you are unable to psql in, try increasing your CPU and memory settings for docker to the max or disabling mirror segments in gpdemo's `clusterConfigFile`.

```bash
su - gpadmin
rm -rf /tmp/.s*  #kill leftover lock files in case docker shutdown ungracefully
cd ~/gpdb_src/gpAux/gpdemo
source ~/gpdb_install/greenplum_path.sh
make cluster
source gpdemo-env.sh
psql postgres
```

Assuming that `gpdemo-env.sh` has been sourced, modify the `pg_hba.conf` file located in `$MASTER_DATA_DIRECTORY` to include the following:

`host all gpadmin 0.0.0.0 trust`

This instructs Greenplum within docker to trust all connections from the local machine, like that from an SQL editor.

Then run `gpstop -au` to reload the Greenplum cluster's config so that the changes in `pg_hba.conf` take effect.

### Running Regression Tests

Note that the installcheck-world make target has at least 43 out of 401 failures, some of which are non-deterministic.

```bash
su - gpadmin
cd ~/gpdb_src
make installcheck-world
```

