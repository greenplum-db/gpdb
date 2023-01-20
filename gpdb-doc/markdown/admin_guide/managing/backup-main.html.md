---
title: Backing Up and Restoring Databases 
---

Performing backups regularly ensures that you can restore your data or rebuild your Greenplum Database system if data corruption or a system failure occurs. You can also use backups to migrate data from one Greenplum Database system to another. 

Greenplum Database supports parallel and non-parallel methods for backing up and restoring databases. Parallel operations scale regardless of the number of segments in your system, because segment hosts each write their data to local disk storage simultaneously. With non-parallel backup and restore operations, the data must be sent over the network from the segments to the coordinator, which writes all of the data to its storage. In addition to restricting I/O to one host, non-parallel backup requires that the coordinator have sufficient local disk storage to store the entire database.

## <a id="parback"></a>Parallel Backup with gpbackup and gprestore 

`gpbackup` and `gprestore` are the recommended Greenplum Database backup and restore utilities. `gpbackup` utilizes `ACCESS SHARE` locks at the individual table level, instead of `EXCLUSIVE` locks on the `pg_class` catalog table. This enables you to run DML statements during the backup, such as `CREATE`, `ALTER`, `DROP`, and `TRUNCATE` operations, as long as those operations do not target the current backup set. Backup files created with `gpbackup` are designed to provide future capabilities for restoring individual database objects along with their dependencies, such as functions and required user-defined datatypes.

`gpbackup`, `gprestore`, and related utilities are provided as a separate download, [VMware Greenplum® Backup and Restore](https://network.pivotal.io/products/pivotal-gpdb-backup-restore). Follow the instructions in the [VMware Greenplum Backup and Restore Documentation](https://docs.vmware.com/en/VMware-Tanzu-Greenplum-Backup-and-Restore/index.html) to install and use these utilities.

## <a id="nparback"></a>Non-Parallel Backup with pg\_dump 

The PostgreSQL `pg_dump` and `pg_dumpall` non-parallel backup utilities can be used to create a single dump file on the coordinator host that contains all data from all active segments.

The PostgreSQL non-parallel utilities should be used only for special cases. They are much slower than using the Greenplum backup utilities since all of the data must pass through the coordinator. Additionally, it is often the case that the coordinator host has insufficient disk space to save a backup of an entire distributed Greenplum database.

The `pg_restore` utility requires compressed dump files created by `pg_dump` or `pg_dumpall`. To perform a non-parallel restore using parallel backup files, you can copy the backup files from each segment host to the coordinator host, and then load them through the coordinator.

![Non-parallel Restore Using Parallel Backup Files](../graphics/nonpar_restore.jpg "Non-parallel Restore Using Parallel Backup Files")

Another non-parallel method for backing up Greenplum Database data is to use the `COPY TO` SQL command to copy all or a portion of a table out of the database to a delimited text file on the coordinator host.

**Parent topic:** [Managing a Greenplum System](../managing/partII.html)

