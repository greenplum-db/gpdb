## Travis [![Travis Build Status](https://travis-ci.org/greenplum-db/gpdb.svg?branch=master)](https://travis-ci.org/greenplum-db/gpdb)

## Concourse [![Concourse Build Status](https://gpdb.ci.pivotalci.info/api/v1/teams/gpdb/pipelines/gpdb_master/jobs/gpdb_rc_packaging_centos/badge)](https://gpdb.ci.pivotalci.info/teams/gpdb)

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
  manual is maintained separately and only the reference pages used
  to build man pages are here.

* __gpdb-doc/__

  Constains the Greenplum documentation in DITA XML format. Refer to
  `gpdb-doc/README.md` for information on how to build, and work with
  the documentation.

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

## Building GPDB

Some configure options are nominally optional, but required to pass
all regression tests. The minimum set of options for running the
regression tests successfully is:

`./configure --with-perl --with-python --with-libxml --enable-mapreduce`

### Build GPDB with Planner

```
# Clean environment
make distclean

# Configure build environment to install at /usr/local/gpdb
./configure --with-perl --with-python --with-libxml --enable-mapreduce --prefix=/usr/local/gpdb

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

Compilation can be sped up with parallelization. Instead of `make`, consider:

```
make -j8
```

The directory and the TCP ports for the demo cluster can be changed on the fly.
Instead of `make cluster`, consider:

```
DATADIRS=/tmp/gpdb-cluster MASTER_PORT=15432 PORT_BASE=25432 make cluster
```

The TCP port for the regression test can be changed on the fly:

```
PGPORT=15432 make installcheck-good
```


### Build GPDB with GPORCA
You must first install the below libraries in the below order (see the READMEs on each repository):

1. https://github.com/greenplum-db/gp-xerces
2. https://github.com/greenplum-db/gporca

Next, change your `configure` command to have the additional option `--enable-orca`.
```
# Configure build environment to install at /usr/local/gpdb
# Enable GPORCA
# Build with perl module (PL/Perl)
# Build with python module (PL/Python)
# Build with XML support
./configure --with-perl --with-python --with-libxml --enable-mapreduce --enable-orca --prefix=/usr/local/gpdb
```

Once build and started, run `psql` and check the GPOPT (e.g. GPORCA) version:

```
select gp_opt_version();
```

### Build GPDB with code generation enabled

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
./configure --with-perl --with-python --with-libxml ---enable-mapreduce --enable-codegen --prefix=/usr/local/gpdb --with-codegen-prefix="/path/to/llvm;/path/to/clang"
```

## Regression tests

* The default regression tests

```
make installcheck-good
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

* When adding a new test, please add it to one of the GPDB-specific tests,
  in greenplum_schedule, rather than the PostgreSQL tests inherited from the
  upstream. We try to keep the upstream tests identical to the upstream
  versions, to make merging with newer PostgreSQL releases easier.

## Development with Docker

We provide a docker image with all dependencies required to compile and test
GPDB. You can view the dependency dockerfile at `./src/tools/docker/base/Dockerfile`.
The image is hosted on docker hub at `pivotaldata/gpdb-devel`. This docker
image is currently under heavy development.

A quickstart guide to Docker can be found on the [Pivotal Engineering Journal](http://engineering.pivotal.io/post/docker-gpdb/).

Known issues:
* The `installcheck-good` make target has at least 4 failures, some of which
  are non-deterministic

### Running regression tests with Docker

1. Create a docker host with 8gb RAM and 4 cores
    ```bash
    docker-machine create -d virtualbox --virtualbox-cpu-count 4 --virtualbox-disk-size 50000 --virtualbox-memory 8192 gpdb
    eval $(docker-machine env gpdb)
    ```

1. Build your code on gpdb-devel rootfs
    ```bash
    cd [path/to/gpdb]
    docker build .
    # image beefc4f3 built
    ```
    The top level Dockerfile will automatically sync your current working
    directory into the docker image. This means that any code you are working
    on will automatically be built and ready for testing in the docker context

1. Log into docker image
    ```bash
    docker run -it beefc4f3
    ```

1. As `gpadmin` user run `installcheck-good`
    ```bash
    su gpadmin
    cd /workspace/gpdb
    make installcheck-good
    ```

### Caveats

* No Space Left On Device
    On macOS the docker-machine vm can periodically become full with unused images.
    You can clear these images with a combination of docker commands.
    ```bash
    # assuming no currently running containers
    # remove all stopped containers from cache
    docker ps -aq | xargs -n 1 docker rm
    # remove all untagged images
    docker images -aq --filter dangling=true | xargs -n 1 docker rmi
    ```

* The Native macOS docker client available with docker 1.12+ (beta) or
  Community Edition 17+ may also work

## Contributing

Greenplum is maintained by a core team of developers with commit rights to the
[main gpdb repository](https://github.com/greenplum-db/gpdb) on GitHub. At the
same time, we are very eager to receive contributions from anybody in the wider
Greenplum community. This section covers all you need to know if you want to see
your code or documentation changes be added to Greenplum and appear in the
future releases.

### Getting started

Greenplum is developed on GitHub, and anybody wishing to contribute to it will
have to [have a GitHub account](https://github.com/signup/free) and be familiar
with [Git tools and workflow](https://wiki.postgresql.org/wiki/Working_with_Git). 
It is also recommend that you follow the [developer's mailing list](http://greenplum.org/#contribute)
since some of the contributions may generate more detailed discussions there.

Once you have your GitHub account, [fork](https://github.com/greenplum-db/gpdb/fork)
repository so that you can have your private copy to start hacking on and to
use as source of pull requests.

Anybody contributing to Greenplum has to be covered by either the Corporate or
the Individual Contributor License Agreement. If you have not previously done
so, please fill out and submit the [Contributor License Agreement](https://cla.pivotal.io/sign/greenplum).
Note that we do allow for really trivial changes to be contributed without a
CLA if they fall under the rubric of [obvious fixes](https://cla.pivotal.io/about#obvious-fixes).
However, since our GitHub workflow checks for CLA by default you may find it
easier to submit one instead of claiming an "obvious fix" exception.

### Coding guidelines

Your chances of getting feedback and seeing your code merged into the project
greatly depend on how granular your changes are. If you happen to have a bigger
change in mind, we highly recommend engaging on the developer's mailing list
first and sharing your proposal with us before you spend a lot of time writing
code. Even when your proposal gets validated by the community, we still recommend
doing the actual work as a series of small, self-contained commits. This makes
the reviewer's job much easier and increases the timeliness of feedback.

When it comes to C and C++ parts of Greenplum, we try to follow 
[PostgreSQL Coding Conventions](https://www.postgresql.org/docs/devel/static/source.html).
In addition to that we require that:
   * All Python code passes [Pylint](https://www.pylint.org/)
   * All Go code is formatted according to [gofmt](https://golang.org/cmd/gofmt/)

We recommend using ```git diff --color``` when reviewing your changes so that you
don't have any spurious whitespace issues in the code that you submit.

All new functionality that is contributed to Greenplum should be covered by regression
tests that are contributed alongside it. If you are uncertain on how to test, or document 
your work, please raise the question on the gpdb-dev mailinglist and the developer 
community will do its best to help you.

### Testing guidelines

At the very minimum you should always be running 
```make installcheck-world```
to make sure that you're not breaking anything.

### Changes applicable to upstream PostgreSQL

If the change you're working on touches functionality that is common between PostgreSQL
and Greenplum, you may be asked to forward-port it to PostgreSQL. This is not only so
that we keep reducing the delta between the two projects, but also so that any change
that is relevant to PostgreSQL can benefit from a much broader review of the upstream
PostgreSQL community. In general, it is a good idea to keep both code bases handy so
you can be sure whether your changes may need to be forward-ported.

### Submission timing

To improve the odds of the right discussion of your patch or idea happening, pay attention 
to what the community work cycle is. For example, if you send in a brand new idea in the 
beta phase, don't be surprised if no one is paying attention because we are focused on 
release work. Come back when the beta is done, please!

You can read more on Greenplum release policy and timing in the RELEASE.md

### Patch submission

Once you are ready to share your work with the Greenplum core team and the rest of
the Greenplum community, you should push all the commits to a branch in your own 
repository forked from the official Greenplum and [send us a pull request](https://help.github.com/articles/about-pull-requests/).

For now, we require all pull requests to be submitted against the main master
branch, but over time, once there are many supported open source releases of Greenplum
in the wild, you may decide to submit your pull requests against an active
release branch if the change is only applicable to a given release.

### Validation checks and CI

Once you submit your pull request, you will immediately see a number of validation
checks performed by our automated CI pipelines. There also will be a CLA check
telling you whether your CLA was recognized. If any of these checks fails, you
will need to update your pull request to take care of the issue. Pull requests
with failed validation checks are very unlikely to receive any further peer
review from the community members.

Keep in mind that the most common reason for a failed CLA check is a mismatch
between an email on file and an email recorded in the commits submitted as
part of the pull request.

If you can not figure out why a certain validation check failed, feel free to
ask on the developer's mailing list, but make sure to include a direct link
to a pull request in your email.

### Patch review

A submitted pull request with passing validation checks is assumed to be available 
for peer review. Peer review is the process that ensures that contributions to Greenplum 
are of high quality and align well with the road map and community expectations. Every
member of the Greenplum community is encouraged to review pull requests and provide
feedback. Since you don't have to be a core team member to be able to do that, we
recommend following a stream of pull reviews to anybody who's interested in becoming
a long-term contributor to Greenplum. As [Linus would say](https://en.wikipedia.org/wiki/Linus's_Law) 
"given enough eyeballs, all bugs are shallow".

One outcome of the peer review could be a consensus that you need to modify your
pull request in certain ways. GitHub allows you to push additional commits into
a branch from which a pull request was sent. Those additional commits will be then
visible to all of the reviewers.

A peer review converges when it receives at least one +1 and no -1s votes from
the participants. At that point you should expect one of the core team 
members to pull your changes into the project.

Greenplum prides itself on being a collaborative, consensus-driven environment. 
We do not believe in vetoes and any -1 vote casted as part of the peer review 
has to have a detailed technical explanation of what's wrong with the change.
Should a strong disagreement arise it may be advisable to take the matter onto
the mailing list since it allows for a more natural flow of the conversation.

At any time during the patch review, you may experience delays based on the
availability of reviewers and core team members. Please be patient. That being
said, don't get discouraged either. If you're not getting expected feedback for
a few days add a comment asking for updates on the pull request itself or send
an email to the mailing list.

### Direct commits to the repository

On occasion you will see core team members committing directly to the repository
without going through the pull request workflow. This is reserved for small changes
only and the rule of thumb we use is this: if the change touches any functionality
that may result in a test failure, then it has to go through a pull request workflow.
If, on the other hand, the change is in the non-functional part of the code base
(such as fixing a typo inside of a comment block) core team members can decide to
just commit to the repository directly.

## Glossary

* __QD__

  Query Dispatcher. A synonym for the master server.

* __QE__

  Query Executor. A synonym for a segment server.

## Documentation

For Greenplum Database documentation, please check online docs:
http://greenplum.org/docs/

For further information beyond the scope of this README, please see
[our wiki](https://github.com/greenplum-db/gpdb/wiki)

There is also a Vagrant-based quickstart guide for developers in `src/tools/vagrant/README.md`.
