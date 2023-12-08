---
title: Migrating VMware Greenplum from Enterprise Linux 7 to 8
---

Use this procedure to migrate a VMware Greenplum Database installation from Enterprise Linux (EL) version 7 to Enterprise Linux 8, while maintaining your existing version of Greenplum Database.

Enterprise Linux includes CentOS, Rocky, Redhat (RHEL), and Oracle Linux (OEL) as the variants supported by Greenplum. See [Platform Requirements](platform-requirements-overview.md.hbs) for a list of the supported operating systems.

Major version upgrades of Linux operating systems are always a complex task in a Greenplum environment. You must weigh the risks of the different upgrade methods, as well as consider the impact of the required downtime.

## <a id="glib"></a>Important Upgrade Considerations

The GNU C Library, commonly known as `glibc`, is the GNU Project's implementation of the C standard library. Between EL 7 and 8, the version of `glibc` changes from 2.17 to 2.28. This is a major change that impacts many languages and their collations. The collation of a database specifies how to sort and compare strings of character data. A change in sorting for common languages can have a significant impact on PostgreSQL and Greenplum databases.

PostgreSQL and Greenplum databases use locale data provided by the operating system’s C library for sorting text. Sorting happens in a variety of contexts, including for user output, merge joins, B-tree indexes, and range partitions. In the latter two cases, sorted data is persisted to disk. If the locale data in the C library changes during the lifetime of a database, the persisted data may become inconsistent with the expected sort order, which could lead to erroneous query results and other incorrect behavior. 

If an index is not sorted in a way that an index scan is expecting it, a query could fail to find data, and an update could insert duplicate data. Similarly, in a partitioned table, a query could look in the wrong partition and an update could write to the wrong partition. It is essential to the correct operation of a database that you are aware of and understand any locale definition changes. Below are examples of the impact from locale changes in an EL 7 to EL 8 upgrade:

**Example 1** A range-partitioned table using default partitions displaying the rows in an incorrect order after an upgrade:

```
CREATE TABLE partition_range_test_3(id int, date text) DISTRIBUTED BY (id)
PARTITION BY RANGE (date)
      (
        PARTITION jan START ('01') INCLUSIVE,
        PARTITION feb START ('"02"') INCLUSIVE,
        PARTITION mar START ('"03"') INCLUSIVE );

INSERT INTO partition_range_test_3 VALUES (1, '01'), (1, '"01"'), (1, '"02"'), (1, '02'), (1, '03'), (1, '"03"'), (1, '04'), (1, '"04"');
```

Results for EL 7:

```
# SELECT * FROM partition_range_test_3 ORDER BY date;
 id | date
----+------
  1 | "01"
  1 | 01
  1 | "02"
  1 | 02
  1 | "03"
  1 | 03
  1 | "04"
  1 | 04
(8 rows)

# SELECT * FROM partition_range_test_3_1_prt_jan;
 id | date
----+------
  1 | 01
  1 | "01"
  1 | 02
(3 rows)

# SELECT * FROM partition_range_test_3_1_prt_feb;
 id | date
----+------
  1 | "02"
  1 | 03
(2 rows)
```

After upgrading to EL 8:

```
# SELECT * FROM partition_range_test_3 WHERE date='03';
 id | date
----+------
(0 rows)

=# EXPLAIN SELECT * FROM partition_range_test_3 WHERE date='03';
                                           QUERY PLAN
------------------------------------------------------------------------------------------------
 Gather Motion 4:1  (slice1; segments: 4)  (cost=0.00..720.00 rows=50 width=36)
   ->  Append  (cost=0.00..720.00 rows=13 width=36)
         ->  Seq Scan on partition_range_test_3_1_prt_mar  (cost=0.00..720.00 rows=13 width=36)
               Filter: (date = '03'::text)
 Optimizer: Postgres query optimizer
(5 rows)

# SELECT * FROM partition_range_test_3_1_prt_feb;
 id | date
----+------
  1 | "02"
  1 | 03
(2 rows)
```

**Example 2** A range-partitioned table not using a default partition encountering errors after the upgrade.

```
CREATE TABLE partition_range_test_2 (id int, date text) DISTRIBUTED BY (id)
PARTITION BY RANGE (date)
      (PARTITION Jan START ( '01') INCLUSIVE ,
      PARTITION Feb START ( '02') INCLUSIVE ,
      PARTITION Mar START ( '03') INCLUSIVE
      END ( '04') EXCLUSIVE);

INSERT INTO partition_range_test_2 VALUES (1, '01'), (1, '"01"'), (2, '"02"'), (2, '02'), (3, '03'), (3, '"03"');
```

Results for EL 7:

```
# SELECT * FROM partition_range_test_2 ORDER BY date;
id | date
----+------
  1 | 01
  1 | "01"
  2 | 02
  2 | "02"
  3 | 03
  3 | "03"
```

After upgrading to EL 8:

```
# SELECT * FROM partition_range_test_2 ORDER BY date;
id | date
----+------
  1 | 01
  2 | "02"
  2 | 02
  3 | "03"
  3 | 03
(5 rows)

# INSERT INTO partition_range_test_2 VALUES (1, '"01"');
ERROR:  no partition of relation "partition_range_test_2" found for row  (seg1 10.80.0.2:7003 pid=40499)
DETAIL:  Partition key of the failing row contains (date) = ("01").
```

You must take the following into consideration when planning an upgrade from EL 7 to EL 8:

- When using an in-place upgrade method, all indexes involving columns of collatable data type, such as `text`, `varchar`, `char`, and `citext`, must be reindexed before the database instance is put into production.
- When using an in-place upgrade method, range-partitioned tables using collatable data types in the partition key should be checked to verify that all rows are still in the correct partitions.
- To avoid downtime due to reindexing or repartitioning, consider upgrading using Greenplum Copy or Greenplum Backup and Restore instead of an in-place upgrade.
- When using an in-place upgrade method, databases or table columns using the `C` or `POSIX` locales are not affected. All other locales are potentially affected.

## <a id="methods"></a>Upgrade Methods

The following methods are the currently supported options to perform a major version upgrade from EL 7 to EL 8 with Greenplum Database.

- Using Greenplum Copy Utility to copy from Greenplum on EL 7 to a separate Greenplum on EL 8.
- Using Greenplum Backup and Restore to restore a backup taken from Greenplum on EL 7 to a separate Greenplum on EL 8.
- Using operating system vendor supported utilities, such as `leapp` to perform an in-place, simultaneous upgrade of EL 7 to EL 8 for all Greenplum hosts in a cluster then following the required post upgrade steps.

> *Note* Greenplum does not support a rolling upgrade, such that some Greenplum Segment Hosts are operating with EL 7 and others with EL 8. All Segment Hosts must be upgraded together or otherwise before Greenplum is started and workload continued after an upgrade.

### <a id="gpcopy"></a>Greenplum Copy Utility

The [Greenplum Copy Utility](https://docs.vmware.com/en/VMware-Greenplum-Data-Copy-Utility/index.html) is a utility for transferring data between databases in different Greenplum Database systems. 

This utility is compatible with the Greenplum Database cluster from the source and destination running on different operating systems, including EL 7 to EL 8. The `glibc` changes are not relevant for this migration method because the data is rewritten on copy to the target cluster, which addresses any locale sorting changes. However, since Greenplum Copy enables the option `-parallelize-leaf-partitions` by default, which copies the leaf partition tables of a partitioned table in parallel, it may lead to data being copied to an incorrect partition caused by the `glibc` changes. You must disable this option so that the table is copied as one single table based on the root partition table. 

As part of the overall process of this upgrade method, you:

- Create a new Greenplum cluster using EL 8 with no data.
- Address any [Operating System Configuration Differences](#os_config).
- Use `gpcopy` to migrate data from the source Greenplum cluster on EL 7 to the destination Greenplum cluster on EL 8. You must disable the option `-parallelize-leaf-partitions` to ensure that partitioned tables are copied as one single table based on the root partition. 
- Remove the source Greenplum cluster from the EL 7 systems.

The advantages of this method are optimized performance, migration issues not impacting the source cluster, and that it does not require table locks. The disadvantage of this method is that it requires two separate Greenplum clusters during the migration.

### <a id="gpbackup"></a>Greenplum Backup and Restore

[Greenplum Backup and Restore](https://docs.vmware.com/en/VMware-Greenplum-Backup-and-Restore/index.html) supports parallel and non-parallel methods for backing up and restoring databases. 

The utility is compatible with the Greenplum Database cluster from the source and destination running on different operating systems, including EL 7 to EL 8. The `glibc` changes are not relevant for this migration method because the data is rewritten on the new cluster, which addresses any locale sorting changes. However, if the backup command includes the option `--leaf-partition-data`, it creates one data file per leaf partition, instead of one data file for the entire table. In this situation, when you restore the partition data to the upgraded cluster, the utility copies the data directly into the leaf partitions, which may lead to data being copied into an incorrect partition caused by the `glibc` changes. Therefore, you must ensure that the backup command does not use the option `--leaf-partition-data` so partitioned tables are copied as a single data file.

Greenplum Backup and Restore supports many different options for storage locations, including local, public cloud storage such as S3, and Dell EMC Data Domain through the use of the [gpbackup storage plugins](https://docs.vmware.com/en/VMware-Greenplum-Backup-and-Restore/1.29/greenplum-backup-and-restore/admin_guide-managing-backup-plugins.html). Any of the supported options for storage locations to perform the data transfer are supported for the EL 7 to EL 8 upgrade.

As part of the overall process of this upgrade method, you:

- Create a new Greenplum cluster on the EL 8 systems with no data.
- Address any [Operating System Configuration Differences](#os_config).
- Use `gpbackup` to take a full backup of the source Greenplum cluster on EL 7. Ensure that you are not using the option `--leaf-partition-data`.
- Restore the backup with `gprestore` to the destination Greenplum cluster on EL 8.
- Remove the source Greenplum cluster on the EL 7 systems.

The advantages of this method are different options for storage locations, and migration issues not impacting the source cluster. The disadvantage of this method is that it requires two separate Greenplum clusters during the migration. It is also generally slower than Greenplum Copy, and it requires table locks to perform a full backup.

### <a id="simultaneous"></a>Simultaneous, In-Place Upgrade

Redhat and Oracle Linux both support options for in-place upgrade of the operating system using the Leapp utility. 

> **Note** In-Place upgrades with the Leapp utility are not supported with Rocky or CentOS Linux. You must use Greenplum Copy or Greenplum Backup and Restore instead.

Greenplum Database includes the `el8_migrate_locale.py` utility which helps you identify and address the main challenges associated with an in-place upgrade from EL 7 to 8 caused by the `glibc` GNU C library changes.

As part of the overall process of this upgrade method, you:

- Run the `el8_migrate_locale.py` utility to perform pre-check scripts, these scripts report information on any objects whose data the upgrade might affect.
- Stop the Greenplum cluster and use Leapp to run an in-place upgrade of the operating system.
- Address any required operating system configuration differences and start the Greenplum cluster.
- Follow the required steps given by the `el8_migrate_locale.py` utility for fixing the data that is impacted by the `glibc` locale sorting changes.

The advantage of this method is that it does not require two different Greenplum clusters. The disadvantages are the risk of performing an in-place operating system upgrade, no downgrade options after any issues, the risk of issues that could leave your cluster in a non-operating state, and the requirement of additional steps after the upgrade is complete to address the `glibc` changes. You must also plan downtime of your Greenplum database for the entire process.

Continue reading for a detailed list of steps to upgrade your cluster using this method.

> **Important** We recommend you take a backup of your cluster before proceeding with this method, as you will not be able to recover the database if the upgrade does not complete successfully. You may also be prepared to contact your operating system vendor for any issues encountered with the Leapp utility.

#### <a id="precheck"></a>Run the Pre-Check Script

Before you begin the upgrade, run the following commands:

```
python el8_migrate_locale.py precheck-index --out index.out
python el8_migrate_locale.py precheck-table --pre_upgrade --out table.out
```

The subcommand `precheck-index` checks each database for indexes involving columns of type `text`, `varchar`, `char`, and `citext`, and the subcommand `precheck-table` checks each database for range-partitioned tables using these types in the partition key. The option `--pre_upgrade` lists the partition tables with the partition key using built-in collatable types.

Examine the output files to identify which indexes and range-partitioned tables are affected by the `glibc` GNU C library changes. The provided information will help you estimate the amount of work required during the upgrade process.

In order to address the issues caused to the range-partitioned tables, the utility rebuilds the affected tables at a later step. This can result in additional space requirements for your database, so you must account for additional database space.

#### <a id="upgrade"></a>Perform the Upgrade

Stop the Greenplum Database cluster and use the Leapp utility to run the in-place upgrade for your operating system. Visit the [Redhat Documentation](https://access.redhat.com/articles/4263361) and the [Oracle Documentation](https://docs.oracle.com/en/operating-systems/oracle-linux/8/leapp/#Oracle-Linux-8) for more information on how to use the utility.

Once the upgrade is complete, address any [Operating System Configuration Differences](#os_config), and start the Greenplum Database cluster.

#### <a id="fix"></a>Fix the Impacted Data

##### <a id="indexes"></a>Indexes

You must reindex all indexes involving columns of collatable data types (`text`, `varchar`, `char`, and `citext`) before the database instance is put into production. 

Run the `el8_migrate_locale.py` utility with the `migrate` subcommand to reindex the necessary indexes.

```
python el8_migrate_locale.py migrate --input index.out
```

##### <a id="rangepart"></a>Range-Partitioned Tables

You must check range-partitioned tables that use collatable data types in the partition key to verify that all rows are still in the correct partitions. 

First, run the `el8_migrate_locale.py` utility with the `precheck-table` subcommand in order to verify if the rows are still in the correct partitions after the operating system upgrade. 

```
python el8_migrate_locale.py precheck-table --out table.out
```

The utility returns the list of range-partitioned tables whose rows have been affected. Run the utility using the `migrate` subcommand to rebuild the partitions that have their rows in an incorrect order after the upgrade. 

```
python el8_migrate_locale.py migrate --input table.out
```

#### <a id="postfix"></a>Verify the Changes

Run the pre-upgrade scripts again for each database to verify that all required changes in the database have been addressed.

```
python el8_migrate_locale.py precheck-index --out index.out
python el8_migrate_locale.py precheck-table --out table.out
```

If the utility returns no indexes nor tables, you have successfully addressed all the issues in your Greenplum Database cluster caused by the `glibc` GNU C library changes.

## <a id="os_config"></a>Operating System Configuration Differences

When you prepare your operating system environment for Greenplum Database software installation, there are different configuration options depending on the version of your operating system. See [Configuring Your Systems](install_guide-prep_os.html) and [Using Resource Groups](../admin_guide/workload_mgmt_resgroups.html#topic71717999) for detailed documentation. This section summarizes the main differences to take into consideration when you upgrade from EL 7 to EL 8 regardless of the upgrade method you use.

### <a id="xfs"></a>XFS Mount Options

XFS is the preferred data storage file system on Linux platforms. Use the mount command with the following recommended XFS mount options. The `nobarrier` option is not supported on EL 8 or Ubuntu systems. Use only the options `rw,nodev,noatime,inode64`.

### <a id="diskio"></a>Disk I/O Settings

The Linux disk scheduler orders the I/O requests submitted to a storage device, controlling the way the kernel commits reads and writes to disk. A typical Linux disk I/O scheduler supports multiple access policies. The optimal policy selection depends on the underlying storage infrastructure. For EL 8, use the following recommended scheduler policy:

|Storage Device Type|Recommended Scheduler Policy|
|------|---------------|
|Non-Volatile Memory Express (NVMe)|none|
|Solid-State Drives (SSD)|none|
|Other|mq-deadline|

To specify the I/O scheduler at boot time for EL 8 you must either use TuneD or uDev rules. See the [Redhat Documentation](https://access.redhat.com/documentation/en-us/red_hat_enterprise_linux/8/html/monitoring_and_managing_system_status_and_performance/setting-the-disk-scheduler_monitoring-and-managing-system-status-and-performance) for full details.

### <a id="ntp"></a>Synchronizing System Clocks

You must use NTP (Network Time Protocol) to synchronize the system clocks on all hosts that comprise your Greenplum Database system. Accurate time keeping is essential to ensure reliable operations on the database and data integrity. You may either configure the master as the NTP primary source and the other hosts in the cluster connect to it, or configure an external NTP primary source and all hosts in the cluster connect to it. For EL 8, use the Chrony service to configure NTP. 

### <a id="resgroups"></a>Configuring and Using Resource Groups

Greenplum Database resource groups use Linux Control Groups (cgroups) to manage CPU resources. Greenplum Database also uses cgroups to manage memory for resource groups for external components. With cgroups, Greenplum isolates the CPU and external component memory usage of your Greenplum processes from other processes on the node. This allows Greenplum to support CPU and external component memory usage restrictions on a per-resource-group basis.

If you are using Redhat 8.x, make sure that you configured the system to mount the `cgroups-v1` filesystem by default during system boot. See [Using Resource Groups](../admin_guide/workload_mgmt_resgroups.html#topic71717999) for more details. 

