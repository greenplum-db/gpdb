# The PXF extension client for GPDB

## Table of Contents

* Usage
 * Initialize and start GPDB cluster
 * Enable PXF extension
 * Run unit tests
 * Run regression tests

## Usage

### Enable PXF extension in GPDB build process.

Configure GPDB to build the pxf extension by adding the `--enable-pxf`
configure option. This is required to setup the PXF build environment.

The build will produce the pxf client `pxf.so` shared library. It
will be installed it into `$GPHOME/lib/postgres.`

Additional instructions on building and starting a GPDB cluster can be
found in the top-level [README.md](../../../README.md) ("_Build the
database_" section).

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

## Install PXF extension
Run as a GPDB superuser in psql:
```
# CREATE EXTENSION pxf;
```

## Create table which uses PXF
```
# CREATE EXTERNAL TABLE table_name 
( column_name data_type [, ...] )
LOCATION ('pxf://host[:port]/external_path?Profile=ProfileName')
FORMAT 'TEXT' (DELIMITER ',');
```