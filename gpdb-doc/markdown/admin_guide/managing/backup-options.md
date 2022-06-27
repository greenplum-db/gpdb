# Backup and Restore Options 

The Greenplum Database backup and restore utilities support various locations for backup files:

-   With the `gpcrondump` utility, backup files may be saved in the default location, the `db_dumps` subdirectory of the master and each segment, or saved to a different directory specified with the `gpcrondump -u` option.
-   Both the `gpcrondump` and `gpdbrestore` utilities have integrated support for Dell EMC Data Domain Boost and Veritas NetBackup systems.
-   Backup files can be saved through named pipes to any network accessible location.
-   Backup files saved to the default location may be moved to an archive server on the network. This allows performing the backup at the highest transfer rates \(when segments write the backup data to fast local disk arrays\) and then freeing up disk space by moving the files to remote storage.

You can create dumps containing selected database objects:

-   You can backup tables belonging to one or more schema you specify on the command line or in a text file.
-   You can specify schema to exclude from the backup, as command-line options or in a list provided in a text file.
-   You can backup a specified set of tables listed on the command line or in a text file. The table and schema options cannot be used together in a single backup.
-   In addition to database objects, `gpcrondump` can backup the configuration files `pg_hba.conf`, `pg_ident.conf`, and `postgresql.conf`, and global database objects, such as roles and tablespaces.

You can create incremental backups:

-   An incremental backup contains only append-optimized and column-oriented tables that have changed since the most recent incremental or full backup.
-   For partitioned append-optimized tables, only changed append-optimized/column-oriented table partitions are backed up.
-   Incremental backups include all heap tables.
-   Use the `gpcrondump` `--incremental` flag to specify an incremental backup.
-   Restoring an incremental backup requires a full backup and all subsequent incremental backups, up to the backup you are restoring.

The `gpdbrestore` utility offers many options:

-   By default, `gpdbrestore` restores data to the database it was backed up from.
-   The `--redirect` flag allows you to restore a backup to a different database.
-   The restored database can be dropped and recreated, but the default is to restore into an existing database.
-   Selected tables can be restored from a backup by listing the tables on the command line or by listing them in a text file and specifying the text file on the command line.
-   You can restore a database from backup files moved to an archive server. The backup files are copied back into place on the master host and each segment host and then restored to the database.

**Parent topic:** [Parallel Backup with gpcrondump and gpdbrestore](../managing/backup-heading.html)

