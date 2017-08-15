# The PXF extension client for GPDB

At present, this extension is in development status, where only Demo PXF profile is working and no real data is yet accessible.

## Table of Contents

* Usage
* Initialize and start GPDB cluster
* Enable PXF extension
* Run unit tests
* Run regression tests
=======

## Usage

### Enable PXF extension in GPDB build process.

Configure GPDB to build the pxf extension by adding the `--with-pxf`
configure option. This is required to setup the PXF build environment.

### Build the PXF extension

```
make
```

The build will produce the pxf client shared library named `pxf.so`.
 
### Install the PXF extension
```
make install
```
 
This will copy the `pxf.so` shared library into `$GPHOME/lib/postgresql.`


To create the PXF extension in the database, connect to the database and run as a GPDB superuser in psql:
```
# CREATE EXTENSION pxf;
```

Additional instructions on building and starting a GPDB cluster can be
found in the top-level [README.md](../../../README.md) ("_Build the
database_" section).

## Create and use PXF external table
At this time, only PXF Demo profile is working:
```
# CREATE EXTERNAL TABLE pxf_read_test (a TEXT, b TEXT, c TEXT) \
LOCATION ('pxf://localhost:51200/tmp/dummy1' \
'?FRAGMENTER=org.apache.hawq.pxf.api.examples.DemoFragmenter' \
'&ACCESSOR=org.apache.hawq.pxf.api.examples.DemoAccessor' \
'&RESOLVER=org.apache.hawq.pxf.api.examples.DemoTextResolver') \
FORMAT 'TEXT' (DELIMITER ',');
```

Once you also install and run PXF server on the machines where GPDB segments are run, you can select data from the demo PXF profile:
```
# SELECT * from pxf_read_test order by a;

       a        |   b    |   c
----------------+--------+--------
 fragment1 row1 | value1 | value2
 fragment1 row2 | value1 | value2
 fragment2 row1 | value1 | value2
 fragment2 row2 | value1 | value2
 fragment3 row1 | value1 | value2
 fragment3 row2 | value1 | value2
(6 rows)
```

### Run unit tests

This will run the unit tests located in the `test` directory

```
make unittest-check
```

## Run regression tests

```
make installcheck
```

This will connect to the running database, and run the regression
tests located in the `regress` directory.
