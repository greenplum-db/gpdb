---
title: Installing the Greenplum Database Software 
---

Describes how to install the Greenplum Database software binaries on all of the hosts that will comprise your Greenplum Database system, how to enable passwordless SSH for the `gpadmin` user, and how to verify the installation.

Perform the following tasks in order:

1.  [Install Greenplum Database](#topic_oy5_21n_1jb):
    - [Download the Greenplum Database Server Software](#topic_download) (VMware Greenplum)
    - [Verify the Greenplum Database Software](#topic_verify_sha) (VMware Greenplum)
    - [Install the Greenplum Database Software](#topic_install) 
    - [(Optional) Install to a Non-Default Directory](#topic_dj4_ssr_cmb)
2.  [Enabling Passwordless SSH](#topic_xmb_gb5_vhb)
3.  [Confirm the software installation.](#topic10)
4.  [Next Steps](#topic_cwj_hzb_vhb)

**Parent topic:** [Installing and Upgrading Greenplum](install_guide.html)

## <a id="topic_oy5_21n_1jb"></a>Installing Greenplum Database 

You must install Greenplum Database on each host machine of the Greenplum Database system. 

VMware distributes the Greenplum Database software as a downloadable package that you install on each host system with the operating system's package management system. 

Open source Greenplum Database releases are available as: source code tarballs or RPM installers for RHEL/Oracle/Rocky Linux. See [https://greenplum.org/download/](https://greenplum.org/download/) for links to source code and instructions to compile Greenplum Database from source, and for links to download pre-built binaries in RPM format.

### <a id="topic_download"></a>Downloading the Greenplum Database Server Software (VMware Greenplum)

You can download the *Greenplum Database Server* software package from [VMware Tanzu Network](https://network.pivotal.io/products/pivotal-gpdb).

Be sure to note the name and the file system location of the downloaded file.

### <a id="topic_verify_sha"></a>Verifying the Greenplum Database Software (VMware Greenplum)

VMware generates a SHA256 fingerprint for each Greenplum Database software download available from Tanzu Network. This fingerprint enables you to verify that your downloaded file is unaltered from the original.

Follow the instructions in [Verifying the VMware Greenplum Software Download](verify_sw.html) to verify the integrity of the *Greenplum Database Server* software.

### <a id="topic_install"></a>Installing the Greenplum Database Software

Before you begin installing Greenplum Database, be sure you have completed the steps in [Configuring Your Systems](prep_os.html) to configure each of the coordinator, standby coordinator, and segment host machines for Greenplum Database.

> **Important** After installing Greenplum Database, you must set Greenplum Database environment variables. See [Setting Greenplum Environment Variables](init_gpdb.html).

See [Example Ansible Playbook](ansible-example.html) for an example script that shows how you can automate creating the `gpadmin` user and installing the Greenplum Database.

Follow these instructions to install Greenplum Database from a pre-built binary.

> **Important** You require sudo or root user access to install from a pre-built RPM or DEB file.

1.  Download and copy the Greenplum Database package to the `gpadmin` user's home directory on the coordinator, standby coordinator, and every segment host machine. The distribution file name has the format `greenplum-db-<version>-<platform>.rpm` for RHEL, Oracle Linux, or Rocky Linux systems.

    > **Note** For Oracle Linux or Rocky Linux installations, download and install the `rhel8-x86_64`distribution files.

2.  With sudo \(or as root\), install the Greenplum Database package on each host machine using your system's package manager software:
    ```
    $ sudo yum install ./greenplum-db-<version>-<platform>.rpm
    ```

    The `yum` command automatically installs software dependencies, copies the Greenplum Database software files into a version-specific directory under `/usr/local`, `/usr/local/greenplum-db-<version>`, and creates the symbolic link `/usr/local/greenplum-db` to the installation directory.

3.  Change the owner and group of the installed files to `gpadmin`:

    ```
    $ sudo chown -R gpadmin:gpadmin /usr/local/greenplum*
    $ sudo chgrp -R gpadmin /usr/local/greenplum*
    ```


## <a id="topic_dj4_ssr_cmb"></a>\(Optional\) Installing to a Non-Default Directory 

On RHEL systems, you can use the `rpm` command with the `--prefix` option to install Greenplum Database to a non-default directory \(instead of under `/usr/local`\). Note, however, that using `rpm` does not automatically install Greenplum Database dependencies; you must manually install dependencies to each host system.

Follow these instructions to install Greenplum Database to a specific directory.

> **Important** You require sudo or root user access to install from a pre-built RPM file.

1.  Download and copy the Greenplum Database package to the `gpadmin` user's home directory on the coordinator, standby coordinator, and every segment host machine. The distribution file name has the format `greenplum-db-<version>-rhel8-x86_64.rpm` for RHEL/Oracle/Rocky Linux systems.
2.  Manually install the Greenplum Database dependencies to each host system:

    ```
    $ sudo yum install apr apr-util bash bzip2 curl krb5 libcurl libevent \
    libxml2 libyaml zlib openldap openssh openssl openssl-libs perl readline rsync R sed tar zip
    ```

3.  Use `rpm` with the `--prefix` option to install the Greenplum Database package to your chosen installation directory on each host machine:

    ```
    $ sudo rpm --install ./greenplum-db-<version>-<platform>.rpm --prefix=<directory>
    ```

    The `rpm` command copies the Greenplum Database software files into a version-specific directory under your chosen `<directory>`, `<directory>/greenplum-db-<version>`, and creates the symbolic link `<directory>/greenplum-db` to the versioned directory.

4.  Change the owner and group of the installed files to `gpadmin`:

    ```
    $ sudo chown -R gpadmin:gpadmin <directory>/greenplum*
    ```


> **Note** All example procedures in the Greenplum Database documentation assume that you installed to the default directory, which is `/usr/local`. If you install to a non-default directory, substitute that directory for `/usr/local`.

If you install to a non-default directory using `rpm`, you will need to continue using `rpm` \(and of `yum`\) to perform minor version upgrades; these changes are covered in the upgrade documentation.

## <a id="topic_xmb_gb5_vhb"></a>Enabling Passwordless SSH 

The `gpadmin` user on each Greenplum host must be able to SSH from any host in the cluster to any other host in the cluster without entering a password or passphrase \(called "passwordless SSH"\). If you enable passwordless SSH from the coordinator host to every other host in the cluster \("1-*n* passwordless SSH"\), you can use the Greenplum Database `gpssh-exkeys` command-line utility to enable passwordless SSH from every host to every other host \("*n*-*n* passwordless SSH"\).

1.  Log in to the coordinator host as the `gpadmin` user.
2.  Source the `path` file in the Greenplum Database installation directory.

    ```
    $ source /usr/local/greenplum-db-<version>/greenplum_path.sh
    ```

    > **Note** Add the above `source` command to the `gpadmin` user's `.bashrc` or other shell startup file so that the Greenplum Database path and environment variables are set whenever you log in as `gpadmin`.

3.  Use the `ssh-copy-id` command to add the `gpadmin` user's public key to the `authorized_hosts` SSH file on every other host in the cluster.

    ```
    $ ssh-copy-id scdw
    $ ssh-copy-id sdw1
    $ ssh-copy-id sdw2
    $ ssh-copy-id sdw3
    . . .
    ```

    This enables 1-*n* passwordless SSH. You will be prompted to enter the `gpadmin` user's password for each host. If you have the `sshpass` command on your system, you can use a command like the following to avoid the prompt.

    ```
    $ SSHPASS=<password> sshpass -e ssh-copy-id scdw
    ```

4.  In the `gpadmin` home directory, create a file named `hostfile_exkeys` that has the machine configured host names and host addresses \(interface names\) for each host in your Greenplum system \(coordinator, standby coordinator, and segment hosts\). Make sure there are no blank lines or extra spaces. Check the `/etc/hosts` file on your systems for the correct host names to use for your environment. For example, if you have a coordinator, standby coordinator, and three segment hosts with two unbonded network interfaces per host, your file would look something like this:

    ```
    cdw
    cdw-1
    cdw-2
    scdw
    scdw-1
    scdw-2
    sdw1
    sdw1-1
    sdw1-2
    sdw2
    sdw2-1
    sdw2-2
    sdw3
    sdw3-1
    sdw3-2
    ```

5.  Run the `gpssh-exkeys` utility with your `hostfile_exkeys` file to enable *n*-*n* passwordless SSH for the `gpadmin` user.

    ```
    $ gpssh-exkeys -f hostfile_exkeys
    ```


## <a id="topic10"></a>Confirming Your Installation 

To make sure the Greenplum software was installed and configured correctly, run the following confirmation steps from your Greenplum coordinator host. If necessary, correct any problems before continuing on to the next task.

1.  Log in to the coordinator host as `gpadmin`:

    ```
    $ su - gpadmin
    ```

2.  Use the `gpssh` utility to see if you can log in to all hosts without a password prompt, and to confirm that the Greenplum software was installed on all hosts. Use the `hostfile_exkeys` file you used to set up passwordless SSH. For example:

    ```
    $ gpssh -f hostfile_exkeys -e 'ls -l /usr/local/greenplum-db-<version>'
    ```

    If the installation was successful, you should be able to log in to all hosts without a password prompt. All hosts should show that they have the same contents in their installation directories, and that the directories are owned by the `gpadmin` user.

    If you are prompted for a password, run the following command to redo the ssh key exchange:

    ```
    $ gpssh-exkeys -f hostfile_exkeys
    ```


## <a id="topic_zdf_1f5_vhb"></a>About Your Greenplum Database Installation 

-   `greenplum_path.sh` — This file contains the environment variables for Greenplum Database. See [Setting Greenplum Environment Variables](init_gpdb.html).
-   **bin** — This directory contains the Greenplum Database management utilities. This directory also contains the PostgreSQL client and server programs, most of which are also used in Greenplum Database.
-   **docs/cli\_help** — This directory contains help files for Greenplum Database command-line utilities.
-   **docs/cli\_help/gpconfigs** — This directory contains sample `gpinitsystem` configuration files and host files that can be modified and used when installing and initializing a Greenplum Database system.
-   **ext** — Bundled programs \(such as Python\) used by some Greenplum Database utilities.
-   **include** — The C header files for Greenplum Database.
-   **lib** — Greenplum Database and PostgreSQL library files.
-   **sbin** — Supporting/Internal scripts and programs.
-   **share** — Shared files for Greenplum Database.

## <a id="topic_cwj_hzb_vhb"></a>Next Steps 

-   [Creating the Data Storage Areas](create_data_dirs.html)
-   [Validating Your Systems](validate.html)
-   [Initializing a Greenplum Database System](init_gpdb.html)

