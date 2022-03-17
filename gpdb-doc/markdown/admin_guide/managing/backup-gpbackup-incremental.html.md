---
title: Creating and Using Incremental Backups with gpbackup and gprestore 
---

The `gpbackup` and `gprestore` utilities support creating incremental backups of append-optimized tables and restoring from incremental backups. An incremental backup backs up all specified heap tables and backs up append-optimized tables \(including append-optimized, column-oriented tables\) only if the tables have changed. For example, if a row of an append-optimized table has changed, the table is backed up. For partitioned append-optimized tables, only the changed leaf partitions are backed up.

Incremental backups are efficient when the total amount of data in append-optimized tables or table partitions that changed is small compared to the data that has not changed since the last backup.

An incremental backup backs up an append-optimized table only if one of the following operations was performed on the table after the last full or incremental backup:

-   `ALTER TABLE`
-   `DELETE`
-   `INSERT`
-   `TRUNCATE`
-   `UPDATE`
-   `DROP` and then re-create the table

To restore data from incremental backups, you need a complete incremental backup set.

-   **[About Incremental Backup Sets](../managing/backup-gpbackup-incremental.html)**  

-   **[Using Incremental Backups](../managing/backup-gpbackup-incremental.html)**  


**Parent topic:**[Parallel Backup with gpbackup and gprestore](../managing/backup-gpbackup.html)

## <a id="topic_kvf_mkr_t2b"></a>About Incremental Backup Sets 

An incremental backup set includes the following backups:

-   A full backup. This is the full backup that the incremental backups are based on.
-   The set of incremental backups that capture the changes to the database from the time of the full backup.

For example, you can create a full backup and then create three daily incremental backups. The full backup and all three incremental backups are the backup set. For information about using an incremental backup set, see [Example Using Incremental Backup Sets](#incr_backup_scenario).

When you create or add to an incremental backup set, `gpbackup` ensures that the backups in the set are created with a consistent set of backup options to ensure that the backup set can be used in a restore operation. For information about backup set consistency, see [Using Incremental Backups](#topic_btr_xfr_t2b).

When you create an incremental backup you include these options with the other `gpbackup` options to create a backup:

-   `--leaf-partition-data` - Required for all backups in the incremental backup set.
    -   Required when you create a full backup that will be the base backup for an incremental backup set.
    -   Required when you create an incremental backup.
-   `--incremental` - Required when you create an incremental backup.

    You cannot combine `--data-only` or `--metadata-only` with `--incremental`.

-   `--from-timestamp` - Optional. This option can be used with `--incremental`. The timestamp you specify is an existing backup. The timestamp can be either a full backup or incremental backup. The backup being created must be compatible with the backup specified with the `--from-timestamp` option.

    If you do not specify `--from-timestamp`, `gpbackup` attempts to find a compatible backup based on information in the `gpbackup` history file. See [Incremental Backup Notes](#incr_backup_notes).


**Parent topic:**[Creating and Using Incremental Backups with gpbackup and gprestore](../managing/backup-gpbackup-incremental.html)

## <a id="topic_btr_xfr_t2b"></a>Using Incremental Backups 

-   [Example Using Incremental Backup Sets](#incr_backup_scenario)
-   [Creating an Incremental Backup with gpbackup](#gpbackup_increment)
-   [Restoring from an Incremental Backup with gprestore](#gprestore_increment)
-   [Incremental Backup Notes](#incr_backup_notes)

When you add an incremental backup to a backup set, `gpbackup` ensures that the full backup and the incremental backups are consistent by checking these `gpbackup` options:

-   `--dbname` - The database must be the same.
-   `--backup-dir` - The directory must be the same. The backup set, the full backup and the incremental backups, must be in the same location.
-   `--single-data-file` - This option must be either specified or absent for all backups in the set.
-   `--plugin-config` - If this option is specified, it must be specified for all backups in the backup set. The configuration must reference the same plugin binary.
-   `--include-table-file`, `--include-schema`, or any other options that filter tables and schemas must be the same.

    When checking schema filters, only the schema names are checked, not the objects contained in the schemas.

-   `--no-compression` - If this option is specified, it must be specified for all backups in the backup set.

    If compression is used on the on the full backup, compression must be used on the incremental backups. Different compression levels are allowed for the backups in the backup set. For a backup, the default is compression level 1.


If you try to add an incremental backup to a backup set, the backup operation fails if the `gpbackup` options are not consistent.

For information about the `gpbackup` and `gprestore` utility options, see the [gpbackup](../../utility_guide/ref/gpbackup.html) and [gprestore](../../utility_guide/ref/gprestore.html) reference documentation.

### <a id="incr_backup_scenario"></a>Example Using Incremental Backup Sets 

Each backup has a timestamp taken when the backup is created. For example, if you create a backup on May 14, 2017, the backup file names contain `20170514hhmmss`. The hhmmss represents the time: hour, minute, and second.

This example assumes that you have created two full backups and incremental backups of the database *mytest*. To create the full backups, you used this command:

```
gpbackup --dbname mytest --backup-dir /mybackup --leaf-partition-data
```

You created incremental backups with this command:

```
gpbackup --dbname mytest --backup-dir /mybackup --leaf-partition-data --incremental
```

When you specify the `--backup-dir` option, the backups are created in the `/mybackup` directory on each Greenplum Database host.

In the example, the full backups have the timestamp keys `20170514054532` and `20171114064330`. The other backups are incremental backups. The example consists of two backup sets, the first with two incremental backups, and second with one incremental backup. The backups are listed from earliest to most recent.

-   `20170514054532` \(full backup\)
-   `20170714095512`
-   `20170914081205`
-   `20171114064330` \(full backup\)
-   `20180114051246`

To create a new incremental backup based on the latest incremental backup, you must include the same `--backup-dir` option as the incremental backup as well as the options `--leaf-partition-data` and `--incremental`.

```
gpbackup --dbname mytest --backup-dir /mybackup --leaf-partition-data --incremental
```

You can specify the `--from-timestamp` option to create an incremental backup based on an existing incremental or full backup. Based on the example, this command adds a fourth incremental backup to the backup set that includes `20170914081205` as an incremental backup and uses `20170514054532` as the full backup.

```
gpbackup --dbname mytest --backup-dir /mybackup --leaf-partition-data --incremental --from-timestamp 20170914081205
```

This command creates an incremental backup set based on the full backup `20171114064330` and is separate from the backup set that includes the incremental backup `20180114051246`.

```
gpbackup --dbname mytest --backup-dir /mybackup --leaf-partition-data --incremental --from-timestamp 20171114064330
```

To restore a database with the incremental backup `20170914081205`, you need the incremental backups `20120914081205` and `20170714095512`, and the full backup `20170514054532`. This would be the `gprestore` command.

```
gprestore --backup-dir /backupdir --timestamp 20170914081205
```

### <a id="gpbackup_increment"></a>Creating an Incremental Backup with gpbackup 

The `gpbackup` output displays the timestamp of the backup on which the incremental backup is based. In this example, the incremental backup is based on the backup with timestamp `20180802171642`. The backup `20180802171642` can be an incremental or full backup.

```
$ gpbackup --dbname test --backup-dir /backups --leaf-partition-data --incremental
20180803:15:40:51 gpbackup:gpadmin:mdw:002907-[INFO]:-Starting backup of database test
20180803:15:40:52 gpbackup:gpadmin:mdw:002907-[INFO]:-Backup Timestamp = 20180803154051
20180803:15:40:52 gpbackup:gpadmin:mdw:002907-[INFO]:-Backup Database = test
20180803:15:40:52 gpbackup:gpadmin:mdw:002907-[INFO]:-Gathering list of tables for backup
20180803:15:40:52 gpbackup:gpadmin:mdw:002907-[INFO]:-Acquiring ACCESS SHARE locks on tables
Locks acquired:  5 / 5 [================================================================] 100.00% 0s
20180803:15:40:52 gpbackup:gpadmin:mdw:002907-[INFO]:-Gathering additional table metadata
20180803:15:40:52 gpbackup:gpadmin:mdw:002907-[INFO]:-Metadata will be written to /backups/gpseg-1/backups/20180803/20180803154051/gpbackup_20180803154051_metadata.sql
20180803:15:40:52 gpbackup:gpadmin:mdw:002907-[INFO]:-Writing global database metadata
20180803:15:40:52 gpbackup:gpadmin:mdw:002907-[INFO]:-Global database metadata backup complete
20180803:15:40:52 gpbackup:gpadmin:mdw:002907-[INFO]:-Writing pre-data metadata
20180803:15:40:52 gpbackup:gpadmin:mdw:002907-[INFO]:-Pre-data metadata backup complete
20180803:15:40:52 gpbackup:gpadmin:mdw:002907-[INFO]:-Writing post-data metadata
20180803:15:40:52 gpbackup:gpadmin:mdw:002907-[INFO]:-Post-data metadata backup complete
20180803:15:40:52 gpbackup:gpadmin:mdw:002907-[INFO]:-Basing incremental backup off of backup with timestamp = 20180802171642
20180803:15:40:52 gpbackup:gpadmin:mdw:002907-[INFO]:-Writing data to file
Tables backed up:  4 / 4 [==============================================================] 100.00% 0s
20180803:15:40:52 gpbackup:gpadmin:mdw:002907-[INFO]:-Data backup complete
20180803:15:40:53 gpbackup:gpadmin:mdw:002907-[INFO]:-Found neither /usr/local/greenplum-db/./bin/gp_email_contacts.yaml nor /home/gpadmin/gp_email_contacts.yaml
20180803:15:40:53 gpbackup:gpadmin:mdw:002907-[INFO]:-Email containing gpbackup report /backups/gpseg-1/backups/20180803/20180803154051/gpbackup_20180803154051_report will not be sent
20180803:15:40:53 gpbackup:gpadmin:mdw:002907-[INFO]:-Backup completed successfully
```

### <a id="gprestore_increment"></a>Restoring from an Incremental Backup with gprestore 

When restoring an from an incremental backup, you can specify the `--verbose` option to display the backups that are used in the restore operation on the command line. For example, the following `gprestore` command restores a backup using the timestamp `20180807092740`, an incremental backup. The output includes the backups that were used to restore the database data.

```
$ gprestore --create-db --timestamp 20180807162904 --verbose
...
20180807:16:31:56 gprestore:gpadmin:mdw:008603-[INFO]:-Pre-data metadata restore complete
20180807:16:31:56 gprestore:gpadmin:mdw:008603-[DEBUG]:-Verifying backup file count
20180807:16:31:56 gprestore:gpadmin:mdw:008603-[DEBUG]:-Restoring data from backup with timestamp: 20180807162654
20180807:16:31:56 gprestore:gpadmin:mdw:008603-[DEBUG]:-Reading data for table public.tbl_ao from file (table 1 of 1)
20180807:16:31:56 gprestore:gpadmin:mdw:008603-[DEBUG]:-Checking whether segment agents had errors during restore
20180807:16:31:56 gprestore:gpadmin:mdw:008603-[DEBUG]:-Restoring data from backup with timestamp: 20180807162819
20180807:16:31:56 gprestore:gpadmin:mdw:008603-[DEBUG]:-Reading data for table public.test_ao from file (table 1 of 1)
20180807:16:31:56 gprestore:gpadmin:mdw:008603-[DEBUG]:-Checking whether segment agents had errors during restore
20180807:16:31:56 gprestore:gpadmin:mdw:008603-[DEBUG]:-Restoring data from backup with timestamp: 20180807162904
20180807:16:31:56 gprestore:gpadmin:mdw:008603-[DEBUG]:-Reading data for table public.homes2 from file (table 1 of 4)
20180807:16:31:56 gprestore:gpadmin:mdw:008603-[DEBUG]:-Reading data for table public.test2 from file (table 2 of 4)
20180807:16:31:56 gprestore:gpadmin:mdw:008603-[DEBUG]:-Reading data for table public.homes2a from file (table 3 of 4)
20180807:16:31:56 gprestore:gpadmin:mdw:008603-[DEBUG]:-Reading data for table public.test2a from file (table 4 of 4)
20180807:16:31:56 gprestore:gpadmin:mdw:008603-[DEBUG]:-Checking whether segment agents had errors during restore
20180807:16:31:57 gprestore:gpadmin:mdw:008603-[INFO]:-Data restore complete
20180807:16:31:57 gprestore:gpadmin:mdw:008603-[INFO]:-Restoring post-data metadata
20180807:16:31:57 gprestore:gpadmin:mdw:008603-[INFO]:-Post-data metadata restore complete
...
```

The output shows that the restore operation used three backups.

When restoring an from an incremental backup, `gprestore` also lists the backups that are used in the restore operation in the `gprestore` log file.

During the restore operation, `gprestore` displays an error if the full backup or other required incremental backup is not available.

### <a id="incr_backup_notes"></a>Incremental Backup Notes 

To create an incremental backup, or to restore data from an incremental backup set, you need the complete backup set. When you archive incremental backups, the complete backup set must be archived. You must archive all the files created on the master and all segments.

Each time `gpbackup` runs, the utility adds backup information to the history file `gpbackup_history.yaml` in the Greenplum Database master data directory. The file includes backup options and other backup information.

If you do not specify the `--from-timestamp` option when you create an incremental backup, `gpbackup` uses the most recent backup with a consistent set of options. The utility checks the backup history file to find the backup with a consistent set of options. If the utility cannot find a backup with a consistent set of options or the history file does not exist, `gpbackup` displays a message stating that a full backup must be created before an incremental can be created.

If you specify the `--from-timestamp` option when you create an incremental backup, `gpbackup` ensures that the options of the backup that is being created are consistent with the options of the specified backup.

The `gpbackup` option `--with-stats` is not required to be the same for all backups in the backup set. However, to perform a restore operation with the `gprestore` option `--with-stats` to restore statistics, the backup you specify must have must have used the `--with-stats` when creating the backup.

You can perform a restore operation from any backup in the backup set. However, changes captured in incremental backups later than the backup use to restore database data will not be restored.

When restoring from an incremental backup set, `gprestore` checks the backups and restores each append-optimized table from the most recent version of the append-optimized table in the backup set and restores the heap tables from the latest backup.

The incremental back up set, a full backup and associated incremental backups, must be on a single device. For example, the backups in a backup set must all be on a file system or must all be on a Data Domain system.

If you specify the `gprestore` option `--incremental` to restore data from a specific incremental backup, you must also specify the `--data-only` option. Before performing the restore operation, `gprestore` ensures that the tables being restored exist. If a table does not exist, `gprestore` returns an error and exits.

**Warning:** Changes to the Greenplum Database segment configuration invalidate incremental backups. After you change the segment configuration \(add or remove segment instances\), you must create a full backup before you can create an incremental backup.

**Parent topic:**[Creating and Using Incremental Backups with gpbackup and gprestore](../managing/backup-gpbackup-incremental.html)

