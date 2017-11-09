# Debian Packaging

This documentation assumes that you run the following commands on an Ubuntu platform and has been tested on Xenial 16.04
This debian binary install will only copy the necessary files but does not initialize the cluster. To create and test 
one you can use commands like those found in the script used in the build pipeline at 
`gpdb/concourse/scripts/deb_init_cluster.bash` 

## Requirements

```bash
apt-get install -y software-properties-common \
                   debmake \
                   equivs
                   
add-apt-repository -y ppa:ubuntu-toolchain-r/test
apt-get update
```

## Create sample changelog file

For now, there is no changelog file that contains history of previous deb releases.
However, we can create one using the following command.

```bash
pushd ~/workspace/gpdb
    dch --create -M --package greenplum-db-oss -v 6.0.0-alpha.0  "First Release"
popd
```

## Download build dependencies

In order to create a debian binary, we need all the build dependencies downloaded prior to building it.
All build dependencies are listed in `gpdb/debian/control` file.

```bash
pushd ~/workspace/gpdb
    yes | mk-build-deps -i debian/control
popd
```
## How to create debian package

Use dpkg utility to create greenplum debian binary

```bash
pushd ~/workspace/gpdb
    dpkg-buildpackage -uc -us -b
popd
```
