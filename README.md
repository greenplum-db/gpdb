[![Build Status](https://travis-ci.org/greenplum-db/gpdb.svg?branch=master)](https://travis-ci.org/greenplum-db/gpdb)

----------------------------------------------------------------------

![Greenplum](/gpAux/releng/images/logo-greenplum.png)

The Greenplum Database (GPDB) is an advanced, fully featured, open
source data warehouse. It provides powerful and rapid analytics on
petabyte scale data volumes. Uniquely geared toward big data
analytics, Greenplum Database is powered by the world’s most advanced
cost-based query optimizer delivering high analytical query
performance on large data volumes.

The Greenplum project is released under the [Apache 2
license](http://www.apache.org/licenses/LICENSE-2.0). We want to thank
all our current community contributors and are really interested in
all new potential contributions. For the Greenplum Database community
no contribution is too small, we encourage all types of contributions.

## Overview

A Greenplum cluster consists of a __master__ server, and multiple
__segment__ servers. All user data resides in the segments, the master
contains only metadata. The master server, and all the segments, share
the same schema.

Users always connect to the master server, which divides up the query
into fragments that are executed in the segments, sends the fragments
to the segments, and collects the results.

## Requirements

* From the GPDB doc set, review [Configuring Your Systems and
  Installing
  Greenplum](http://gpdb.docs.pivotal.io/4360/prep_os-overview.html#topic1)
  and perform appropriate updates to your system for GPDB use.

* **gpMgmt** utilities - command line tools for managing the cluster.

  You will need to add the following Python modules (2.7 & 2.6 are
  supported) into your installation

  * psutil
  * lockfile (>= 0.9.1)
  * paramiko
  * setuptools
  * epydoc

  If necessary, upgrade modules using "pip install --upgrade".
  pip should be at least version 7.x.x.


## Code layout

The directory layout of the repository follows the same general layout
as upstream PostgreSQL. There are changes compared to PostgreSQL
throughout the codebase, but a few larger additions worth noting:

* __gpMgmt/__

  Contains Greenplum-specific command-line tools for managing the
  cluster. Scripts like gpinit, gpstart, gpstop live here. They are
  mostly written in Python.

* __gpAux/__

  Contains Greenplum-specific extensions such as gpfdist and
  gpmapreduce.  Some additional directories are submodules and will be
  made available over time.

* __doc/__

  In PostgreSQL, the user manual lives here. In Greenplum, the user
  manual is distributed separately (see http://gpdb.docs.pivotal.io),
  and only the reference pages used to build man pages are here.

* __ci/__

  Contains configuration files for the GPDB continuous integration system.
 

* __src/backend/cdb/__

  Contains larger Greenplum-specific backend modules. For example,
  communication between segments, turning plans into parallelizable
  plans, mirroring, distributed transaction and snapshot management,
  etc. __cdb__ stands for __Cluster Database__ - it was a workname used in
  the early days. That name is no longer used, but the __cdb__ prefix
  remains.

* __src/backend/gpopt/__

  Contains the so-called __translator__ library, for using the ORCA
  optimizer with Greenplum. The translator library is written in C++
  code, and contains glue code for translating plans and queries
  between the DXL format used by ORCA, and the PostgreSQL internal
  representation. This goes unused, unless building with
  _--enable-orca_.

* __src/backend/gp_libpq_fe/__

  A slightly modified copy of libpq. The master node uses this to
  connect to segments, and to send fragments of a query plan to
  segments for execution. It is linked directly into the backend, it
  is not a shared library like libpq.

* __src/backend/fts/__

  FTS is a process that runs in the master node, and periodically
  polls the segments to maintain the status of each segment.

## Build GPDB with Planner

```
# Clean environment
make distclean

# Configure build environment to install at /usr/local/gpdb
./configure --prefix=/usr/local/gpdb

# Compile and install
make
make install

# Bring in greenplum environment into your running shell
source /usr/local/gpdb/greenplum_path.sh

# Start demo cluster (gpdemo-env.sh is created which contain
# __PGPORT__ and __MASTER_DATA_DIRECTORY__ values)
cd gpAux/gpdemo
make cluster
source gpdemo-env.sh
```

The directory and the TCP ports for the demo cluster can be changed on the fly:

```
DATADIRS=/tmp/gpdb-cluster MASTER_PORT=15432 PORT_BASE=25432 make cluster
```

The TCP port for the regression test can be changed on the fly:

```
PGPORT=15432 make installcheck-good
```


## Build GPDB with GPORCA

To compile GPDB with the GPORCA query planner, the `configure` script needs to
be invoked with an additional option: `--enable-orca`. If GPORCA was built from
source together with the modified version of the Xerces-C library required by
GPORCA, or you have GPXerces-C installed in a non-standard directory, the
install location of the library need to be configured with `--with-xerces=DIR`.
```
# Configure build environment to install at /usr/local/gpdb
# Enable GPORCA
# Build with perl module (PL/Perl)
# Build with python module (PL/Python)
# Build with XML support
./configure --enable-orca --with-perl --with-python --with-libxml --prefix=/usr/local/gpdb
```

Once build and started, run `psql` and check the GPOPT (e.g. GPORCA) version:

```
select gp_opt_version();
```
To enable GPORCA for queries, set the optimizer GUC to on:

```
set optimizer = on;
```

## Build GPDB with code generation enabled

To build GPDB with code generation (codegen) enabled, you will need cmake 2.8 or higher
and a recent version of llvm and clang (include headers and developer libraries). Codegen utils
is currently developed against the LLVM 3.7.X release series. You can find more details about the codegen feature,
including details about obtaining the prerequisites, building and testing GPDB with codegen in the [Codegen README](src/backend/codegen).

In short, you can change the `configure` with additional option
`--enable-codegen`, optionally giving the path to llvm and clang libraries on
your system.
```
# Configure build environment to install at /usr/local/gpdb
# Enable CODEGEN
./configure --enable-codegen --prefix=/usr/local/gpdb --with-codegen-prefix="/path/to/llvm;/path/to/clang"
```

## Regression tests

* The default regression tests

```
make installcheck-good
```

* optional extra/heavier regression tests

```
make installcheck-bugbuster
```

* The PostgreSQL __check__ target does not work. Setting up a
  Greenplum cluster is more complicated than a single-node PostgreSQL
  installation, and no-one's done the work to have __make check__
  create a cluster. Create a cluster manually or use gpAux/gpdemo/
  (example below) and run __make installcheck-good__ against
  that. Patches are welcome!

* The PostgreSQL __installcheck__ target does not work either, because
  some tests are known to fail with Greenplum. The
  __installcheck-good__ schedule excludes those tests.

## Glossary

* __QD__

  Query Dispatcher. A synonym for the master server.

* __QE__

  Query Executor. A synonym for a segment server.

## Documentation

For Greenplum Database documentation, please check online docs:
http://gpdb.docs.pivotal.io

There is also a Vagrant-based quickstart guide for developers in `vagrant/README.md`.
