The PXF extension client for GPDB
===================================

Table of Contents
=================

* Introduction
* Package Contents
* Building

Introduction
============

Package Contents
================

Usage
========

* Install and start GPDB cluster

```
cd ../../../gpAux/gpdemo
make
source ./gpdemo-env.sh
```

* Build PXF extension

Assuming you configured GPDB build with `--prefix=/usr/local/gpdb`
```
source /usr/local/gpdb/greenplum_path.sh
make
```
This will compile pxf client code into `pxf.so` shared library and run unit tests located in `test` directory.

* Install PXF extension
```
make install
```
This will copy the shared library `pxf.so` into `/usr/local/gpdb/lib/postgresql/`

* Run regression test
```
make installcheck
```
This will connect to the running database, and run `basic.sql` test which creates the functions, the protocol, external table and selects test records from the table.

* To run the test manaully
```
psql -d template1 -f ./sql/basic.sql
```

You can now define your external readable tables or writable tables.
