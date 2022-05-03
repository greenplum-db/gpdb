---
title: gpbackup 
---

Create a Greenplum Database backup for use with the `gprestore` utility.

## <a id="synopsis"></a>Synopsis 

```
gpbackup --dbname <database_name>
   [--backup-dir <directory>]
   [--compression-level <level>]
   [--compression-type <type>]
   [--copy-queue-size <int>
   [--data-only]
   [--debug]
   [--exclude-schema <schema_name> [--exclude-schema <schema_name> ...]]
   [--exclude-table <schema.table> [--exclude-table <schema.table> ...]]
   [--exclude-schema-file <file_name>]
   [--exclude-table-file <file_name>]
   [--include-schema <schema_name> [--include-schema <schema_name> ...]]
   [--include-table <schema.table> [--include-table <schema.table> ...]]
   [--include-schema-file <file_name>]
   [--include-table-file <file_name>]
   [--incremental [--from-timestamp <backup-timestamp>]]
   [--jobs <int>]
   [--leaf-partition-data]
   [--metadata-only]
   [--no-compression]
   [--plugin-config <config_file_location>]
   [--quiet]
   [--single-data-file]
   [--verbose]
   [--version]
   [--with-stats]
   [--without-globals]

gpbackup --help 
```

## <a id="desc"></a>Description 

The `gpbackup` utility backs up the contents of a database into a collection of metadata files and data files that can be used to restore the database at a later time using `gprestore`. When you back up a database, you can specify table level and schema level filter options to back up specific tables. For example, you can combine schema level and table level options to back up all the tables in a schema except for a single table.

By default, `gpbackup` backs up objects in the specified database as well as global Greenplum Database system objects. Use `--without-globals` to omit global objects. `gprestore` does not restore global objects by default; use `--with-globals` to restore them. See [Objects Included in a Backup or Restore](../../admin_guide/managing/backup-gpbackup.html) for additional information.

For materialized views, data is not backed up, only the materialized view definition is backed up.

`gpbackup` stores the object metadata files and DDL files for a backup in the Greenplum Database master data directory by default. Greenplum Database segments use the `COPY ... ON SEGMENT` command to store their data for backed-up tables in compressed CSV data files, located in each segment's data directory. See [Understanding Backup Files](../../admin_guide/managing/backup-gpbackup.html) for additional information.

You can add the `--backup-dir` option to copy all backup files from the Greenplum Database master and segment hosts to an absolute path for later use. Additional options are provided to filter the backup set in order to include or exclude specific tables.

You can create an incremental backup with the [--incremental](#incremental) option. Incremental backups are efficient when the total amount of data in append-optimized tables or table partitions that changed is small compared to the data has not changed. See [Creating and Using Incremental Backups with gpbackup and gprestore](../../admin_guide/managing/backup-gpbackup-incremental.html) for information about incremental backups.

With the default `--jobs` option \(1 job\), each `gpbackup` operation uses a single transaction on the Greenplum Database master host. The `COPY ... ON SEGMENT` command performs the backup task in parallel on each segment host. The backup process acquires an `ACCESS SHARE` lock on each table that is backed up. During the table locking process, the database should be in a quiescent state.

When a back up operation completes, `gpbackup` returns a status code. See [Return Codes](#return_codes).

The `gpbackup` utility cannot be run while `gpexpand` is initializing new segments. Backups created before the expansion cannot be restored with `gprestore` after the cluster expansion is completed.

`gpbackup` can send status email notifications after a back up operation completes. You specify when the utility sends the mail and the email recipients in a configuration file. See [Configuring Email Notifications](../../admin_guide/managing/backup-gpbackup.html).

**Note:** This utility uses secure shell \(SSH\) connections between systems to perform its tasks. In large Greenplum Database deployments, cloud deployments, or deployments with a large number of segments per host, this utility may exceed the host's maximum threshold for unauthenticated connections. Consider updating the SSH `MaxStartups` and `MaxSessions` configuration parameters to increase this threshold. For more information about SSH configuration options, refer to the SSH documentation for your Linux distribution.

## <a id="opts"></a>Options 

**--dbname** database\_name
:   Required. Specifies the database to back up.

**--backup-dir** directory
:   Optional. Copies all required backup files \(metadata files and data files\) to the specified directory. You must specify directory as an absolute path \(not relative\). If you do not supply this option, metadata files are created on the Greenplum Database master host in the $MASTER\_DATA\_DIRECTORY/backups/YYYYMMDD/YYYYMMDDhhmmss/ directory. Segment hosts create CSV data files in the <seg\_dir\>/backups/YYYYMMDD/YYYYMMDDhhmmss/ directory. When you specify a custom backup directory, files are copied to these paths in subdirectories of the backup directory.

:   You cannot combine this option with the option `--plugin-config`.

**--compression-level** level
:   Optional. Specifies the compression level \(from 1 to 9\) used to compress data files. The default is 1. Note that `gpbackup` uses compression by default.

**--compression-type** type
:   Optional. Specifies the compression type \(`gzip` or `zstd`\) used to compress data files. The default is `gzip`.

:   **Note:** In order to use the `zstd` compression type, Zstandard \(http://facebook.github.io/zstd/\) must be installed in a $PATH accessible by the gpadmin user.

**--copy-queue-size** int
:   Optional. Specifies the number of `COPY` commands `gpbackup` should enqueue when backing up using the `--single-data-file` option. This option optimizes backup performance by reducing the amount of time spent initializing `COPY` commands. If you do not set this option to 2 or greater, `gpbackup` enqueues 1 `COPY` command at a time.

:   **Note:** This option must be used with the`--single-data-file` option and cannot be used with the `--jobs` option.

**--data-only**
:   Optional. Backs up only the table data into CSV files, but does not backup metadata files needed to recreate the tables and other database objects.

**--debug**
:   Optional. Displays verbose debug messages during operation.

**--exclude-schema** schema\_name
:   Optional. Specifies a database schema to exclude from the backup. You can specify this option multiple times to exclude multiple schemas. You cannot combine this option with the option `--include-schema`, `--include-schema-file`, or a table filtering option such as `--include-table`.

:   See [Filtering the Contents of a Backup or Restore](../../admin_guide/managing/backup-gpbackup.html) for more information.

:   See [Requirements and Limitations](../../admin_guide/managing/backup-gpbackup.html) for limitations when leaf partitions of a partitioned table are in different schemas from the root partition.

**--exclude-schema-file** file\_name
:   Optional. Specifies a text file containing a list of schemas to exclude from the backup. Each line in the text file must define a single schema. The file must not include trailing lines. If a schema name uses any character other than a lowercase letter, number, or an underscore character, then you must include that name in double quotes. You cannot combine this option with the option `--include-schema` or `--include-schema-file`, or a table filtering option such as `--include-table`.

:   See [Filtering the Contents of a Backup or Restore](../../admin_guide/managing/backup-gpbackup.html) for more information.

:   See [Requirements and Limitations](../../admin_guide/managing/backup-gpbackup.html) for limitations when leaf partitions of a partitioned table are in different schemas from the root partition.

**--exclude-table** schema.table
:   Optional. Specifies a table to exclude from the backup. The table must be in the format `<schema-name>.<table-name>`. If a table or schema name uses any character other than a lowercase letter, number, or an underscore character, then you must include that name in double quotes. You can specify this option multiple times. You cannot combine this option with the option `--exclude-schema`, `--exclude-schema-file`, or another a table filtering option such as `--include-table`.

:   If you specify a leaf partition name, `gpbackup` ignores the partition names. The leaf partition is not excluded.

:   See [Filtering the Contents of a Backup or Restore](../../admin_guide/managing/backup-gpbackup.html) for more information.

**--exclude-table-file** file\_name
:   Optional. Specifies a text file containing a list of tables to exclude from the backup. Each line in the text file must define a single table using the format `<schema-name>.<table-name>`. The file must not include trailing lines. If a table or schema name uses any character other than a lowercase letter, number, or an underscore character, then you must include that name in double quotes. You cannot combine this option with the option `--exclude-schema`, `--exclude-schema-file`, or another a table filtering option such as `--include-table`.

:   If you specify leaf partition names in a file that is used with `--exclude-table-file`, `gpbackup` ignores the partition names. The leaf partitions are not excluded.

:   See [Filtering the Contents of a Backup or Restore](../../admin_guide/managing/backup-gpbackup.html) for more information.

**--include-schema** schema\_name
:   Optional. Specifies a database schema to include in the backup. You can specify this option multiple times to include multiple schemas. If you specify this option, any schemas that are not included in subsequent `--include-schema` options are omitted from the backup set. You cannot combine this option with the options `--exclude-schema`, `--exclude-schema-file`, `--exclude-schema-file`, `--include-table`, or `--include-table-file`. See [Filtering the Contents of a Backup or Restore](../../admin_guide/managing/backup-gpbackup.html) for more information.

**--include-schema-file** file\_name
:   Optional. Specifies a text file containing a list of schemas to back up. Each line in the text file must define a single schema. The file must not include trailing lines. If a schema name uses any character other than a lowercase letter, number, or an underscore character, then you must include that name in double quotes. See [Filtering the Contents of a Backup or Restore](../../admin_guide/managing/backup-gpbackup.html) for more information.

**--include-table** schema.table
:   Optional. Specifies a table to include in the backup. The table must be in the format `<schema-name>.<table-name>`. For information on specifying special characters in schema and table names, see [Schema and Table Names](http://docs-lena-gpbackup-globals.cfapps.io/backup-restore/1-18/utility_guide/ref/gpbackup.html#topic1__table_names).

:   You can specify this option multiple times. You cannot combine this option with a schema filtering option such as `--include-schema`, or another table filtering option such as `--exclude-table-file`.

:   You can also specify the qualified name of a sequence, a view, or a materialized view.

:   If you specify this option, the utility does not automatically back up dependent objects. You must also explicitly specify dependent objects that are required. For example if you back up a view or a materialized view, you must also back up the tables that the view or materialized view uses. If you back up a table that uses a sequence, you must also back up the sequence.

:   You can optionally specify a table leaf partition name in place of the table name, to include only specific leaf partitions in a backup with the `--leaf-partition-data` option. When a leaf partition is backed up, the leaf partition data is backed up along with the metadata for the partitioned table.

:   See [Filtering the Contents of a Backup or Restore](../../admin_guide/managing/backup-gpbackup.html) for more information.

**--include-table-file** file\_name
:   Optional. Specifies a text file containing a list of tables to include in the backup. Each line in the text file must define a single table using the format `<schema-name>.<table-name>`. The file must not include trailing lines. For information on specifying special characters in schema and table names, see [Schema and Table Names](#table_names).

:   Any tables not listed in this file are omitted from the backup set. You cannot combine this option with a schema filtering option such as `--include-schema`, or another table filtering option such as `--exclude-table-file`.

:   You can also specify the qualified name of a sequence, a view, or a materialized view.

:   If you specify this option, the utility does not automatically back up dependent objects. You must also explicitly specify dependent objects that are required. For example if you back up a view or a materialized view, you must also specify the tables that the view or the materialized view uses. If you specify a table that uses a sequence, you must also specify the sequence.

:   You can optionally specify a table leaf partition name in place of the table name, to include only specific leaf partitions in a backup with the `--leaf-partition-data` option. When a leaf partition is backed up, the leaf partition data is backed up along with the metadata for the partitioned table.

:   See [Filtering the Contents of a Backup or Restore](../../admin_guide/managing/backup-gpbackup.html) for more information.

**--incremental**
:   Specify this option to add an incremental backup to an incremental backup set. A backup set is a full backup and one or more incremental backups. The backups in the set must be created with a consistent set of backup options to ensure that the backup set can be used in a restore operation.

:   By default, `gpbackup` attempts to find the most recent existing backup with a consistent set of options. If the backup is a full backup, the utility creates a backup set. If the backup is an incremental backup, the utility adds the backup to the existing backup set. The incremental backup is added as the latest backup in the backup set. You can specify `--from-timestamp` to override the default behavior.

:   **--from-timestamp** backup-timestamp
:   Optional. Specifies the timestamp of a backup. The specified backup must have backup options that are consistent with the incremental backup that is being created. If the specified backup is a full backup, the utility creates a backup set. If the specified backup is an incremental backup, the utility adds the incremental backup to the existing backup set.

:   You must specify `--leaf-partition-data` with this option. You cannot combine this option with `--data-only` or `--metadata-only`.

:   A backup is not created and the utility returns an error if the backup cannot add the backup to an existing incremental backup set or cannot use the backup to create a backup set.

:   For information about creating and using incremental backups, see [Creating and Using Incremental Backups with gpbackup and gprestore](../../admin_guide/managing/backup-gpbackup-incremental.html).

**--jobs** int
:   Optional. Specifies the number of jobs to run in parallel when backing up tables. By default, `gpbackup` uses 1 job \(database connection\). Increasing this number can improve the speed of backing up data. When running multiple jobs, each job backs up tables in a separate transaction. For example, if you specify `--jobs 2`, the utility creates two processes, each process starts a single transaction, and the utility backs up the tables in parallel using the two processes.
    <br/><br/>**Important:** If you specify a value higher than 1, the database must be in a quiescent state at the very beginning while the utility creates the individual connections, initializes their transaction snapshots, and acquires a lock on the tables that are being backed up. If concurrent database operations are being performed on tables that are being backed up during the transaction snapshot initialization and table locking step, consistency between tables that are backed up in different parallel workers cannot be guaranteed.

:   You cannot use this option in combination with the options `--metadata-only`, `--single-data-file`, or `--plugin-config`.
:   **Note:** When using the `--jobs` flag, there is a potential deadlock scenario to generate a `WARNING` message in the log files. During the metadata portion of the backup, the main worker process gathers Access Share locks on all the tables in the backup set. During the data portion of the backup, based on the value of the `--jobs` flag, additional workers are created that attempt to take additional Access Share locks on the tables they back up. Between the metadata backup and the data backup, if a third party process \(operations like `TRUNCATE`, `DROP`, `ALTER`\) attempts to access the same tables and obtain an Exclusive lock, the worker thread identifies the potential deadlock, terminates its process, and hands off the table backup responsibilities to the main worker \(that already has an Access Share lock on that particular table\). A warning message is logged, similar to: `[WARNING]:-Worker 5 could not acquire AccessShareLock for table public.foo. Terminating worker and deferring table to main worker thread.`

**--leaf-partition-data**
:   Optional. For partitioned tables, creates one data file per leaf partition instead of one data file for the entire table \(the default\). Using this option also enables you to specify individual leaf partitions to include in or exclude from a backup, with the `--include-table`, `--include-table-file`, `--exclude-table`, and `--exclude-table-file` options.

**--metadata-only**
:   Optional. Creates only the metadata files \(DDL\) needed to recreate the database objects, but does not back up the actual table data.

**--no-compression**
:   Optional. Do not compress the table data CSV files.

**--plugin-config** config-file\_location
:   Specify the location of the `gpbackup` plugin configuration file, a YAML-formatted text file. The file contains configuration information for the plugin application that `gpbackup` uses during the backup operation.

:   If you specify the `--plugin-config` option when you back up a database, you must specify this option with configuration information for a corresponding plugin application when you restore the database from the backup.

:   You cannot combine this option with the option `--backup-dir`.

:   For information about using storage plugin applications, see [Using gpbackup Storage Plugins](../../admin_guide/managing/backup-plugins.html).

**--quiet**
:   Optional. Suppress all non-warning, non-error log messages.

**--single-data-file**
:   Optional. Create a single data file on each segment host for all tables backed up on that segment. By default, each `gpbackup` creates one compressed CSV file for each table that is backed up on the segment.
    <br/><br/>**Note:** If you use the `--single-data-file` option to combine table backups into a single file per segment, you cannot set the `gprestore` option `--jobs` to a value higher than 1 to perform a parallel restore operation.

**--verbose**
:   Optional. Print verbose log messages.

**--version**
:   Optional. Print the version number and exit.

**--with-stats**
:   Optional. Include query plan statistics in the backup set.

**--without-globals**
:   Optional. Omit the global Greenplum Database system objects during backup.

--help
:   Displays the online help.

## <a id="return_codes"></a>Return Codes 

One of these codes is returned after `gpbackup` completes.

-   **0** – Backup completed with no problems.
-   **1** – Backup completed with non-fatal errors. See log file for more information.
-   **2** – Backup failed with a fatal error. See log file for more information.

## <a id="table_names"></a>Schema and Table Names 

When using the option `--include-table` or `--include-table-file` to filter backups, the schema or table names may contain upper-case characters, space \( \), newline \(\\n\), \(\\t\), or any of these special characters:

`~ # $ % ^ & * ( ) _ - + [ ] { } > < \ | ; : / ? ! , " '`

For example:

```
public.foo"bar 
public.foo bar
public.foo\nbar
```

**Note:** The `--include-table` and `--include-table-file` options do not support schema or table names that contain periods \(`.`\) or evaluated newlines.

When the table name has special characters, the name must be enclosed in single quotes:

```
gpbackup --dbname test --include-table 'my#1schema'.'my_$42_Table'
```

When the table name contains single quotes, use an escape character for each quote or encapsulate the table name within double quotes. For example:

```

gpbackup --dbname test --include-table public.'foo\'bar'
gpbackup --dbname test --include-table public."foo'bar"
```

When using the option `--include-table-file`, the table names in the text file do not require single quotes. For example, the contents of the text file could be similar to:

```
my#1schema.my_$42_Table
my#1schema.my_$590_Table
```

## <a id="examples"></a>Examples 

Backup all schemas and tables in the "demo" database, including global Greenplum Database system objects statistics:

```
$ gpbackup --dbname demo
```

Backup all schemas and tables in the "demo" database except for the "twitter" schema:

```
$ gpbackup --dbname demo --exclude-schema twitter
```

Backup only the "twitter" schema in the "demo" database:

```
$ gpbackup --dbname demo --include-schema twitter
```

Backup all schemas and tables in the "demo" database, including global Greenplum Database system objects and query statistics, and copy all backup files to the /home/gpadmin/backup directory:

```
$ gpbackup --dbname demo --with-stats --backup-dir /home/gpadmin/backup
```

This example uses `--include-schema` with `--exclude-table` to back up a schema except for a single table.

```
$ gpbackup --dbname demo --include-schema mydata --exclude-table mydata.addresses
```

You cannot use the option `--exclude-schema` with a table filtering option such as `--include-table`.

## <a id="section9"></a>See Also 

[gprestore](gprestore.html), [Parallel Backup with gpbackup and gprestore](../../admin_guide/managing/backup-gpbackup.html) and [Using the S3 Storage Plugin with gpbackup and gprestore](../../admin_guide/managing/backup-s3-plugin.html)

**Parent topic:**[Backup Utility Reference](../../backup-utilities.html)

