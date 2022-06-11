# gpseginstall 

Installs Greenplum Database on segment hosts.

## Synopsis 

```
gpseginstall -f <hostfile> [-u <gpdb_admin_user>] [-p <password>] 
             [-c u|p|c|s|E|e|l|v]

gpseginstall --help
```

## Description 

The `gpseginstall` utility provides a simple way to quickly install Greenplum Database on segment hosts that you specify in a host list file. The utility does not install or update Greenplum Database on the master host. You can run `gpseginstall` as `root` or as a non-root user. `gpseginstall` does not perform database initialization. See `gpinitsystem` for more information about initializing Greenplum Database.

When run as root, `gpseginstall` default actions are to add a system user \(default is `gpadmin`\), create a password \(default is `changeme`\), and deploy and install Greenplum Database on segment hosts. To do this, `gpseginstall` locates the current Greenplum Database binaries on the master from the installation path in the current user's environment variables \(`$GPHOME`\). It compresses Greenplum Database software into a `tar.gz` file and performs an MD5 checksum to verify file integrity.

Then, it copies Greenplum Database to the segment hosts, installs \(decompresses\) Greenplum Database, and changes the ownership of the Greenplum Database installation to the system user you specify with the `-u` option. Lastly, it exchanges keys between all Greenplum Database hosts as both root and as the system user you specify with the -u option. gpseginstall also perform a user limit check and verifies the version number of Greenplum Database on all the segments.

If you run gpseginstall as a non-root user, gpseginstall only compresses, copies, and installs Greenplum Database on segment hosts. It can also exchanges keys between Greenplum Database hosts for the current system user, and verifies the version number of Greenplum Database on all the segments.

## Options 

-c \| --commands option\_list
:   Optional. This allows you to customize `gpseginstall` actions. Note that these command options are executed by default if you do not specify the `-c` option in the `gpseginstall` syntax.

    -   `u`: Adds a system user. \(`root` only\)
    -   `p`: Changes the password for a system user. \(`root` only\)
    -   `s`: Compresses, copies, decompresses \(installs\) Greenplum Database on all segments.
    -   `c`: Changes the ownership of the Greenplum Database installation directory on the segment hosts. \(`root` only\)
    -   `E`: Exchange keys between Greenplum Database master and segment hosts for the root user. \(`root` only\)
    -   `e`: Exchange keys between Greenplum Database master and segment hosts for the non-root system user.
    -   `l`: \(Linux only\) Checks and modifies the user limits configuration file \(`/etc/security/limits.conf` file\) when adding a new user to segment hosts. \(`root` only\)
    -   `v`: Verifies the version of Greenplum Database running on all segments. `gpseginstall` checks the version number of the Greenplum Database installation referenced by the `$GPHOME` environment variable and symbolic link to the installation directory. An error occurs if there is a version number mismatch or the Greenplum Database installation directory cannot be found.

-f \| --file hostfile
:   Required. This specifies the file that lists the segment hosts onto which you want to install Greenplum Database.

:   The host list file must have one host name per line and includes a host name for each segment host in your Greenplum system. Make sure there are no blank lines or extra spaces. If a host has multiple configured host names, use only one host name per host. For example:

:   ```
sdw1-1
sdw2-1
sdw3-1
sdw4-1
```

:   If available, you can use the same `gpssh-exkeys` host list file you used to exchange keys between Greenplum Database hosts.

-p \| --password password
:   Optional. Sets the password for the user you specify with the `-u` option. The default password is `changeme`. This option is only available when you run `gpsetinstall` as root.

:   Recommended security best practices:

    -   Always use passwords.
    -   Do not use default passwords.
    -   Change default passwords immediately after installation.

-u \| --user user
:   Optional. This specifies the system user. This user is also the Greenplum Database administrative user. This user owns Greenplum Database installation and administers the database. This is also the user under which Greenplum Database is started/initialized. This option is only available when you run `gpseginstall` as `root`. The default is `gpadmin`.

--help \(help\)
:   Displays the online help.

## Examples 

As `root`, install a Greenplum Database on all segments, leave the system user as the default \(`gpadmin`\) and set the `gpadmin` password to `secret123`:

```
# gpseginstall -f my_host_list_file -p secret123
```

As a non-root user, compress and copy Greenplum Database binaries to all segments \(as `gpadmin`\):

```
$ gpseginstall -f host_file
```

As root, add a user \(`gpadmin2`\), set the password for the user \(`secret1234`\), exchange keys between hosts as the new user, check user limits, and verify version numbers, but do not change ownership of Greenplum binaries, compress/copy/ install Greenplum Database on segments, or exchange keys as `root`.

```
$ gpseginstall -f host_file -u gpadmin2 -p secret1234 -c upelv
```

## See Also 

[gpinitsystem](gpinitsystem.html), [gpssh-exkeys](gpssh-exkeys.html)

