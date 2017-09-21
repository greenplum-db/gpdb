### CentOS Build Setup Instructions

These setup instructions only apply to builds without Orca.

### Install Dependencies

```bash
./README.CentOS.bash
```

### Get an Updated GCC on CentOS 6

Install [devtoolset-6][devtoolset-6], which contains gcc 6.3.

[devtoolset-6]: https://www.softwarecollections.org/en/scls/rhscl/devtoolset-6/

First, enable the repository:

```bash
#CentOS 6:
sudo yum install centos-release-scl
#RHEL on AWS:
sudo yum-config-manager --enable rhui-REGION-rhel-server-rhscl
```

Then install devtoolset-6:

```bash
sudo yum install devtoolset-6-toolchain
```

Now running `scl enable devtoolset-6 bash` starts a bash session that puts gcc
6.3 in `$PATH`. To enable devtoolset-6 in all your login shells:

```bash
echo 'source scl_source enable devtoolset-6' >> ~/.bashrc
```
