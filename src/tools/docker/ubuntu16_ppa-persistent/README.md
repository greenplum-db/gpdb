# Docker container of Greenplum DB based on Ubuntu 16

This container raises the Greenplum database in "Single Instance" mode (master and two segments in one docker container). Use docker `-v` to mount host data directory to the container `/data` directory (as in example below).

This dockerfile is using PPA for Ubuntu 16 — https://launchpad.net/~greenplum/+archive/ubuntu/db

## Build and run
```
$ docker build -t local/gpdb .
$ mkdir -p /tmp/gpdata/
$ docker run -d -p 5432:5432 -h dwgpdb -v /tmp/gpdata:/data local/gpdb
```
The host name `dwgpdb` is strongly required to initializing the database properly with the same data from `/data` directory after restarting or rebuilding the container.

## The first connect to GPDB from the host
Review the docker logs to check that GPDB is installed and started up successfully:
```
$ docker ps -a
$ docker logs --follow <GPDB_CONTAINDER_ID>
```

Connect:
```
# apt-get install postgresql-client-9.5
$ psql -h 127.0.0.1 -p 5432 -U gpadmin
Password: gppassword
gpadmin=# select version();
```

## Using gp-utils in container
File `greenplum_path.sh` added to `.bashrc` of `gpmaster` user so you can use gp-utils immediately in container:
```
$ docker ps
$ docker exec -it <GPDB_CONTAINER_ID> bash
# su gpadmin
$ gpstate
```

## Stop database
```
$ docker ps
$ docker stop <GPDB_CONTAINER_ID>
```
