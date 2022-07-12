# Backing Up Databases with Data Domain Boost 

Dell EMC Data Domain Boost \(DD Boost\) is Dell EMC software that can be used with the `gpcrondump` and `gpdbrestore` utilities to perform faster backups to the Dell EMC Data Domain storage appliance. Data Domain performs deduplication on the data it stores, so after the initial backup operation, the appliance stores only pointers to data that is unchanged. This reduces the size of backups on disk. When DD Boost is used with `gpcrondump`, Greenplum Database participates in the deduplication process, reducing the volume of data sent over the network. When you restore files from the Data Domain system with Data Domain Boost, some files are copied to the master local disk and are restored from there, and others are restored directly.

With Data Domain Boost managed file replication, you can replicate Greenplum Database backup images that are stored on a Data Domain system for disaster recover purposes. The `gpmfr` utility manages the Greenplum Database backup sets that are on the primary and a remote Data Domain system. For information about `gpmfr`, see the *Greenplum Database Utility Guide*.

Managed file replication requires network configuration when a replication network is being used between two Data Domain systems:

-   The Greenplum Database system requires the Data Domain login credentials to be configured using `gpcrondump`. Credentials must be created for both the local and remote Data Domain systems.
-   When the non-management network interface is used for replication on the Data Domain systems, static routes must be configured on the systems to pass the replication data traffic to the correct interfaces.

Do not use Data Domain Boost with `pg_dump` or `pg_dumpall`.

Refer to Data Domain Boost documentation for detailed information.

**Important:** For incremental back up sets, a full backup and the associated incremental backups must be on a single device. For example, a backup set must all be on a file system. The backup set cannot have some backups on the local file system and others on single storage unit of a Data Domain system. For backups on a Data Domain system, the backup set must be in a single storage unit.

**Note:** You can use a Data Domain server as an NFS file system \(without Data Domain Boost\) to perform incremental backups.

**Parent topic:** [Parallel Backup with gpcrondump and gpdbrestore](../managing/backup-heading.html)

## Data Domain Boost Requirements 

Using Data Domain Boost requires the following.

-   Data Domain Boost is included only with the commercial release of Tanzu Greenplum.
-   Purchase and install Dell EMC Data Domain Boost and Replicator licenses on the Data Domain systems.
-   Obtain sizing recommendations for Data Domain Boost. Make sure the Data Domain system supports sufficient write and read streams for the number of segment hosts in your Greenplum cluster.

Contact your Dell EMC Data Domain account representative for assistance.

## One-Time Data Domain Boost Credential Setup 

There is a one-time process to set up credentials to use Data Domain Boost. Credential setup connects one Greenplum Database instance to one Data Domain instance. If you are using the `gpcrondump --replicate` option or DD Boost managed file replication capabilities for disaster recovery purposes, you must set up credentials for both the local and remote Data Domain systems.

To set up credentials, run `gpcrondump` with the following options:

```
--ddboost-host <ddboost_hostname> --ddboost-user <ddboost_user>
--ddboost-backupdir <backup_directory> --ddboost-storage-unit <storage_unit_ID>
```

The `--ddboost-storage-unit` is optional. If not specified, the storage unit ID is `GPDB`.

To remove credentials, run `gpcrondump` with the `--ddboost-config-remove` option.

To manage credentials for the remote Data Domain system that is used for backup replication, include the `--ddboost-remote` option with the other `gpcrondump` options. For example, the following options set up credentials for a Data Domain system that is used for backup replication. The system IP address is `192.0.2.230`, the user ID is `ddboostmyuser`, and the location for the backups on the system is `GPDB/gp_production`:

```
--ddboost-host 192.0.2.230 --ddboost-user ddboostmyuser
--ddboost-backupdir gp_production --ddboost-remote
```

For details, see `gpcrondump` in the *Greenplum Database Utility Guide*.

If you use two or more network connections to connect to the Data Domain system, use `gpcrondump` to set up the login credentials for the Data Domain hostnames associated with the network interfaces. To perform this setup for two network connections, run `gpcrondump` with the following options:

```
--ddboost-host <ddboost_hostname1>
--ddboost-host <ddboost_hostname2> --ddboost-user <ddboost_user>
--ddboost-backupdir <backup_directory>
```

### About DD Boost Credential Files 

The `gpcrondump` utility is used to schedule DD Boost backup operations. The utility is also used to set, change, or remove one-time credentials and a storage unit ID for DD Boost. The `gpcrondump`, `gpdbrestore`, and `gpmfr` utilities use the DD Boost credentials to access Data Domain systems. DD Boost information is stored in these files.

-   `DDBOOST_CONFIG` is used by `gpdbrestore` and `gpcrondump` for backup and restore operations with the Data Domain system. The `gpdbrestore` utility creates or updates the file when you specify Data Domain information with the `--ddboost-host` option.
-   `DDBOOST_MFR_CONFIG` is used by `gpmfr` for remote replication operations with the remote Data Domain system. The `gpdbrestore` utility creates or updates the file when you specify Data Domain information with the `--ddboost-host` option and `--ddboost-remote` option.

The configuration files are created in the current user \(`gpadmin`\) home directory on the Greenplum Database master and segment hosts. The path and file name cannot be changed. Information in the configuration files includes:

-   Data Domain host name or IP address
-   DD Boost user name
-   DD Boost password
-   Default Data Domain backup directory \(`DDBOOST_CONFIG` only\)
-   Data Domain storage unit ID: default is `GPDB` \(`DDBOOST_CONFIG` only\)
-   Data Domain default log level: default is `WARNING`
-   Data Domain default log size: default is `50`

Use the `gpcrondump` option `--ddboost-show-config` to display the current DD Boost configuration information from the Greenplum Database master configuration file. Specify the `--remote` option to display the configuration information for the remote Data Domain system.

### About Data Domain Storage Units 

When you use a Data Domain system to perform a backup, restore, or remote replication operation with the `gpcrondump`, `gpdbrestore`, or `gpmfr` utility, the operation uses a storage unit on a Data Domain system. You can specify the storage unit ID when you perform these operations:

-   When you set the DD Boost credentials with the `gpcrondump` utility `--ddboost-host` option.

    If you specify the `--ddboost-storage-unit` option, the storage unit ID is written to the Greenplum Database DD Boost configuration file `DDBOOST_CONFIG`. If the storage unit ID is not specified, the default value is `GPDB`.

    If you specify the `--ddboost-storage-unit` option and the `--ddboost-remote` option to set DD Boost credentials for the remote Data Domain server, the storage ID information is ignored. The storage unit ID in the `DDBOOST_CONFIG` file is the default ID that is used for remote replication operations.

-   When you perform a backup, restore, or remote replication operation with `gpcrondump`, `gpdbrestore`, or `gpmfr`.

    When you specify the `--ddboost-storage-unit` option, the utility uses the specified Data Domain storage unit for the operation. The value in the configuration file is not changed.


A Greenplum Database utility uses the storage unit ID based on this order of precedence from highest to lowest:

-   Storage unit ID specified with `--ddboost-storage-unit`
-   Storage unit ID specified in the configuration file
-   Default storage unit ID `GPDB`

Greenplum Database master and segment instances use a single storage unit ID when performing a backup, restore, or remote replication operation.

**Important:** The storage unit ID in the Greenplum Database master and segment host configuration files must be the same. The Data Domain storage unit is created by the `gpcrondump` utility from the Greenplum Database master host.

The following occurs if storage unit IDs are different in the master and segment host configuration files:

-   If all the storage units have not been created, the operation fails.
-   If all the storage units have been created, a backup operation completes. However, the backup files are in different storage units and a restore operation fails because a full set of backup files is not in a single storage unit.

When performing a full backup operation \(not an incremental backup\), the storage unit is created on the Data Domain system if it does not exist.

A storage unit is not created if these `gpcrondump` options are specified: `--incremental`, `--list-backup-file`, `--list-filter-tables`, `-o`, or `--ddboost-config-remove`.

Greenplum Database replication operations use the same storage unit ID on both systems. For example, if you specify the `--ddboost-storage-unit` option for `--replicate` or `--recover` through `gpmfr` or `--replicate` from `gpcrondump`, the storage unit ID applies to both local and remote Data Domain systems.

When performing a replicate or recover operation with `gpmfr`, the storage unit on the destination Data Domain system \(where the backup is being copied\) is created if it does not exist.

## Configuring Data Domain Boost for Greenplum Database 

After you set up credentials for Data Domain Boost on the Greenplum Database, perform the following tasks in Data Domain to allow Data Domain Boost to work with Greenplum Database:

-   [Configuring Distributed Segment Processing in Data Domain](#topic21)
-   [Configuring Advanced Load Balancing and Link Failover in Data Domain](#topic20b)
-   [Export the Data Domain Path to Greenplum Database Hosts](#topic23)

## Configuring Distributed Segment Processing in Data Domain 

Configure the distributed segment processing option on the Data Domain system. The configuration applies to all the Greenplum Database servers with the Data Domain Boost plug-in installed on them. This option is enabled by default, but verify that it is enabled before using Data Domain Boost backups:

```
# ddboost option show
```

To enable or disable distributed segment processing:

```
# ddboost option set distributed-segment-processing 
```

### Configuring Advanced Load Balancing and Link Failover in Data Domain 

If you have multiple network connections on a network subnet, you can create an interface group to provide load balancing and higher network throughput on your Data Domain system. When a Data Domain system on an interface group receives data from the media server clients, the data transfer is load balanced and distributed as separate jobs on the private network. You can achieve optimal throughput with multiple 10 GbE connections.

**Note:** To ensure that interface groups function properly, use interface groups only when using multiple network connections on the same networking subnet.

To create an interface group on the Data Domain system, create interfaces with the `net` command. If interfaces do not already exist, add the interfaces to the group, and register the Data Domain system with the backup application.

1.  Add the interfaces to the group:

    ```
    # ddboost ifgroup add interface 192.0.2.1
    # ddboost ifgroup add interface 192.0.2.2
    # ddboost ifgroup add interface 192.0.2.3
    # ddboost ifgroup add interface 192.0.2.4
    
    ```

    **Note:** You can create only one interface group and this group cannot be named.

2.  Select one interface on the Data Domain system to register with the backup application. Create a failover aggregated interface and register that interface with the backup application.

    **Note:** You do not have to register one of the `ifgroup` interfaces with the backup application. You can use an interface that is not part of the `ifgroup` to register with the backup application.

3.  Enable `ddboost` on the Data Domain system:

    ```
    # ddboost ifgroup enable
    ```

4.  Verify the Data Domain system configuration as follows:

    ```
    # ddboost ifgroup show config
    ```

    Results similar to the following are displayed.

    ```
    Interface
    -------------
    192.0.2.1
    192.0.2.2
    192.0.2.3
    192.0.2.4
    -------------
    
    ```


You can add or delete interfaces from the group at any time.

**Note:** Manage Advanced Load Balancing and Link Failover \(an interface group\) using the `ddboost ifgroup` command or from the **Enterprise Manager Data Management** \> **DD Boost** view.

## Export the Data Domain Path to Greenplum Database Hosts 

The commands and options in this topic apply to DDOS 5.0.x and 5.1.x. See the Data Domain documentation for details.

Use the following Data Domain commands to export the `/backup/ost` directory to a Greenplum Database host for Data Domain Boost backups.

```
# nfs add /backup/ost 192.0.2.0/24, 198.51.100.0/24 (insecure)
```

**Note:** The IP addresses refer to the Greenplum Database system working with the Data Domain Boost system.

### Create the Data Domain Login Credentials for the Greenplum Database Host 

Create a username and password for the host to access the DD Boost Storage Unit \(SU\) at the time of backup and restore:

```
# user add <user> [password <password>] [priv ]
```

## Backup Options for Data Domain Boost 

Specify the `gpcrondump` options to match the setup.

Data Domain Boost backs up files to a storage unit in the Data Domain system. Status and report files remain on the local disk. If needed, specify the Data Domain system storage unit with the `--ddboost-storage-unit` option. This DD Boost comand display the names of all storage units on a Data Domain system

```
ddboost storage-unit show
```

To configure Data Domain Boost to remove old backup directories before starting a backup operation, specify a `gpcrondump` backup expiration option:

-   The `-c` option clears all backup directories.
-   The `-o` option clears the oldest backup directory.

To remove the oldest dump directory, specify `gpcrondump --ddboost` with the `-o` option. For example, if your retention period is 30 days, use `gpcrondump --ddboost` with the `-o` option on day 31.

Use `gpcrondump --ddboost` with the `-c` option to clear out all the old dump directories in `db_dumps`. The `-c` option deletes all dump directories that are at least one day old.

## Using CRON to Schedule a Data Domain Boost Backup 

1.  Ensure the [One-Time Data Domain Boost Credential Setup](#topic19) is complete.
2.  Add the option `--ddboost` to the `gpcrondump` option:

    ```
    gpcrondump -x mydatabase -z -v --ddboost 
    ```

    If needed, specify the Data Domain system storage unit with the `--ddboost-storage-unit` option.


**Important:** Do not use compression with Data Domain Boost backups. The `-z` option turns backup compression off.

Some of the options available in `gpcrondump` have different implications when using Data Domain Boost. For details, see `gpcrondump` in the *Greenplum Database Utility Reference*.

## Restoring From a Data Domain System with Data Domain Boost 

1.  Ensure the [One-Time Data Domain Boost Credential Setup](backup-ddboost.html) is complete.
2.  Add the option `--ddboost` to the `gpdbrestore` command:

    ```
    $ gpdbrestore -t <backup_timestamp> -v -ddboost
    ```

    If needed, specify the Data Domain system storage unit with the `--ddboost-storage-unit` option.


**Note:** Some of the `gpdbrestore` options available have different implications when using Data Domain. For details, see `gpdbrestore` in the *Greenplum Database Utility Reference*.

