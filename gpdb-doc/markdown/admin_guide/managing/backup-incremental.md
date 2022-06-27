# Creating Incremental Backups 

The `gpcrondump` and `gpdbrestore` utilities support incremental backups and restores of append-optimized tables, including column-oriented tables. Use the `gpcrondump` option `--incremental` to create an incremental backup.

An incremental backup only backs up an append-optimized or column-oriented table if one of the following operations was performed on the table after the last full or incremental backup:

-   `ALTER TABLE`
-   `DELETE`
-   `INSERT`
-   `TRUNCATE`
-   `UPDATE`
-   `DROP` and then re-create the table

For partitioned append-optimized tables, only the changed partitions are backed up.

Heap tables are backed up with every full and incremental backup.

Incremental backups are efficient when the total amount of data in append-optimized table partitions or column-oriented tables that changed is small compared to the data has not changed.

Each time `gpcrondump` runs, it creates state files that contain row counts for each append optimized and column-oriented table and partition in the database. The state files also store metadata operations such as truncate and alter. When `gpcrondump` runs with the `--incremental` option, it compares the current state with the stored state to determine if the table or partition should be included in the incremental backup.

A unique 14-digit timestamp key identifies files that comprise an incremental backup set.

To create an incremental backup or to restore data from an incremental backup, you need the complete backup set. A complete backup set consists of a full backup and any incremental backups that were created since the last full backup. When you archive incremental backups, all incremental backups between the last full backup and the target incremental backup must be archived. You must archive all the files created on the master and all segments.

**Important:** For incremental back up sets, a full backup and associated incremental backups, the backup set must be on a single device. For example, a backup set must all be on a Data Domain system. The backup set cannot have some backups on a Data Domain system and others on the local file system or a NetBackup system.

**Note:** You can use a Data Domain server as an NFS file system \(without Data Domain Boost\) to perform incremental backups.

Changes to the Greenplum Database segment configuration invalidate incremental backups. After you change the segment configuration you must create a full backup before you can create an incremental backup.

## Incremental Backup Example 

Each backup set has a key, which is a timestamp taken when the backup is created. For example, if you create a backup on May 14, 2016, the backup set file names contain `20160514hhmmss`. The hhmmss represents the time: hour, minute, and second.

For this example, assume you have created both full and incremental backups of the database *mytest*. To create the full backup, you used the following command:

```
gpcrondump -x mytest -u /backupdir 
```

Later, after some changes have been made to append optimized tables, you created an increment backup with the following command:

```
gpcrondump -x mytest -u /backupdir --incremental
```

When you specify the `-u` option, the backups are created in the `/backupdir` directory on each Greenplum Database host. The file names include the following timestamp keys. The full backups have the timestamp key `20160514054532` and `20161114064330`. The other backups are incremental backups.

-   `20160514054532` \(full backup\)
-   `20160714095512`
-   `20160914081205`
-   `20161114064330` \(full backup\)
-   `20170114051246`

To create a new incremental backup, you need both the most recent incremental backup `20170114051246` and the preceding full backup `20161114064330`. Also, you must specify the same `-u` option for any incremental backups that are part of the backup set.

To restore a database with the incremental backup `20160914081205`, you need the incremental backups `20160914081205` and `20160714095512`, and the full backup `20160514054532`.

To restore the *mytest* database with the incremental backup `20170114051246`, you need only the incremental backup and the full backup `20161114064330`. The restore command would be similar to this command.

```
gpdbrestore -t 20170114051246 -u /backupdir
```

## Incremental Backup with Sets 

To back up a set of database tables with incremental backup, identify the backup set with the `--prefix` option when you create the full backup with `gpcrondump`. For example, to create incremental backups for tables in the `myschema` schema, first create a full backup with a prefix, such as `myschema`:

```
gpcrondump -x mydb -s myschema --prefix myschema
```

The `-s` option specifies that tables qualified by the `myschema` schema are to be included in the backup. See [Backing Up a Set of Tables](backup-sets.html) for more options to specify a set of tables to back up.

Once you have a full backup you can create an incremental backup for the same set of tables by specifying the `gpcrondump` `--incremental` and `--prefix` options, specifying the prefix you set for the full backup. The incremental backup is automatically limited to only the tables in the full backup. For example:

```
gpcrondump -x mydb --incremental --prefix myschema
```

The following command lists the tables that were included or excluded for the full backup.

```
gpcrondump -x mydb --incremental --prefix myschema --list-filter-tables

```

**Parent topic:** [Parallel Backup with gpcrondump and gpdbrestore](../managing/backup-heading.html)

## Restoring From an Incremental Backup 

When restoring a backup with `gpdbrestore`, the command-line output displays whether the restore type is incremental or a full database restore. You do not have to specify that the backup is incremental. For example, the following `gpdbrestore` command restores the most recent backup of the `mydb` database. `gpdbrestore` searches the `db_dumps` directory to locate the most recent dump and displays information about the backup it found.

```

$ gpdbrestore -s mydb
...
20151015:20:10:34:002664 gpdbrestore:mdw:gpadmin-[INFO]:-------------------------------------------
20151015:20:10:34:002664 gpdbrestore:mdw:gpadmin-[INFO]:-Greenplum database restore parameters
20151015:20:10:34:002664 gpdbrestore:mdw:gpadmin-[INFO]:-------------------------------------------
20151015:20:10:34:002664 gpdbrestore:mdw:gpadmin-[INFO]:-Restore type               = Incremental Restore
20151015:20:10:34:002664 gpdbrestore:mdw:gpadmin-[INFO]:-Database to be restored    = mydb
20151015:20:10:34:002664 gpdbrestore:mdw:gpadmin-[INFO]:-Drop and re-create db      = Off
20151015:20:10:34:002664 gpdbrestore:mdw:gpadmin-[INFO]:-Restore method             = Search for latest
20151015:20:10:34:002664 gpdbrestore:mdw:gpadmin-[INFO]:-Restore timestamp          = 20151014194445
20151015:20:10:34:002664 gpdbrestore:mdw:gpadmin-[INFO]:-Restore compressed dump    = On
20151015:20:10:34:002664 gpdbrestore:mdw:gpadmin-[INFO]:-Restore global objects     = Off
20151015:20:10:34:002664 gpdbrestore:mdw:gpadmin-[INFO]:-Array fault tolerance      = f
20151015:20:10:34:002664 gpdbrestore:mdw:gpadmin-[INFO]:-------------------------------------------

Continue with Greenplum restore Yy|Nn (default=N):

```

`gpdbrestore` ensures that the full backup and other required incremental backups are available before restoring the backup. With the `--list-backup` option you can display the full and incremental backup sets required to perform a restore.

If the `gpdbrestore` option `-q` is specified, the backup type information is written to the log file. With the `gpdbrestore` option `--noplan`, you can restore only the data contained in an incremental backup.

