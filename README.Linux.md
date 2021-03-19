## For CentOS:

- Install Dependencies

  ```bash
    ./README.CentOS.bash
  ```

- If you want to link cmake3 to cmake, run:

  ```bash
    sudo ln -sf /usr/bin/cmake3 /usr/local/bin/cmake
  ```

- Make sure that you add `/usr/local/lib` and `/usr/local/lib64` to
`/etc/ld.so.conf`, then run command `ldconfig`.

- If you want to install and use gcc-7 by default, run:

  ```bash
  sudo yum install -y centos-release-scl
  sudo yum install -y devtoolset-7-toolchain
  echo 'source scl_source enable devtoolset-7' >> ~/.bashrc
  ```

## For RHEL

Use dependency script for CentOS.

- If you want to install `devtoolset-7`:

  ```bash
  sudo yum-config-manager --enable rhui-REGION-rhel-server-rhscl
  sudo yum install -y devtoolset-7-toolchain
  ```

## For Ubuntu:

- Install Dependencies
  When you run the README.ubuntu.bash script for dependencies, you will be asked to configure realm for kerberos.
  You can enter any realm, since this is just for testing, and during testing, it will reconfigure a local server/client.
  If you want to skip this manual configuration, use:
  `export DEBIAN_FRONTEND=noninteractive`

  ```bash
    ./README.ubuntu.bash
  ```

- Ubuntu 18.04 and newer should have use gcc 7 or newer, but you can also enable gcc-7 on older versions of Ubuntu:

  ```bash
  add-apt-repository ppa:ubuntu-toolchain-r/test -y
  apt-get update
  apt-get install -y gcc-7 g++-7
  ```

## Common Platform Tasks:

Make sure that you add `/usr/local/lib` to `/etc/ld.so.conf`,
then run command `ldconfig`.
1. Create gpadmin and setup ssh keys
   Either use:

   ```bash
   # Requires gpdb clone to be named gpdb_src
   gpdb_src/concourse/scripts/setup_gpadmin_user.bash
   ```
   to create the gpadmin user and set up keys,

   OR

   manually create ssh keys so you can do ssh localhost without a password, e.g., 
   
   ```
   ssh-keygen
   cat ~/.ssh/id_rsa.pub >> ~/.ssh/authorized_keys
   chmod 600 ~/.ssh/authorized_keys
   ```

1. Verify that you can ssh to your machine name without a password

   ```bash
    ssh <hostname of your machine>  # e.g., ssh briarwood
   ```

1. Set up your system configuration by following the installation guide on [docs.greenplum.org](https://docs.greenplum.org)


