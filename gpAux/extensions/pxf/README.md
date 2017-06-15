# The PXF extension client for GPDB

## Table of Contents

* Usage
 * Initialize and start GPDB cluster
 * Enable PXF extension
 * Run unit tests
 * Run regression tests

## Usage

### Initialize and start GPDB cluster

```
cd ../../gpAux/gpdemo
make
source ./gpdemo-env.sh
```

### Enable PXF extension

Configure GPDB to build the pxf extension by adding the "--enable-pxf"
configure option. This is required to setup the PXF build environment.
Here is an example:

```
cd ../../../
configure --enable-pxf <plus other options of your choice>
make
make install
```

In addition to building GPDB, this will compile the pxf client code
into the `pxf.so` shared library and install it into
`$GPHOME/lib/postgres.`

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
