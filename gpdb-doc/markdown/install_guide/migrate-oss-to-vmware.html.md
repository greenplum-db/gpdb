---
title: Migrating from Open Source Greenplum Database to VMware Greenplum Database
---

This topic walks you through migration from an Open Source Greenplum Database to the commercial VMware Greenplum Database. Information is presented in the following topics:

[Before You Begin](#before-you-begin)<br>
[Software Dependencies](#software-dependencies)<br>
[Steps to Migrate](#steps-to-migrate)<br>

## <a id="before-you-begin"></a> Before You Begin

- If you have configured the Greenplum Platform Extension Framework (PXF) in your previous Greenplum Database installation, you must stop the PXF service, and you might need to back up PXF configuration files before upgrading to a new version of Greenplum Database. Refer to [PXF Pre-Upgrade Actions topic](../pxf/upgrade_pxf_6x.html#pxfpre)] for instructions. 

- If you were previously using any Greenplum Database extensions such as `gpbackup`, and want to upgrade from an Open Source Greenplum Database to a commercial VMware Greenplum Database -- for example, upgrading from a 6.22.0 open source Greenplum Database to a 6.23.0 commercial VMware Greenplum Databse, you might need to re-install the extensions. You can download the corresponding packages from VMware Tanzu Network, and use the `gppkg` utility to re-install the extensions. 

- Review the Greenplum Database [Platform Requirements topic](platform-requirements-overview.html) to verify that you have all software you need in place to successfully migrate to VMware Greenplum Database.

<!--
## <a id="dependencies"></a>Software Dependencies


| RHEL/CentOS 6/7/8 Packages | Ubuntu 18.04 Packages |
|---------------|-----------------|
| apr | libapr1 |
| apr-util | libaprutil1 |
| bash | bash |
| bzip2 | bzip2 |
| curl | krb5-multidev |
| krb5 | libcurl3-gnutls |
| libcurl | libcurl4 |
| libevent | libevent-2.1-6 |
| libxml2 | libxml2 |
| libyaml | libyaml-0-2 |
| zlib | zlib1g |
| openldap | libldap-2.4-2 |
| openssh-client | openssh-client |
| openssl |	openssl |
| openssl-libs (RHEL7/Centos7) | iproute2 | 
| perl | perl |
| readline | readline |
| rsync	| less |
| R | net-tools |
| sed (used by `gpinitsystem`) | sed |
| tar |	tar |
| zip | zip |

-->

## <a id="Steps"></a>Steps to Migrate

Migrating from an open source Greenplum Database to a commercial VMware Greenplum database involves stopping the open source Greenplum Database, updating the Greenplum Database binary files (WHAT DOES THIS MEAN, EXACTLY?), and restarting the Greenplum Database. If you are using Greenplum Database extension packages, there are additional requirements, as summarized in the [Prerequisites](#prerequisites) section.

Here are the detailed migration steps:

1. Log into your Greenplum Database coordinator host as the Greenplum administrative user:

```
$ su - gpadmin
```

2. Terminate any active connections to the database, and then perform a smart shutdown of your Greenplum Database 6.x system. This example uses the `-a` option to deactivate confirmation prompts:

```
$ gpstop -a
```

3. Download the commercial VMware Greenplum package from [VMware Tanzu Network](https://network.pivotal.io/), and then copy the package to the `gpadmin` user's home directory on each coordinator, standby, and segment host.

4. If you used `yum` or `apt` to install Greenplum Database to the default location, run the following commands on each host to upgrade to the new software release. (I DON'T KNOW WHAT YOU MEAN BY "upgrade to the new software release"):

For RHEL/CentOS systems:

```
$ sudo yum upgrade ./greenplum-db-<version>-<platform>.rpm
```

For Ubuntu systems:

```
# apt install ./greenplum-db-<version>-<platform>.deb
```

The `yum` or `apt` command installs the commercial VMware Greenplum Database software files into a version-specific directory under `/usr/local` and updates the symbolic link `/usr/local/greenplum-db` to point to the new installation directory.

5. If you used `rpm` to install Greenplum Database to a non-default location on RHEL/CentOS systems, run `rpm` on each host to upgrade to the new software release (AGAIN, I DON'T KNOW WHAT THIS MEANS) and specify the same custom installation directory with the `--prefix` option, as in the following example:

```
$ sudo rpm -U ./greenplum-db-<version>-<platform>.rpm --prefix=<directory>
```

The `rpm` command installs the new Greenplum Database software files into a version-specific directory under the directory you specify, and updates the symbolic link <directory>/greenplum-db to point to the new installation directory.

6. Update the permissions for the new installation. For example, run the following command as root to change the user and group of the installed files to `gpadmin`:

```
$ sudo chown -R gpadmin:gpadmin /usr/local/greenplum*
```

7. If doing an upgrade (WHAT KIND OF UPGRADE ARE YOU REERRING TO? FROM WHAT TO WHAT?), edit the environment of the Greenplum Database superuser (gpadmin) and verify that you are sourcing the `greenplum_path.sh` file for the new installation. For example, update the following line in the `.bashrc` file or in  your profile file:

`source /usr/local/greenplum-db-<current_version>/greenplum_path.sh`

to

`source /usr/local/greenplum-db-<new_version>/greenplum_path.sh`
	
>**Note**
>if you are sourcing a symbolic link (`/usr/local/greenplum-db`) in your profile files, the symbolic link will redirect to the newly installed gpdb folder; no action is necessary.

8. Source the environment file you just edited. For example:

```
$ source ~/.bashrc
```

9. Once all segment hosts have been updated, log into the database as the `gpadmin` user and restart your Greenplum Database system:

```
$ su - gpadmin
$ gpstart
```

10. Log into the commercial VMware Greenplum Database coordinator host as the Greenplum administrative user and check the database version: 

```
$ su - gpadmin

$ psql -d postgres

$ select versions(); 
```

