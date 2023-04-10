# gpinitsystem 

Initializes a Greenplum Database system using configuration parameters specified in the `gpinitsystem_config` file.

## <a id="section2"></a>Synopsis 

```
gpinitsystem -c <cluster_configuration_file> 
            [-h <hostfile_gpinitsystem>]
            [-B <parallel_processes>] 
            [-p <postgresql_conf_param_file>]
            [-s <standby_coordinator_host>
                [-P <standby_coordinator_port>]
                [-S <standby_coordinator_datadir> | --standby_datadir=<standby_coordinator_datadir>]]
            [-m <number> | --max_connections=number>]
            [-b <size> | --shared_buffers=<size>]
            [-n <locale> | --locale=<locale>] [--lc-collate=<locale>] 
            [--lc-ctype=<locale>] [--lc-messages=<locale>] 
            [--lc-monetary=<locale>] [--lc-numeric=<locale>] 
            [--lc-time=<locale>] [-e <password> | --su_password=<password>] 
            [--mirror-mode={group|spread}] [-a] [-q] [-l <logfile_directory>] [-D]
            [-I <input_configuration_file>]
            [-O <output_configuration_file>]

gpinitsystem -v | --version

gpinitsystem -? | --help
```

## <a id="section3"></a>Description 

The `gpinitsystem` utility creates a Greenplum Database instance or writes an input configuration file using the values defined in a cluster configuration file and any command-line options that you provide. See [Initialization Configuration File Format](#section5) for more information about the configuration file. Before running this utility, make sure that you have installed the Greenplum Database software on all the hosts in the array.

With the `<-O output_configuration_file>` option, `gpinitsystem` writes all provided configuration information to the specified output file. This file can be used with the `-I` option to create a new cluster or re-create a cluster from a backed up configuration. See [Initialization Configuration File Format](#section5) for more information.

In a Greenplum Database DBMS, each database instance \(the coordinator instance and all segment instances\) must be initialized across all of the hosts in the system in such a way that they can all work together as a unified DBMS. The `gpinitsystem` utility takes care of initializing the Greenplum coordinator and each segment instance, and configuring the system as a whole.

Before running `gpinitsystem`, you must set the `$GPHOME` environment variable to point to the location of your Greenplum Database installation on the coordinator host and exchange SSH keys between all host addresses in the array using `gpssh-exkeys`.

This utility performs the following tasks:

-   Verifies that the parameters in the configuration file are correct.
-   Ensures that a connection can be established to each host address. If a host address cannot be reached, the utility will exit.
-   Verifies the locale settings.
-   Displays the configuration that will be used and prompts the user for confirmation.
-   Initializes the coordinator instance.
-   Initializes the standby coordinator instance \(if specified\).
-   Initializes the primary segment instances.
-   Initializes the mirror segment instances \(if mirroring is configured\).
-   Configures the Greenplum Database system and checks for errors.
-   Starts the Greenplum Database system.

> **Note** This utility uses secure shell \(SSH\) connections between systems to perform its tasks. In large Greenplum Database deployments, cloud deployments, or deployments with a large number of segments per host, this utility may exceed the host's maximum threshold for unauthenticated connections. Consider updating the SSH `MaxStartups` and `MaxSessions` configuration parameters to increase this threshold. For more information about SSH configuration options, refer to the SSH documentation for your Linux distribution.

## <a id="section4"></a>Options 

-a
:   Do not prompt the user for confirmation.

-B parallel\_processes
:   The number of segments to create in parallel. If not specified, the utility will start up to 4 parallel processes at a time.

-c cluster\_configuration\_file
:   Required. The full path and filename of the configuration file, which contains all of the defined parameters to configure and initialize a new Greenplum Database system. See [Initialization Configuration File Format](#section5) for a description of this file. You must provide either the `-c <cluster_configuration_file>` option or the `-I <input_configuration_file>` option to `gpinitsystem`.

-D
:   Sets log output level to debug.

-h hostfile\_gpinitsystem
:   Optional. The full path and filename of a file that contains the host addresses of your segment hosts. If not specified on the command line, you can specify the host file using the `MACHINE_LIST_FILE` parameter in the gpinitsystem\_config file.

-I input\_configuration\_file
:   The full path and filename of an input configuration file, which defines the Greenplum Database host systems, the coordinator instance and segment instances on the hosts, using the `QD_PRIMARY_ARRAY`, `PRIMARY_ARRAY`, and `MIRROR_ARRAY` parameters. The input configuration file is typically created by using `gpinitsystem` with the `-O output\_configuration\_file` option. Edit those parameters in order to initialize a new cluster or re-create a cluster from a backed up configuration. You must provide either the `-c <cluster_configuration_file>` option or the `-I <input_configuration_file>` option to `gpinitsystem`.

-n locale \| --locale=locale
:   Sets the default locale used by Greenplum Database. If not specified, the default locale is `en_US.utf8`. A locale identifier consists of a language identifier and a region identifier, and optionally a character set encoding. For example, `sv_SE` is Swedish as spoken in Sweden, `en_US` is U.S. English, and `fr_CA` is French Canadian. If more than one character set can be useful for a locale, then the specifications look like this: `en_US.UTF-8` \(locale specification and character set encoding\). On most systems, the command `locale` will show the locale environment settings and `locale -a` will show a list of all available locales.

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

-m number \| --max\_connections=number
:   Sets the maximum number of client connections allowed to the coordinator. The default is 250.

-O output\_configuration\_file
:   Optional, used during new cluster initialization. This option writes the `cluster_configuration_file` information \(used with -c\) to the specified `output_configuration_file`. This file defines the Greenplum Database members using the `QD_PRIMARY_ARRAY`, `PRIMARY_ARRAY`, and `MIRROR_ARRAY` parameters. Use this file as a template for the `-I` `input_configuration_file` option. See [Examples](#section6) for more information.

-p postgresql\_conf\_param\_file
:   Optional. The name of a file that contains `postgresql.conf` parameter settings that you want to set for Greenplum Database. These settings will be used when the individual coordinator and segment instances are initialized. You can also set parameters after initialization using the `gpconfig` utility.

-q
:   Run in quiet mode. Command output is not displayed on the screen, but is still written to the log file.

-b size \| --shared\_buffers=size
:   Sets the amount of memory a Greenplum server instance uses for shared memory buffers. You can specify sizing in kilobytes \(kB\), megabytes \(MB\) or gigabytes \(GB\). The default is 125MB.

-s standby\_coordinator\_host
:   Optional. If you wish to configure a backup coordinator instance, specify the host name using this option. The Greenplum Database software must already be installed and configured on this host.

-P standby\_coordinator\_port
:   If you configure a standby coordinator instance with `-s`, specify its port number using this option. The default port is the same as the coordinator port. To run the standby and coordinator on the same host, you must use this option to specify a different port for the standby. The Greenplum Database software must already be installed and configured on the standby host.

-S standby\_coordinator\_datadir \| --standby\_dir=standby\_coordinator\_datadir
:   If you configure a standby coordinator host with `-s`, use this option to specify its data directory. If you configure a standby on the same host as the coordinator instance, the coordinator and standby must have separate data directories.

-e superuser\_password \| --su\_password=superuser\_password
:   Use this option to specify the password to set for the Greenplum Database superuser account \(such as `gpadmin`\). If this option is not specified, the default password `gparray` is assigned to the superuser account. You can use the `ALTER ROLE` command to change the password at a later time.

    Recommended security best practices:

    -   Do not use the default password option for production environments.
    -   Change the password immediately after installation.

--mirror-mode=\{group\|spread\}
:   Use this option to specify the placement of mirror segment instances on the segment hosts. The default, `group`, groups the mirror segments for all of a host's primary segments on a single alternate host. `spread` spreads mirror segments for the primary segments on a host across different hosts in the Greenplum Database array. Spreading is only allowed if the number of hosts is greater than the number of segment instances per host. See [Overview of Segment Mirroring](../../admin_guide/highavail/topics/g-overview-of-segment-mirroring.html#topic3) for information about Greenplum Database mirroring strategies.

-v \| --version
:   Print the `gpinitsystem` version and exit.

-? \| --help
:   Show help about `gpinitsystem` command line arguments, and exit.

## <a id="section5"></a>Initialization Configuration File Format 

`gpinitsystem` requires a cluster configuration file with the following parameters defined. An example initialization configuration file can be found in `$GPHOME/docs/cli_help/gpconfigs/gpinitsystem_config`.

To avoid port conflicts between Greenplum Database and other applications, the Greenplum Database port numbers should not be in the range specified by the operating system parameter `net.ipv4.ip_local_port_range`. For example, if `net.ipv4.ip_local_port_range = 10000 65535`, you could set Greenplum Database base port numbers to these values.

```
PORT_BASE = 6000
MIRROR_PORT_BASE = 7000
```

MACHINE\_LIST\_FILE
:   **Optional.** Can be used in place of the `-h` option. This specifies the file that contains the list of the segment host address names that comprise the Greenplum Database system. The coordinator host is assumed to be the host from which you are running the utility and should not be included in this file. If your segment hosts have multiple network interfaces, then this file would include all addresses for the host. Give the absolute path to the file.

SEG\_PREFIX
:   **Required.** This specifies a prefix that will be used to name the data directories on the coordinator and segment instances. The naming convention for data directories in a Greenplum Database system is SEG\_PREFIXnumber where number starts with 0 for segment instances \(the coordinator is always -1\). So for example, if you choose the prefix `gpseg`, your coordinator instance data directory would be named `gpseg-1`, and the segment instances would be named `gpseg0`, `gpseg1`, `gpseg2`, `gpseg3`, and so on.

PORT\_BASE
:   **Required.** This specifies the base number by which primary segment port numbers are calculated. The first primary segment port on a host is set as `PORT_BASE`, and then incremented by one for each additional primary segment on that host. Valid values range from 1 through 65535.

DATA\_DIRECTORY
:   **Required.** This specifies the data storage location\(s\) where the utility will create the primary segment data directories. The number of locations in the list dictate the number of primary segments that will get created per physical host \(if multiple addresses for a host are listed in the host file, the number of segments will be spread evenly across the specified interface addresses\). It is OK to list the same data storage area multiple times if you want your data directories created in the same location. The user who runs `gpinitsystem` \(for example, the `gpadmin` user\) must have permission to write to these directories. For example, this will create six primary segments per host:

:   ```
declare -a DATA_DIRECTORY=(/data1/primary /data1/primary 
/data1/primary /data2/primary /data2/primary /data2/primary)
```

MASTER\_HOSTNAME
:   **Required.** The host name of the coordinator instance. This host name must exactly match the configured host name of the machine \(run the `hostname` command to determine the correct hostname\).

MASTER\_DIRECTORY
:   **Required.** This specifies the location where the data directory will be created on the coordinator host. You must make sure that the user who runs `gpinitsystem` \(for example, the `gpadmin` user\) has permissions to write to this directory.

MASTER\_PORT
:   **Required.** The port number for the coordinator instance. This is the port number that users and client connections will use when accessing the Greenplum Database system.

TRUSTED\_SHELL
:   **Required.** The shell the `gpinitsystem` utility uses to run commands on remote hosts. Allowed values are `ssh`. You must set up your trusted host environment before running the `gpinitsystem` utility \(you can use `gpssh-exkeys` to do this\).

ENCODING
:   **Required.** The character set encoding to use. This character set must be compatible with the `--locale` settings used, especially `--lc-collate` and `--lc-ctype`. Greenplum Database supports the same character sets as PostgreSQL.

DATABASE\_NAME
:   **Optional.** The name of a Greenplum Database database to create after the system is initialized. You can always create a database later using the `CREATE DATABASE` command or the `createdb` utility.

MIRROR\_PORT\_BASE
:   **Optional.** This specifies the base number by which mirror segment port numbers are calculated. The first mirror segment port on a host is set as `MIRROR_PORT_BASE`, and then incremented by one for each additional mirror segment on that host. Valid values range from 1 through 65535 and cannot conflict with the ports calculated by `PORT_BASE`.

MIRROR\_DATA\_DIRECTORY
:   **Optional.** This specifies the data storage location\(s\) where the utility will create the mirror segment data directories. There must be the same number of data directories declared for mirror segment instances as for primary segment instances \(see the `DATA_DIRECTORY` parameter\). The user who runs `gpinitsystem` \(for example, the `gpadmin` user\) must have permission to write to these directories. For example:

:   ```
declare -a MIRROR_DATA_DIRECTORY=(/data1/mirror 
/data1/mirror /data1/mirror /data2/mirror /data2/mirror 
/data2/mirror)
```

QD\_PRIMARY\_ARRAY, PRIMARY\_ARRAY, MIRROR\_ARRAY
:   **Required** when using `input_configuration file` with `-I` option. These parameters specify the Greenplum Database coordinator host, the primary segment, and the mirror segment hosts respectively. During new cluster initialization, use the `gpinitsystem` `-O output\_configuration\_file` to populate `QD_PRIMARY_ARRAY`, `PRIMARY_ARRAY`, `MIRROR_ARRAY`.

:   To initialize a new cluster or re-create a cluster from a backed up configuration, edit these values in the input configuration file used with the `gpinitsystem` `-I input\_configuration\_file` option. Use one of the following formats to specify the host information:

    ```
    <hostname>~<address>~<port>~<data_directory>/<seg_prefix<segment_id>~<dbid>~<content_id>
    ```

    or

    ```
    <host>~<port>~<data_directory>/<seg_prefix<segment_id>~<dbid>~<content_id>
    ```

:   The first format populates the `hostname` and `address` fields in the `gp_segment_configuration` catalog table with the hostname and address values provided in the input configuration file. The second format populates `hostname` and `address` fields with the same value, derived from host.

:   The Greenplum Database coordinator always uses the value -1 for the segment ID and content ID. For example, seg\_prefix<segment\_id\> and dbid values for `QD_PRIMARY_ARRAY` use `-1` to indicate the coordinator instance:

    ```
    QD_PRIMARY_ARRAY=cdw~cdw~5432~/gpdata/coordinator/gpseg-1~1~-1
    declare -a PRIMARY_ARRAY=(
    sdw1~sdw1~40000~/gpdata/data1/gpseg0~2~0
    sdw1~sdw1~40001~/gpdata/data2/gpseg1~3~1
    sdw2~sdw2~40000~/gpdata/data1/gpseg2~4~2
    sdw2~sdw2~40001~/gpdata/data2/gpseg3~5~3
    )
    declare -a MIRROR_ARRAY=(
    sdw2~sdw2~50000~/gpdata/mirror1/gpseg0~6~0
    sdw2~sdw2~50001~/gpdata/mirror2/gpseg1~7~1
    sdw1~sdw1~50000~/gpdata/mirror1/gpseg2~8~2
    sdw1~sdw1~50001~/gpdata/mirror2/gpseg3~9~3
    )
    ```

:   To re-create a cluster using a known Greenplum Database system configuration, you can edit the segment and content IDs to match the values of the system.

HEAP\_CHECKSUM
:   **Optional.** This parameter specifies if checksums are enabled for heap data. When enabled, checksums are calculated for heap storage in all databases, enabling Greenplum Database to detect corruption in the I/O system. This option is set when the system is initialized and cannot be changed later.

:   The `HEAP_CHECKSUM` option is on by default and turning it off is strongly discouraged. If you set this option to off, data corruption in storage can go undetected and make recovery much more difficult.

:   To determine if heap checksums are enabled in a Greenplum Database system, you can query the `data_checksums` server configuration parameter with the `gpconfig` management utility:

    ```
    $ gpconfig -s data_checksums
    ```

HBA\_HOSTNAMES
:   **Optional.** This parameter controls whether `gpinitsystem` uses IP addresses or host names in the `pg_hba.conf` file when updating the file with addresses that can connect to Greenplum Database. The default value is `0`, the utility uses IP addresses when updating the file. When initializing a Greenplum Database system, specify `HBA_HOSTNAMES=1` to have the utility use host names in the `pg_hba.conf` file.

:   For information about how Greenplum Database resolves host names in the `pg_hba.conf` file, see [Configuring Client Authentication](../../admin_guide/client_auth.html#topic1).

## <a id="spechosts"></a>Specifying Hosts using Hostnames or IP Addresses 

When initializing a Greenplum Database system with `gpinitsystem`, you can specify segment hosts using either hostnames or IP addresses. For example, you can use hostnames or IP addresses in the file specified with the `-h` option.

-   If you specify a hostname, the resolution of the hostname to an IP address should be done locally for security. For example, you should use entries in a local `/etc/hosts` file to map a hostname to an IP address. The resolution of a hostname to an IP address should not be performed by an external service such as a public DNS server. You must stop the Greenplum system before you change the mapping of a hostname to a different IP address.
-   If you specify an IP address, the address should not be changed after the initial configuration. When segment mirroring is enabled, replication from the primary to the mirror segment will fail if the IP address changes from the configured value. For this reason, you should use a hostname when initializing a Greenplum Database system unless you have a specific requirement to use IP addresses.

When initializing the Greenplum Database system, `gpinitsystem` uses the initialization information to populate the [gp\_segment\_configuration](../../ref_guide/system_catalogs/gp_segment_configuration.html) catalog table and adds hosts to the `pg_hba.conf` file. By default, the host IP address is added to the file. Specify the `gpinitsystem` configuration file parameter [HBA\_HOSTNAMES](#hba_hostnames)=1 to add hostnames to the file.

Greenplum Database uses the `address` value of the `gp_segment_configuration` catalog table when looking up host systems for Greenplum interconnect \(internal\) communication between the coordinator and segment instances and between segment instances, and for other internal communication.

## <a id="section6"></a>Examples 

Initialize a Greenplum Database system by supplying a cluster configuration file and a segment host address file, and set up a spread mirroring \(`--mirror-mode=spread`\) configuration:

```
$ gpinitsystem -c gpinitsystem_config -h hostfile_gpinitsystem --mirror-mode=spread
```

Initialize a Greenplum Database system and set the superuser remote password:

```
$ gpinitsystem -c gpinitsystem_config -h hostfile_gpinitsystem --su-password=mypassword
```

Initialize a Greenplum Database system with an optional standby coordinator host:

```
$ gpinitsystem -c gpinitsystem_config -h hostfile_gpinitsystem -s host09
```

Initialize a Greenplum Database system and write the provided configuration to an output file, for example `cluster_init.config`:

```
$ gpinitsystem -c gpinitsystem_config -h hostfile_gpinitsystem -O cluster_init.config
```

The output file uses the `QD_PRIMARY_ARRAY` and `PRIMARY_ARRAY` parameters to define coordinator and segment hosts:

```
TRUSTED_SHELL=ssh
CHECK_POINT_SEGMENTS=8
ENCODING=UNICODE
SEG_PREFIX=gpseg
HEAP_CHECKSUM=on
HBA_HOSTNAMES=0
QD_PRIMARY_ARRAY=cdw~cdw.local~5433~/data/coordinator1/gpseg-1~1~-1
declare -a PRIMARY_ARRAY=(
cdw~cdw.local~6001~/data/primary1/gpseg0~2~0
)
declare -a MIRROR_ARRAY=(
cdw~cdw.local~7001~/data/mirror1/gpseg0~3~0
)
```

Initialize a Greenplum Database using an input configuration file \(a file that defines the Greenplum Database cluster\) using `QD_PRIMARY_ARRAY` and `PRIMARY_ARRAY` parameters:

```
$ gpinitsystem -I cluster_init.config
```

The following example uses a host system configured with multiple NICs. If host systems are configured with multiple NICs, you can initialize a Greenplum Database system to use each NIC as a Greenplum host system. You must ensure that the host systems are configured with sufficient resources to support all the segment instances being added to the host. Also, if high availability is enabled, you must ensure that the Greenplum system configuration supports failover if a host system fails. For information about Greenplum Database mirroring schemes, see [../../best\_practices/ha.html\#topic\_ngz\_qf4\_tt](../../best_practices/ha.html#topic_ngz_qf4_tt).

For this simple coordinator and segment instance configuration, the host system `gp7c` is configured with two NICs `gp7c-1` and `gp7c-2`. In the configuration, the [QD\_PRIMARY\_ARRAY](#array_params) parameter defines the coordinator segment using `gp7c-1`. The [PRIMARY\_ARRAY](#array_params) and [MIRROR\_ARRAY](#array_params) parameters use `gp7c-2` to define a primary and mirror segment instance.

```
QD_PRIMARY_ARRAY=gp7c~gp7c-1~5432~/data/coordinator/gpseg-1~1~-1
declare -a PRIMARY_ARRAY=(
gp7c~gp7c-2~40000~/data/data1/gpseg0~2~0
gp7s~gp7s~40000~/data/data1/gpseg1~3~1
)
declare -a MIRROR_ARRAY=(
gp7s~gp7s~50000~/data/mirror1/gpseg0~4~0
gp7c~gp7c-2~50000~/data/mirror1/gpseg1~5~1
)
```

## <a id="section7"></a>See Also 

[gpssh-exkeys](gpssh-exkeys.html), [gpdeletesystem](gpdeletesystem.html), [Initializing Greenplum Database](../../install_guide/init_gpdb.html).

