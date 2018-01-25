# Docker container with GPDB for development/testing

## Build locally
```
docker build -t pivotaldata/gpdb-dev:centos6 centos6
docker build -t pivotaldata/gpdb-dev:centos6-gpadmin centos6-gpadmin
docker build -t pivotaldata/gpdb-dev:centos7 centos7
docker build -t pivotaldata/gpdb-dev:centos7-gpadmin centos7-gpadmin
```

OR
## Download from docker hub
```
docker pull pivotaldata/gpdb-dev:centos6
docker pull pivotaldata/gpdb-dev:centos6-admin
docker pull pivotaldata/gpdb-dev:centos7
docker pull pivotaldata/gpdb-dev:centos7-admin
```


# Build GPDB code with Docker

### Clone GPDB repo
```
git clone https://github.com/greenplum-db/gpdb.git
cd gpdb
git checkout 5X_STABLE
```
### Use docker image based on gpdb/src/tools/docker/centos7-gpadmin
```
docker run -w /home/build/gpdb -v ${PWD}:/home/build/gpdb:cached -it pivotaldata/gpdb-dev:centos7-gpadmin /bin/bash
```

### Inside docker
(Total time to build and run ~ 15-20 min)
```
sudo /usr/sbin/sshd
make clean
./configure --enable-debug --with-perl --with-python --with-libxml --disable-orca --prefix=/usr/local/gpdb
make -j4
make install
source /usr/local/gpdb/greenplum_path.sh
make create-demo-cluster
source ./gpAux/gpdemo/gpdemo-env.sh
psql -d template1
```