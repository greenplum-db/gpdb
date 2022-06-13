# gpinitsystem 

Initializes a Greenplum Database system using configuration parameters specified in the `gpinitsystem_config` file.

## Synopsis 

```
gpinitsystem -c <cluster_configuration_file> 
            [-h <hostfile_gpinitsystem>]
            [-B <parallel_processes>] 
            [-p <postgresql_conf_param_file>]
            [-s <standby_master_host> [-P <standby_master_port>]
              [-F <standby_master_filespaces>]]
            [-m <number> | --max_connections=<number>]
            [-b <size> | --shared_buffers=<size>]
            [-n <locale> | --locale=<locale>] [--lc-collate=<locale>] 
            [--lc-ctype=<locale>] [--lc-messages=<locale>] 
            [--lc-monetary=<locale>] [--lc-numeric=<locale>] 
            [--lc-time=<locale>] [-e <password> | --su_password=<password>] 
            [-S] [-a] [-q] [-l <logfile_directory>] [-D]
            [-I <input_configuration_file>]
            [-O <output_configuration_file>]

gpinitsystem -v
```

## Description 

The `gpinitsystem` utility creates a Greenplum Database instance or writes an input configuration file using the values defined in a cluster configuration file and any command-line options that you provide. See [Initialization Configuration File Format](#section5) for more information about the configuration file. Before running this utility, make sure that you have installed the Greenplum Database software on all the hosts in the array.

With the `-O output\_configuration\_file` option, `gpinitsystem` does not create a new database instance but instead writes all provided configuration information to the specified output file. This file uses the `QD_PRIMARY_ARRAY` and `PRIMARY_ARRAY` parameters to define each member using its hostname, port, data directory, segment prefix, segment ID, and content ID. Details of the array configuration can be modified as necessary to match values available in a Greenplum Database backup, or can simply be used to recreate the same cluster configuration at a later time. Configuration files that use `QD_PRIMARY_ARRAY` and `PRIMARY_ARRAY` must be passed into `gpinitsystem` using the `-I input\_configuration\_file` option. See [Initialization Configuration File Format](#section5) for more information.

In a Greenplum Database DBMS, each database instance \(the master and all segments\) must be initialized across all of the hosts in the system in such a way that they can all work together as a unified DBMS. The `gpinitsystem` utility takes care of initializing the Greenplum master and each segment instance, and configuring the system as a whole.

Before running `gpinitsystem`, you must set the `$GPHOME` environment variable to point to the location of your Greenplum Database installation on the master host and exchange SSH keys between all host addresses in the array using `gpssh-exkeys`.

This utility performs the following tasks:

-   Verifies that the parameters in the configuration file are correct.
-   Ensures that a connection can be established to each host address. If a host address cannot be reached, the utility will exit.
-   Verifies the locale settings.
-   Displays the configuration that will be used and prompts the user for confirmation.
-   Initializes the master instance.
-   Initializes the standby master instance \(if specified\).
-   Initializes the primary segment instances.
-   Initializes the mirror segment instances \(if mirroring is configured\).
-   Configures the Greenplum Database system and checks for errors.
-   Starts the Greenplum Database system.

**Note:** This utility uses secure shell \(SSH\) connections between systems to perform its tasks. In large Greenplum Database deployments, cloud deployments, or deployments with a large number of segments per host, this utility may exceed the host's maximum threshold for unauthenticated connections. Consider updating the SSH `MaxStartups` and `MaxSessions` configuration parameters to increase this threshold. For more information about SSH configuration options, refer to the SSH documentation for your Linux distribution.

## Options 

-a
:   Do not prompt the user for confirmation.

-B parallel\_processes
:   The number of segments to create in parallel. If not specified, the utility will start up to 4 parallel processes at a time.

-c cluster\_configuration\_file
:   Required. The full path and filename of the configuration file, which contains all of the defined parameters to configure and initialize a new Greenplum Database system. See [Initialization Configuration File Format](#section5) for a description of this file. You must provide either the `-c cluster\_configuration\_file` option or the `-I input\_configuration\_file` option to `gpinitsystem`.

-D
:   Sets log output level to debug.

-h hostfile\_gpinitsystem
:   Optional. The full path and filename of a file that contains the host addresses of your segment hosts. If not specified on the command line, you can specify the host file using the `MACHINE_LIST_FILE` parameter in the `gpinitsystem\_config` file.

-I input\_configuration\_file
:   The full path and filename of an input configuration file, which defines the Greenplum Database members and segments using the `QD_PRIMARY_ARRAY` and `PRIMARY_ARRAY` parameters. The input configuration file is typically created by using `gpinitsystem` with the `-O output\_configuration\_file` option. You must provide either the `-c cluster\_configuration\_file` option or the `-I input\_configuration\_file` option to `gpinitsystem`.

--locale=locale \| -n locale
:   Sets the default locale used by Greenplum Database. If not specified, the `LC_ALL`, `LC_COLLATE`, or `LANG` environment variable of the master host determines the locale. If these are not set, the default locale is `C` \(`POSIX`\). A locale identifier consists of a language identifier and a region identifier, and optionally a character set encoding. For example, `sv_SE` is Swedish as spoken in Sweden, `en_US` is U.S. English, and `fr_CA` is French Canadian. If more than one character set can be useful for a locale, then the specifications look like this: `en_US.UTF-8` \(locale specification and character set encoding\). On most systems, the command `locale` will show the locale environment settings and `locale -a` will show a list of all available locales.

--lc-collate=locale
:   Similar to `--locale`, but sets the locale used for collation \(sorting data\). The sort order cannot be changed after Greenplum Database is initialized, so it is important to choose a collation locale that is compatible with the character set encodings that you plan to use for your data. There is a special collation name of `C` or `POSIX` \(byte-order sorting as opposed to dictionary-order sorting\). The `C` collation can be used with any character encoding.

--lc-ctype=locale
:   Similar to `--locale`, but sets the locale used for character classification \(what character sequences are valid and how they are interpreted\). This cannot be changed after Greenplum Database is initialized, so it is important to choose a character classification locale that is compatible with the data you plan to store in Greenplum Database.

--lc-messages=locale
:   Similar to `--locale`, but sets the locale used for messages output by Greenplum Database. The current version of Greenplum Database does not support multiple locales for output messages \(all messages are in English\), so changing this setting will not have any effect.

--lc-monetary=locale
:   Similar to `--locale`, but sets the locale used for formatting currency amounts.

--lc-numeric=locale
:   Similar to `--locale`, but sets the locale used for formatting numbers.

--lc-time=locale
:   Similar to `--locale`, but sets the locale used for formatting dates and times.

-l logfile\_directory
:   The directory to write the log file. Defaults to `~/gpAdminLogs`.

--max\_connections=number \| -m number
:   Sets the maximum number of client connections allowed to the master. The default is 250.

-O output\_configuration\_file
:   When used with the `-O` option, `gpinitsystem` does not create a new Greenplum Database cluster but instead writes the supplied cluster configuration information to the specified `output\_configuration\_file`. This file defines Greenplum Database members and segments using the `QD_PRIMARY_ARRAY`, `PRIMARY_ARRAY`, and `MIRROR_ARRAY` parameters, and can be later used with `-I input\_configuration\_file` to initialize a new cluster.

-p postgresql\_conf\_param\_file
:   Optional. The name of a file that contains `postgresql.conf` parameter settings that you want to set for Greenplum Database. These settings will be used when the individual master and segment instances are initialized. You can also set parameters after initialization using the `gpconfig` utility.

-q
:   Run in quiet mode. Command output is not displayed on the screen, but is still written to the log file.

--shared\_buffers=size \| -b size
:   Sets the amount of memory a Greenplum server instance uses for shared memory buffers. You can specify sizing in kilobytes \(kB\), megabytes \(MB\) or gigabytes \(GB\). The default is 125MB.

-s standby\_master\_host
:   Optional. If you wish to configure a backup master host, specify the host name using this option. The Greenplum Database software must already be installed and configured on this host.

-P standby\_master\_port
:   Optional. If you configure a standby master host, specify its port number using this option. The Greenplum Database software must already be installed and configured on this host.

-F standby\_master\_filespaces
:   Optional. If you configure a standby master host, specify a list of filespace names and the associated locations using this option. Each filespace name and its location is separated by a colon. If there is more than one file space name, each pair \(name and location\) is separated by a comma. For example:

:   ```
filespace1_name:fs1_location,filespace2_name:fs2_location
```

:   If this option is not specified, `gpinitstandby` prompts the user for the filespace names and locations.

:   If the list is not formatted correctly or number of filespaces do not match the number of filespaces already created in the system, `gpinitstandby` returns an error.

--su\_password=superuser\_password \| -e superuser\_password
:   Use this option to specify the password to set for the Greenplum Database superuser account \(such as `gpadmin`\). If this option is not specified, the default password `gparray` is assigned to the superuser account. You can use the `ALTER ROLE` command to change the password at a later time.

    Recommended security best practices:

    -   Do not use the default password option for production environments.
    -   Change the password immediately after installation.

-S
:   If mirroring parameters are specified, spreads the mirror segments across the available hosts. The default is to group the set of mirror segments together on an alternate host from their primary segment set. Mirror spreading places each mirror on a different host within the Greenplum Database array. Spreading is only allowed if the number of hosts is greater than the number of segment instances.

-v
:   Displays the version of this utility.

-h
:   Displays the online help.

## Initialization Configuration File Format 

`gpinitsystem` requires a cluster configuration file with the following parameters defined. An example initialization configuration file can be found in `$GPHOME/docs/cli_help/gpconfigs/gpinitsystem_config`.

To avoid port conflicts between Greenplum Database and other applications, the Greenplum Database port numbers should not be in the range specified by the operating system parameter `net.ipv4.ip_local_port_range`. For example, if `net.ipv4.ip_local_port_range = 10000 65535`, you could set Greenplum Database base port numbers to these values.

```
PORT_BASE = 6000
MIRROR_PORT_BASE = 7000
REPLICATION_PORT_BASE = 8000
MIRROR_REPLICATION_PORT_BASE = 9000
```

ARRAY\_NAME
:   **Required.** A name for the array you are configuring. You can use any name you like. Enclose the name in quotes if the name contains spaces.

MACHINE\_LIST\_FILE
:   **Optional.** Can be used in place of the `-h` option. This specifies the file that contains the list of segment host address names that comprise the Greenplum Database system. The master host is assumed to be the host from which you are running the utility and should not be included in this file. If your segment hosts have multiple network interfaces, then this file would include all addresses for the host. Give the absolute path to the file.

SEG\_PREFIX
:   **Required.** This specifies a prefix that will be used to name the data directories on the master and segment instances. The naming convention for data directories in a Greenplum Database system is SEG\_PREFIXnumber where number starts with 0 for segment instances \(the master is always -1\). So for example, if you choose the prefix `gpseg`, your master instance data directory would be named `gpseg-1`, and the segment instances would be named `gpseg0`, `gpseg1`, `gpseg2`, `gpseg3`, and so on.

PORT\_BASE
:   **Required.** This specifies the base number by which primary segment port numbers are calculated. The first primary segment port on a host is set as `PORT_BASE`, and then incremented by one for each additional primary segment on that host. Valid values range from 1 through 65535.

DATA\_DIRECTORY
:   **Required.** This specifies the data storage location\(s\) where the utility will create the primary segment data directories. The number of locations in the list dictate the number of primary segments that will get created per physical host \(if multiple addresses for a host are listed in the host file, the number of segments will be spread evenly across the specified interface addresses\). It is OK to list the same data storage area multiple times if you want your data directories created in the same location. The user who runs `gpinitsystem` \(for example, the `gpadmin` user\) must have permission to write to these directories. For example, this will create six primary segments per host:

:   ```
declare -a DATA_DIRECTORY=(/data1/primary /data1/primary 
/data1/primary /data2/primary /data2/primary /data2/primary)
```

MASTER\_HOSTNAME
:   **Required.** The host name of the master instance. This host name must exactly match the configured host name of the machine \(run the `hostname` command to determine the correct hostname\).

MASTER\_DIRECTORY
:   **Required.** This specifies the location where the data directory will be created on the master host. You must make sure that the user who runs `gpinitsystem` \(for example, the `gpadmin` user\) has permissions to write to this directory.

MASTER\_PORT
:   **Required.** The port number for the master instance. This is the port number that users and client connections will use when accessing the Greenplum Database system.

TRUSTED\_SHELL
:   **Required.** The shell the `gpinitsystem` utility uses to execute commands on remote hosts. Allowed values are `ssh`. You must set up your trusted host environment before running the `gpinitsystem` utility \(you can use `gpssh-exkeys` to do this\).

CHECK\_POINT\_SEGMENTS
:   **Required.** Maximum distance between automatic write ahead log \(WAL\) checkpoints, in log file segments \(each segment is normally 16 megabytes\). This will set the `checkpoint_segments` parameter in the `postgresql.conf` file for each segment instance in the Greenplum Database system.

ENCODING
:   **Required.** The character set encoding to use. This character set must be compatible with the `--locale` settings used, especially `--lc-collate` and `--lc-ctype`. Greenplum Database supports the same character sets as PostgreSQL.

DATABASE\_NAME
:   **Optional.** The name of a Greenplum Database database to create after the system is initialized. You can always create a database later using the `CREATE DATABASE` command or the `createdb` utility.

MIRROR\_PORT\_BASE
:   **Optional.** This specifies the base number by which mirror segment port numbers are calculated. The first mirror segment port on a host is set as `MIRROR_PORT_BASE`, and then incremented by one for each additional mirror segment on that host. Valid values range from 1 through 65535 and cannot conflict with the ports calculated by `PORT_BASE`.

REPLICATION\_PORT\_BASE
:   **Optional.** This specifies the base number by which the port numbers for the primary file replication process are calculated. The first replication port on a host is set as `REPLICATION_PORT_BASE`, and then incremented by one for each additional primary segment on that host. Valid values range from 1 through 65535 and cannot conflict with the ports calculated by `PORT_BASE` or `MIRROR_PORT_BASE`.

MIRROR\_REPLICATION\_PORT\_BASE
:   **Optional.** This specifies the base number by which the port numbers for the mirror file replication process are calculated. The first mirror replication port on a host is set as `MIRROR_REPLICATION_PORT_BASE`, and then incremented by one for each additional mirror segment on that host. Valid values range from 1 through 65535 and cannot conflict with the ports calculated by `PORT_BASE`, `MIRROR_PORT_BASE`, or `REPLICATION_PORT_BASE`.

MIRROR\_DATA\_DIRECTORY
:   **Optional.** This specifies the data storage location\(s\) where the utility will create the mirror segment data directories. There must be the same number of data directories declared for mirror segment instances as for primary segment instances \(see the `DATA_DIRECTORY` parameter\). The user who runs `gpinitsystem` \(for example, the `gpadmin` user\) must have permission to write to these directories. For example:

:   ```
declare -a MIRROR_DATA_DIRECTORY=(/data1/mirror 
/data1/mirror /data1/mirror /data2/mirror /data2/mirror 
/data2/mirror)
```

QD\_PRIMARY\_ARRAY, PRIMARY\_ARRAY, MIRROR\_ARRAY
:   These parameters can only be provided using an input configuration file, with the `gpinitsystem` `-I input\_configuration\_file` option. `QD_PRIMARY_ARRAY`, `PRIMARY_ARRAY`, and `MIRROR_ARRAY` define the Greenplum Database master host and the primary and mirror instances on the segment hosts, respectively, using the format:

    ```
    <host>~<port>~<data_directory>/<seg_prefix<segment_id>>~<dbid>~<content_id>~<replication_port>
    ```

    The Greenplum Database master always uses the value -1 for the segment ID and content ID. For example:

    ```
    QD_PRIMARY_ARRAY=127.0.0.1~5432~/gpmaster/gpsne-1~1~-1~0
    declare -a PRIMARY_ARRAY=(
    127.0.0.1~40000~/gpdata1/gpsne0~2~0~6000
    127.0.0.1~40001~/gpdata2/gpsne1~3~1~6001
    )
    declare -a MIRROR_ARRAY=(
    127.0.0.1~50000~/gpmirror1/gpsne0~4~0~51000
    127.0.0.1~50001~/gpmirror2/gpsne1~5~1~51001
    )
    ```

:   You can use the `gpinitsystem` `-O output\_configuration\_file` to populate `QD_PRIMARY_ARRAY`, `PRIMARY_ARRAY`, `MIRROR_ARRAY` using the hosts, data directories, segment prefix, and base port values that you provide to the command. For recovery purposes, you can edit the segment and content IDs to match the values of an existing Greenplum Database backup.

HEAP\_CHECKSUM
:   **Optional.** This parameter specifies if checksums are enabled for heap data. When enabled, checksums are calculated for heap storage in all databases, enabling Greenplum Database to detect corruption in the I/O system. This option is set when the system is initialized and cannot be changed later.

:   The `HEAP_CHECKSUM` option is on by default and turning it off is strongly discouraged. If you set this option to off, data corruption in storage can go undetected and make recovery much more difficult.

:   To determine if heap checksums are enabled in a Greenplum Database system, you can query the `data_checksums` server configuration parameter with the `gpconfig` management utility:

    ```
    $ gpconfig -s data_checksums
    ```

HBA\_HOSTNAMES
:   **Optional.** This parameter controls whether `gpinitsystem` uses IP addresses or host names in the `pg_hba.conf` file when updating the file with addresses that can connect to Greenplum Database. The default value is `0`, the utility uses IP addresses when updating the file. When initializing a Greenplum Database system, specify `HBA_HOSTNAMES=1` to have the utility use host names in the `pg_hba.conf` file.

:   For information about how Greenplum Database resolves host names in the `pg_hba.conf` file, see [Configuring Client Authentication](../../admin_guide/client_auth.html).

## Examples 

Initialize a Greenplum Database array by supplying a cluster configuration file and a segment host address file, and set up a spread mirroring \(`-S`\) configuration:

```
$ gpinitsystem -c gpinitsystem_config -h 
hostfile_gpinitsystem -S
```

Initialize a Greenplum Database array and set the superuser remote password:

```
$ gpinitsystem -c gpinitsystem_config -h 
hostfile_gpinitsystem --su-password=mypassword
```

Initialize a Greenplum Database array with an optional standby master host:

```
$ gpinitsystem -c gpinitsystem_config -h 
hostfile_gpinitsystem -s host09
```

Instead of initializing a Greenplum Database array, write the provided configuration to an output file. The output file uses the `QD_PRIMARY_ARRAY` and `PRIMARY_ARRAY` parameters to define master and segment hosts:

```
$ gpinitsystem -c gpinitsystem_config -h 
hostfile_gpinitsystem -S -O cluster_init.config
```

Initialize a Greenplum Database using an input configuration file \(a file that defines the Greenplum Database array using `QD_PRIMARY_ARRAY` and `PRIMARY_ARRAY` parameters:

```
$ gpinitsystem -I cluster_init.config
```

## See Also 

[gpssh-exkeys](gpssh-exkeys.html), [gpdeletesystem](gpdeletesystem.html)

