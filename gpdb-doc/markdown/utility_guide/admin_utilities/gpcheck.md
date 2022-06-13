# gpcheck 

Verifies and validates Greenplum Database platform settings.

**Note:** This utility is deprecated and will be removed in the Greenplum Database 6 release.

## Synopsis 

```
gpcheck {{-f | --file} <hostfile_gpcheck> | {-h | --host} <host_ID>| --local } 
   [-m <master_host>] [-s <standby_master_host>] [--stdout | --zipout] 
   [--config <config_file>]

gpcheck --zipin <gpcheck_zipfile>

gpcheck -? 

gpcheck --version
```

## Description 

The `gpcheck` utility determines the platform on which you are running Greenplum Database and validates various platform-specific configuration settings. `gpcheck` can use a host file or a file previously created with the `--zipout` option to validate platform settings. At the end of a successful validation process, `GPCHECK_NORMAL` message displays. If `GPCHECK_ERROR` displays, one or more validation checks failed. You can use also `gpcheck` to gather and view platform settings on hosts without running validation checks.

Some `gpcheck` validations require `root` privileges. If you run `gpcheck` as `gpadmin` \(without `root` privileges\), the utility displays a warning message and will not be able to perform all validations. However, running `gpcheck` as a user with `root` privileges and specifying remote hosts might require a password to log into the remote hosts. You can perform all `gpcheck` validations on a local host by running `gpcheck` with `root` privileges using the `--local` option. See [Examples](#section5).

## Options 

--config config\_file
:   The name of a configuration file to use instead of the default file `$GPHOME/etc/gpcheck.cnf` \(or `~/gpconfigs/gpcheck_dca_config` on the Dell EMC Greenplum Data Computing Appliance\). This file specifies the OS-specific checks to run.

\{-f \| --file\} hostfile\_gpcheck
:   The name of a file that contains a list of hosts that `gpcheck` uses to validate platform-specific settings. This file should contain a single host name for all hosts in your Greenplum Database system \(master, standby master, and segments\). `gpcheck` uses SSH to connect to the hosts.

\{-h \| --host\} host\_ID
:   Checks platform-specific settings on the host in your Greenplum Database system specified by host\_ID. `gpcheck` uses SSH to connect to the host.

--local
:   Checks platform-specific settings on the segment host where `gpcheck` is run. This option does not require SSH authentication.

-m master\_host
:   This option is deprecated and will be removed in a future release.

-s standby\_master\_host
:   This option is deprecated and will be removed in a future release.

--stdout
:   Display collected host information from `gpcheck`. No checks or validations are performed.

--zipout
:   Save all collected data to a `.zip` file in the current working directory. `gpcheck` automatically creates the `.zip` file and names it gpcheck\_timestamp.tar.gz. No checks or validations are performed.

--zipin gpcheck\_zipfile
:   Use this option to decompress and check a `.zip` file created with the `--zipout` option. `gpcheck` performs validation tasks against the file you specify in this option.

-? \(help\)
:   Displays the online help.

--version
:   Displays the version of this utility.

## Examples 

Verify and validate the Greenplum Database platform settings by entering a host file:

```
# gpcheck -f hostfile_gpcheck 
```

Save Greenplum Database platform settings to a zip file:

```
# gpcheck -f hostfile_gpcheck --zipout
```

Verify and validate the Greenplum Database platform settings using a zip file created with the `--zipout` option:

```
# gpcheck --zipin gpcheck_timestamp.tar.gz
```

View collected Greenplum Database platform settings:

```
# gpcheck -f hostfile_gpcheck --stdout
```

Run `gpcheck`as a user with `root` privileges on a host system to perform all `gpcheck` platform validations on the host. This example assumes the user has `sudo` access.

```
$ sudo -s
# source <GPDB_install_dir>/greenplum_path.sh
# gpcheck --local
```

## See Also 

[gpssh](gpssh.html), [gpscp](gpscp.html), [gpcheckperf](gpcheckperf.html)

