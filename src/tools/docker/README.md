# Docker container with GPDB for development/testing


## Build locally
```
# Centos 6 (include dependencies for building GPDB)
docker build -t local/gpdb-dev:centos6 centos6
# Add gpadmin user to Centos 6 (gpdb cannot run as root)
docker build --build-arg REPO_OWNER=local -t local/gpdb-dev:centos6-gpadmin centos6-gpadmin

# Centos 7 (include dependencies for building GPDB)
docker build -t local/gpdb-dev:centos7 centos7
# Add gpadmin user to Centos 7 (gpdb cannot run as root)
docker build --build-arg REPO_OWNER=local -t local/gpdb-dev:centos7-gpadmin centos7-gpadmin
```

OR
## Download from docker hub
```
docker pull pivotaldata/gpdb-dev:centos6-admin
docker pull pivotaldata/gpdb-dev:centos7-admin
```

# Build GPDB code with Docker

### Clone GPDB repo
```
git clone https://github.com/greenplum-db/gpdb.git
cd gpdb
```
### Use docker image based on gpdb/src/tools/docker/centos7-gpadmin
```
docker run -w /home/build/gpdb -v ${PWD}:/home/build/gpdb:cached -it pivotaldata/gpdb-dev:centos7-gpadmin /bin/bash
```

### Inside docker
(Total time to build and run ~ 15-20 min)
```
# ORCA is disabled here to keep the instructions simple
./configure --enable-debug --with-perl --with-python --with-libxml --disable-orca --prefix=/usr/local/gpdb
make -j4

# Install Greenplum binaries (to /usr/local/gpdb)
make install

# Create a single node demo cluster with three segments
# PGPORT is set to 15432
source /usr/local/gpdb/greenplum_path.sh
make create-demo-cluster
source ./gpAux/gpdemo/gpdemo-env.sh

# Create and use a test database
createdb greenplum
psql -d greenplum
```