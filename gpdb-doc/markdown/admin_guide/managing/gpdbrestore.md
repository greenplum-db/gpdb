# Restoring a Database Using gpdbrestore 

The `gpdbrestore` utility restores a database from backup files created by `gpcrondump`.

The `gpdbrestore` requires one of the following options to identify the backup set to restore:

-   `-t timestamp` – restore the backup with the specified timestamp.
-   `-b YYYYYMMDD` – restore dump files for the specified date in the `db_dumps` subdirectories on the segment data directories.
-   `-s database\_name` – restore the latest dump files for the specified database found in the segment data directories.
-   `-R hostname:path` – restore the backup set located in the specified directory of a remote host.

To restore an incremental backup, you need a complete set of backup files—a full backup and any required incremental backups. You can use the `--list-backups` option to list the full and incremental backup sets required for an incremental backup specified by timestamp. For example:

```
$ gpdbrestore -t 20151013195916 --list-backup
```

You can restore a backup to a different database using the `--redirect <database>` option. The database is created if it does not exist. The following example restores the most recent backup of the `mydb` database to a new database named `mydb_snapshot`:

```
$ gpdbrestore -s grants --redirect grants_snapshot
```

You can also restore a backup to a different Greenplum Database system. See [Restoring to a Different Greenplum System Configuration](restore-diff-system.html) for information about this option.

## To restore from an archive host using gpdbrestore 

You can restore a backup saved on a host outside of the Greenplum cluster using the `-R` option. Although the Greenplum Database software does not have to be installed on the remote host, the remote host must have a `gpadmin` account configured with passwordless ssh access to all hosts in the Greenplum cluster. This is required because each segment host will use scp to copy its segment backup file from the archive host. See `gpssh-exkeys` in the *Greenplum Database Utility Guide* for help adding the remote host to the cluster.

This procedure assumes that the backup set was moved off the Greenplum array to another host in the network.

1.  Ensure that the archive host is reachable from the Greenplum master host:

    ```
    $ ping <archive_host>
    ```

2.  Ensure that you can ssh into the remote host with the `gpadmin` account and no password.

    ```
    $ ssh gpadmin@<archive_host>
    ```

3.  Ensure that you can ping the master host from the archive host:

    ```
    $ ping mdw
    ```

4.  Ensure that the restore's target database exists. For example:

    ```
    $ createdb database_name
    ```

5.  From the master, run the `gpdbrestore` utility. The `-R` option specifies the host name and path to a complete backup set:

    ```
    $ gpdbrestore -R <archive_host>:/gpdb/backups/archive/20120714 -e <dbname>
    ```

    Omit the `-e dbname` option if the database has already been created.


**Parent topic:** [Parallel Backup with gpcrondump and gpdbrestore](../managing/backup-heading.html)

