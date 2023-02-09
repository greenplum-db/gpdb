# gpexpand 

Expands an existing Greenplum Database across new hosts in the system.

## <a id="section2"></a>Synopsis 

```
gpexpand [{-f|--hosts-file} <hosts_file>]
        | {-i|--input} <input_file> [-B <batch_size>]
        | [{-d | --duration} <hh:mm:ss> | {-e|--end} '<YYYY-MM-DD hh:mm:ss>'] 
          [-a|-analyze] 
          [-n  <parallel_processes>]
        | {-r|--rollback}
        | {-c|--clean}
        [-v|--verbose] [-s|--silent]
        [{-t|--tardir} <directory> ]
        [-S|--simple-progress ]

gpexpand -? | -h | --help 

gpexpand --version
```

## <a id="section3"></a>Prerequisites 

-   You are logged in as the Greenplum Database superuser \(`gpadmin`\).
-   The new segment hosts have been installed and configured as per the existing segment hosts. This involves:
    -   Configuring the hardware and OS
    -   Installing the Greenplum software
    -   Creating the `gpadmin` user account
    -   Exchanging SSH keys.
-   Enough disk space on your segment hosts to temporarily hold a copy of your largest table.
-   When redistributing data, Greenplum Database must be running in production mode. Greenplum Database cannot be running in restricted mode or in coordinator mode. The `gpstart` options `-R` or `-m` cannot be specified to start Greenplum Database.

> **Note** These utilities cannot be run while `gpexpand` is performing segment initialization.

-   `gpbackup`
-   `gpcheckcat`
-   `gpconfig`
-   `gppkg`
-   `gprestore`

> **Important** When expanding a Greenplum Database system, you must deactivate Greenplum interconnect proxies before adding new hosts and segment instances to the system, and you must update the `gp_interconnect_proxy_addresses` parameter with the newly-added segment instances before you re-enable interconnect proxies. For information about Greenplum interconnect proxies, see [Configuring Proxies for the Greenplum Interconnect](../../admin_guide/managing/proxy-ic.html).

For information about preparing a system for expansion, see [Expanding a Greenplum System](../../admin_guide/expand/expand-main.html)in the *Greenplum Database Administrator Guide*.

## <a id="section4"></a>Description 

The `gpexpand` utility performs system expansion in two phases: segment instance initialization and then table data redistribution.

In the initialization phase, `gpexpand` runs with an input file that specifies data directories, dbid values, and other characteristics of the new segment instances. You can create the input file manually, or by following the prompts in an interactive interview.

If you choose to create the input file using the interactive interview, you can optionally specify a file containing a list of expansion system hosts. If your platform or command shell limits the length of the list of hostnames that you can type when prompted in the interview, specifying the hosts with `-f` may be mandatory.

In addition to initializing the segment instances, the initialization phase performs these actions:

-   Creates an expansion schema named *gpexpand* in the postgres database to store the status of the expansion operation, including detailed status for tables.

In the table data redistribution phase, `gpexpand` redistributes table data to rebalance the data across the old and new segment instances.

> **Note** Data redistribution should be performed during low-use hours. Redistribution can be divided into batches over an extended period.

To begin the redistribution phase, run `gpexpand` with either the `-d` \(duration\) or `-e` \(end time\) options, or with no options. If you specify an end time or duration, then the utility redistributes tables in the expansion schema until the specified end time or duration is reached. If you specify no options, then the utility redistribution phase continues until all tables in the expansion schema are reorganized. Each table is reorganized using `ALTER TABLE` commands to rebalance the tables across new segments, and to set tables to their original distribution policy. If `gpexpand` completes the reorganization of all tables, it displays a success message and ends.

> **Note** This utility uses secure shell \(SSH\) connections between systems to perform its tasks. In large Greenplum Database deployments, cloud deployments, or deployments with a large number of segments per host, this utility may exceed the host's maximum threshold for unauthenticated connections. Consider updating the SSH `MaxStartups` and `MaxSessions` configuration parameters to increase this threshold. For more information about SSH configuration options, refer to the SSH documentation for your Linux distribution.

## <a id="section5"></a>Options 

-a \| --analyze
:   Run `ANALYZE` to update the table statistics after expansion. The default is to not run `ANALYZE`.

-B batch\_size
:   Batch size of remote commands to send to a given host before making a one-second pause. Default is `16`. Valid values are 1-128.

:   The `gpexpand` utility issues a number of setup commands that may exceed the host's maximum threshold for unauthenticated connections as defined by `MaxStartups` in the SSH daemon configuration. The one-second pause allows authentications to be completed before `gpexpand` issues any more commands.

:   The default value does not normally need to be changed. However, it may be necessary to reduce the maximum number of commands if `gpexpand` fails with connection errors such as `'ssh_exchange_identification: Connection closed by remote host.'`

-c \| --clean
:   Remove the expansion schema.

-d \| --duration hh:mm:ss
:   Duration of the expansion session from beginning to end.

-e \| --end 'YYYY-MM-DD hh:mm:ss'
:   Ending date and time for the expansion session.

-f \| --hosts-file filename
:   Specifies the name of a file that contains a list of new hosts for system expansion. Each line of the file must contain a single host name.

:   This file can contain hostnames with or without network interfaces specified. The `gpexpand` utility handles either case, adding interface numbers to end of the hostname if the original nodes are configured with multiple network interfaces.

    > **Note** The Greenplum Database segment host naming convention is `sdwN` where `sdw` is a prefix and `N` is an integer. For example, `sdw1`, `sdw2` and so on. For hosts with multiple interfaces, the convention is to append a dash \(`-`\) and number to the host name. For example, `sdw1-1` and `sdw1-2` are the two interface names for host `sdw1`.

:   For information about using a hostname or IP address, see [Specifying Hosts using Hostnames or IP Addresses](#host_ip). Also, see [Using Host Systems with Multiple NICs](#multi_nic).

-i \| --input input\_file
:   Specifies the name of the expansion configuration file, which contains one line for each segment to be added in the format of:

:   hostname\|address\|port\|datadir\|dbid\|content\|preferred\_role

-n parallel\_processes
:   The number of tables to redistribute simultaneously. Valid values are 1 - 96.

:   Each table redistribution process requires two database connections: one to alter the table, and another to update the table's status in the expansion schema. Before increasing `-n`, check the current value of the server configuration parameter `max_connections` and make sure the maximum connection limit is not exceeded.

-r \| --rollback
:   Roll back a failed expansion setup operation.

-s \| --silent
:   Runs in silent mode. Does not prompt for confirmation to proceed on warnings.

-S \| --simple-progress
:   If specified, the `gpexpand` utility records only the minimum progress information in the Greenplum Database table *gpexpand.expansion\_progress*. The utility does not record the relation size information and status information in the table *gpexpand.status\_detail*.

:   Specifying this option can improve performance by reducing the amount of progress information written to the *gpexpand* tables.

\[-t \| --tardir\] directory
:   The fully qualified path to a directory on segment hosts where the `gpexpand` utility copies a temporary tar file. The file contains Greenplum Database files that are used to create segment instances. The default directory is the user home directory.

-v \| --verbose
:   Verbose debugging output. With this option, the utility will output all DDL and DML used to expand the database.

--version
:   Display the utility's version number and exit.

-? \| -h \| --help
:   Displays the online help.

## <a id="host_ip"></a>Specifying Hosts using Hostnames or IP Addresses 

When expanding a Greenplum Database system, you can specify either a hostname or an IP address for the value.

-   If you specify a hostname, the resolution of the hostname to an IP address should be done locally for security. For example, you should use entries in a local `/etc/hosts` file to map a hostname to an IP address. The resolution of a hostname to an IP address should not be performed by an external service such as a public DNS server. You must stop the Greenplum system before you change the mapping of a hostname to a different IP address.
-   If you specify an IP address, the address should not be changed after the initial configuration. When segment mirroring is enabled, replication from the primary to the mirror segment will fail if the IP address changes from the configured value. For this reason, you should use a hostname when expanding a Greenplum Database system unless you have a specific requirement to use IP addresses.

When expanding a Greenplum system, `gpexpand` populates [gp\_segment\_configuration](../../ref_guide/system_catalogs/gp_segment_configuration.html) catalog table with the new segment instance information. Greenplum Database uses the `address` value of the `gp_segment_configuration` catalog table when looking up host systems for Greenplum interconnect \(internal\) communication between the coordinator and segment instances and between segment instances, and for other internal communication.

## <a id="multi_nic"></a>Using Host Systems with Multiple NICs 

If host systems are configured with multiple NICs, you can expand a Greenplum Database system to use each NIC as a Greenplum host system. You must ensure that the host systems are configured with sufficient resources to support all the segment instances being added to the host. Also, if you enable segment mirroring, you must ensure that the expanded Greenplum system configuration supports failover if a host system fails. For information about Greenplum Database mirroring schemes, see [../../best\_practices/ha.html\#topic\_ngz\_qf4\_tt](../../best_practices/ha.html#topic_ngz_qf4_tt).

For example, this is a `gpexpand` configuration file for a simple Greenplum system. The segment host `gp7s1` and `gp7s2` are configured with two NICs, `-s1` and `-s2`, where the Greenplum Database system uses each NIC as a host system.

```
gp7s1-s2|gp7s1-s2|40001|/data/data1/gpseg2|6|2|p
gp7s2-s1|gp7s2-s1|50000|/data/mirror1/gpseg2|9|2|m
gp7s2-s1|gp7s2-s1|40000|/data/data1/gpseg3|7|3|p
gp7s1-s2|gp7s1-s2|50001|/data/mirror1/gpseg3|8|3|m
```

## <a id="section6"></a>Examples 

Run `gpexpand` with an input file to initialize new segments and create the expansion schema in the postgres database:

```
$ gpexpand -i input_file
```

Run `gpexpand` for sixty hours maximum duration to redistribute tables to new segments:

```
$ gpexpand -d 60:00:00
```

## <a id="section7"></a>See Also 

[gpssh-exkeys](gpssh-exkeys.html), [Expanding a Greenplum System](../../admin_guide/expand/expand-main.html)

