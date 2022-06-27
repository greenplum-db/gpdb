# Backing Up with gpcrondump 

Use `gpcrondump` to backup databases, data, and objects such as database roles and server configuration files.

The `gpcrondump` utility dumps the contents of a Greenplum database to SQL script files on the master and each segment. The script files can then be used to restore the database.

The master backup files contain SQL commands to create the database schema. The segment data dump files contain SQL statements to load the data into the tables. The segment dump files are compressed using `gzip`. Optionally, the server configuration files `postgresql.conf`, `pg_ident.conf`, and `pg_hba.conf` and global data such as roles and tablespaces can be included in a backup.

The `gpcrondump` utility has one required flag, `-x`, which specifies the database to dump:

```
gpcrondump -x <mydb>
```

This performs a full backup of the specified database to the default locations.

**Note:** By default, the utility creates the `public.gpcrondump_history` table that contains details of the database dump. If the `public` schema has been deleted from the database, you must specify the `-H` option to prevent `gpcrondump` from returning an error when it attempts to create the table.

By default, `gpcrondump` creates the backup files in the data directory on the master and each segment instance in the `data\_directory/db_dumps` directory. You can specify a different backup location using the `-u` flag. For example, the following command will save backup files to the `/backups` directory:

```
gpcrondump <mydb> -u </backups>
```

The `gpcrondump` utility creates the `db_dumps` subdirectory in the specified directory. If there is more than one primary segment per host, all of the segments on the host write their backup files to the same directory. This differs from the default, where each segment writes backups to its own data directory. This can be used to consolidate backups to a single directory or mounted storage device.

In the `db_dumps` directory, backups are saved to a directory in the format *YYYYMMDD*, for example `data\_directory/db_dumps/20151012` for a backup created on October 12, 2015. The backup file names in the directory contain a full timestamp for the backup in the format *YYYYMMDDHHMMSS*, for example `gp_dump_0_2_20151012195916.gz`. The `gpdbrestore` command uses the most recent backup by default but you can specify an earlier backup to restore.

The utility creates backup files with this file name format.

```
<prefix_>gp_dump_<content>_<dbid>_<timestamp>
```

The `content` and `dbid` are identifiers for the Greenplum Database segment instances that are assigned by Greenplum Database. For information about the identifiers, see the Greenplum Database system catalog table *gp\_id* in the *Greenplum Database Reference Guide*.

If you include the `-g` option, `gpcrondump` saves the configuration files with the backup. These configuration files are dumped in the master or segment data directory to `db_dumps/YYYYMMDD/config_files_timestamp.tar`. If `--ddboost` is specified, the backup is located on the default storage unit in the directory specified by `--ddboost-backupdir` when the Data Domain Boost credentials were set. The `-G` option backs up global objects such as roles and tablespaces to a file in the master backup directory named `gp_global_-1_1_timestamp`.

If `--ddboost` is specified, the backup is located on the default storage unit in the directory specified by `--ddboost-backupdir` when the Data Domain Boost credentials were set.

There are many more `gpcrondump` options available to configure backups. Refer to the *Greenplum Utility Reference Guide* for information about all of the available options. See [Backing Up Databases with Data Domain Boost](backup-ddboost.html) for details of backing up with Data Domain Boost.

**Warning:** Backing up a database with `gpcrondump` while simultaneously running `ALTER TABLE` might cause `gpcrondump` to fail.

Backing up a database with `gpcrondump` while simultaneously running DDL commands might cause issues with locks. You might see either the DDL command or `gpcrondump` waiting to acquire locks.

**Parent topic:** [Parallel Backup with gpcrondump and gpdbrestore](../managing/backup-heading.html)

