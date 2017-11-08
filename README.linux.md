## For CentOS:

- Install Dependencies

  ```bash
    ./README.CentOS.bash
  ```

- If you want to link cmake3 to cmake, run:

  ```bash
    ln -sf ../../bin/cmake3 /usr/local/bin/cmake
  ```

- Make sure that you add `/usr/local/lib` and `/usr/local/lib64` to
`/etc/ld.so.conf`, then run command `ldconfig`.

- If you want to install and use gcc-6 by default, run:

  ```bash
  sudo yum install -y centos-release-scl devtoolset-6-toolchain
  echo 'source scl_source enable devtoolset-6' >> ~/.bashrc
  ```

## For RHEL

Use dependency script for CentOS.

- If you want to install `devtoolset-6`:

  ```bash
  sudo yum-config-manager --enable rhui-REGION-rhel-server-rhscl
  sudo yum install -y devtoolset-6-toolchain
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

- If you want to use gcc-6 and g++-6:

  ```bash
  add-apt-repository ppa:ubuntu-toolchain-r/test -y
  apt-get update
  apt-get install -y gcc-6 g++-6
  ```

## Common Platform Tasks:

Make sure that you add `/usr/local/lib` to `/etc/ld.so.conf`,
then run command `ldconfig`.

1. ORCA requires [CMake](https://cmake.org) 3.x; make sure you have it installed.
   Installation instructions vary, please check the CMake website.

1. Create gpadmin and setup ssh keys
   Either use:

   ```bash
   # Requires gpdb clone to be named gpdb_src
   gpdb_src/concourse/scripts/setup_gpadmin_user.bash
   ```
   to create the gpadmin user and set up keys,

   OR

   manually create ssh keys so you can do ssh localhost without a password, e.g., using ssh-keygen
   then cp ~/.ssh/id_rsa.pub authorized_keys

1. Verify that you can ssh to your machine name without a password

   ```bash
    ssh <hostname of your machine>  # e.g., ssh briarwood
   ```

1. Set up your system configuration:

  ```bash

  cat >> /etc/sysctl.conf <<-EOF
  kernel.shmmax = 500000000
  kernel.shmmni = 4096
  kernel.shmall = 4000000000
  kernel.sem = 250 512000 100 2048
  kernel.sysrq = 1
  kernel.core_uses_pid = 1
  kernel.msgmnb = 65536
  kernel.msgmax = 65536
  kernel.msgmni = 2048
  net.ipv4.tcp_syncookies = 1
  net.ipv4.ip_forward = 0
  net.ipv4.conf.default.accept_source_route = 0
  net.ipv4.tcp_tw_recycle = 1
  net.ipv4.tcp_max_syn_backlog = 4096
  net.ipv4.conf.all.arp_filter = 1
  net.ipv4.ip_local_port_range = 1025 65535
  net.core.netdev_max_backlog = 10000
  net.core.rmem_max = 2097152
  net.core.wmem_max = 2097152
  vm.overcommit_memory = 2

  EOF

  cat >> /etc/security/limits.conf <<-EOF
  * soft nofile 65536
  * hard nofile 65536
  * soft nproc 131072
  * hard nproc 131072

  EOF

  cat >> /etc/ld.so.conf <<-EOF
  /usr/local/lib

  EOF

  ```
