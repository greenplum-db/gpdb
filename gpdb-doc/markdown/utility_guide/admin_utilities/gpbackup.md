# gpbackup 

Create a Greenplum Database backup for use with the `gprestore` utility.

## Synopsis 

```
gpbackup --dbname <database_name>
   [--backup-dir <directory>]
   [--compression-level <level>]
   [--data-only]
   [--debug]
   [--exclude-schema <schema_name>]
   [--exclude-table <schema.table>]
   [--exclude-table-file <file_name>]
   [--include-schema <schema_name>]
   [--include-table <schema.table>]
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

gpbackup --help 
```

## Description 

The `gpbackup` utility backs up the contents of a database into a collection of metadata files and data files that can be used to restore the database at a later time using `gprestore`. When you back up a database, you can specify table level and schema level filter options to back up specific tables. For example, you can combine schema level and table level options to back up all the tables in a schema except for a single table.

By default, `gpbackup` backs up objects in the specified database as well as global Greenplum Database system objects. You can optionally supply the `--with-globals` option with `gprestore` to restore global objects. See [Objects Included in a Backup or Restore](../../admin_guide/managing/backup-gpbackup.html) for additional information.

`gpbackup` stores the object metadata files and DDL files for a backup in the Greenplum Database master data directory by default. Greenplum Database segments use the `COPY ... ON SEGMENT` command to store their data for backed-up tables in compressed CSV data files, located in each segment's data directory. See [Understanding Backup Files](../../admin_guide/managing/backup-gpbackup.html) for additional information.

You can add the `--backup-dir` option to copy all backup files from the Greenplum Database master and segment hosts to an absolute path for later use. Additional options are provided to filter the backup set in order to include or exclude specific tables.

You can create an incremental backup with the [--incremental](#incremental) option. Incremental backups are efficient when the total amount of data in append-optimized tables or table partitions that changed is small compared to the data has not changed. See [Creating Incremental Backups with gpbackup and gprestore](../../admin_guide/managing/backup-gpbackup-incremental.html) for information about incremental backups.

With the default `--jobs` option \(1 job\), each `gpbackup` operation uses a single transaction on the Greenplum Database master host. The `COPY ... ON SEGMENT` command performs the backup task in parallel on each segment host. The backup process acquires an `ACCESS SHARE` lock on each table that is backed up. During the table locking process, the database should be in a quiescent state.

When a back up operation completes, `gpbackup` returns a status code. See [Return Codes](#return_codes).

`gpbackup` can send status email notifications after a back up operation completes. You specify when the utility sends the mail and the email recipients in a configuration file. See [Configuring Email Notifications](../../admin_guide/managing/backup-gpbackup.html).

**Note:** This utility uses secure shell \(SSH\) connections between systems to perform its tasks. In large Greenplum Database deployments, cloud deployments, or deployments with a large number of segments per host, this utility may exceed the host's maximum threshold for unauthenticated connections. Consider updating the SSH `MaxStartups` and `MaxSessions` configuration parameters to increase this threshold. For more information about SSH configuration options, refer to the SSH documentation for your Linux distribution.

## Options 

**--dbname** database\_name
:   Required. Specifies the database to back up.

**--backup-dir** directory
:   Optional. Copies all required backup files \(metadata files and data files\) to the specified directory. You must specify directory as an absolute path \(not relative\). If you do not supply this option, metadata files are created on the Greenplum Database master host in the $MASTER\_DATA\_DIRECTORY/backups/YYYYMMDD/YYYYMMDDhhmmss/ directory. Segment hosts create CSV data files in the <seg\_dir\>/backups/YYYYMMDD/YYYYMMDDhhmmss/ directory. When you specify a custom backup directory, files are copied to these paths in subdirectories of the backup directory.

:   You cannot combine this option with the option `--plugin-config`.

**--compression-level** level
:   Optional. Specifies the gzip compression level \(from 1 to 9\) used to compress data files. The default is 1. Note that `gpbackup` uses compression by default.

**--data-only**
:   Optional. Backs up only the table data into CSV files, but does not backup metadata files needed to recreate the tables and other database objects.

**--debug**
:   Optional. Displays verbose debug messages during operation.

**--exclude-schema** schema\_name
:   Optional. Specifies a database schema to exclude from the backup. You can specify this option multiple times to exclude multiple schemas. You cannot combine this option with the option `--include-schema`, or a table filtering option such as `--include-table`. See [Filtering the Contents of a Backup or Restore](../../admin_guide/managing/backup-gpbackup.html) for more information.

**--exclude-table** schema.table
:   Optional. Specifies a table to exclude from the backup. The table must be in the format `<schema-name>.<table-name>`. If a table or schema name uses any character other than a lowercase letter, number, or an underscore character, then you must include that name in double quotes. You can specify this option multiple times. You cannot combine this option with the option `--exclude-schema`, or another a table filtering option such as `--include-table`.

:   You cannot use this option in combination with `--leaf-partition-data`. Although you can specify leaf partition names, `gpbackup` ignores the partition names.

:   See [Filtering the Contents of a Backup or Restore](../../admin_guide/managing/backup-gpbackup.html) for more information.

**--exclude-table-file** file\_name
:   Optional. Specifies a text file containing a list of tables to exclude from the backup. Each line in the text file must define a single table using the format `<schema-name>.<table-name>`. The file must not include trailing lines. If a table or schema name uses any character other than a lowercase letter, number, or an underscore character, then you must include that name in double quotes. You cannot combine this option with the option `--exclude-schema`, or another a table filtering option such as `--include-table`.

:   You cannot use this option in combination with `--leaf-partition-data`. Although you can specify leaf partition names in a file specified with `--exclude-table-file`, `gpbackup` ignores the partition names.

:   See [Filtering the Contents of a Backup or Restore](../../admin_guide/managing/backup-gpbackup.html) for more information.

**--include-schema** schema\_name
:   Optional. Specifies a database schema to include in the backup. You can specify this option multiple times to include multiple schemas. If you specify this option, any schemas that are not included in subsequent `--include-schema` options are omitted from the backup set. You cannot combine this option with the options `--exclude-schema`, `--include-table`, or `--include-table-file`. See [Filtering the Contents of a Backup or Restore](../../admin_guide/managing/backup-gpbackup.html) for more information.

**--include-table** schema.table
:   Optional. Specifies a table to include in the backup. The table must be in the format `<schema-name>.<table-name>`. If a table or schema name uses any character other than a lowercase letter, number, or an underscore character, then you must include that name single quotes. See [Schema and Table Names](#table_names) for information about characters that are supported in schema and table names.

:   You can specify this option multiple times. You cannot combine this option with a schema filtering option such as `--include-schema`, or another table filtering option such as `--exclude-table-file`.

:   You can also specify the qualified name of a sequence or a view.

:   If you specify this option, the utility does not automatically back up dependent objects. You must also explicitly specify dependent objects that are required. For example if you back up a view, you must also back up the tables that the view uses. If you back up a table that uses a sequence, you must also back up the sequence.

:   You can optionally specify a table leaf partition name in place of the table name, to include only specific leaf partitions in a backup with the `--leaf-partition-data` option. When a leaf partition is backed up, the leaf partition data is backed up along with the metadata for the partitioned table.

:   See [Filtering the Contents of a Backup or Restore](../../admin_guide/managing/backup-gpbackup.html) for more information.

**--include-table-file** file\_name
:   Optional. Specifies a text file containing a list of tables to include in the backup. Each line in the text file must define a single table using the format `<schema-name>.<table-name>`. The file must not include trailing lines. See [Schema and Table Names](#table_names) for information about characters that are supported in schema and table names.

:   Any tables not listed in this file are omitted from the backup set. You cannot combine this option with a schema filtering option such as `--include-schema`, or another table filtering option such as `--exclude-table-file`.

:   You can also specify the qualified name of a sequence or a view.

:   If you specify this option, the utility does not automatically back up dependent objects. You must also explicitly specify dependent objects that are required. For example if you back up a view, you must also specify the tables that the view uses. If you specify a table that uses a sequence, you must also specify the sequence.

:   You can optionally specify a table leaf partition name in place of the table name, to include only specific leaf partitions in a backup with the `--leaf-partition-data` option. When a leaf partition is backed up, the leaf partition data is backed up along with the metadata for the partitioned table.

:   See [Filtering the Contents of a Backup or Restore](../../admin_guide/managing/backup-gpbackup.html) for more information.

**--incremental**
:   Specify this option to add an incremental backup to an incremental backup set. A backup set is a full backup and one or more incremental backups. The backups in the set must be created with a consistent set of backup options to ensure that the backup set can be used in a restore operation.

:   By default, `gpbackup` attempts to find the most recent existing backup with a consistent set of options. If the backup is a full backup, the utility creates a backup set. If the backup is an incremental backup, the utility adds the backup to the existing backup set. The incremental backup is added as the latest backup in the backup set. You can specify `--from-timestamp` to override the default behavior.

:   **--from-timestamp** backup-timestamp
:   Optional. Specifies the timestamp of a backup. The specified backup must have backup options that are consistent with the incremental backup that is being created. If the specified backup is a full backup, the utility creates a backup set. If the specified backup is an incremental backup, the utility adds the incremental backup to the existing backup set.

:   You must specify `--leaf-partition-data` with this option. You cannot combine this option with `--data-only` or `--metadata-only`.

:   A backup is not created and the utility returns an error if the backup cannot add the backup to an existing incremental backup set or cannot use the backup to create a backup set.

:   For information about creating and using incremental backups, see [Creating Incremental Backups with gpbackup and gprestore](../../admin_guide/managing/backup-gpbackup-incremental.html).

**--jobs** int
:   Optional. Specifies the number of jobs to run in parallel when backing up tables. By default, `gpbackup` uses 1 job \(database connection\). Increasing this number can improve the speed of backing up data. When running multiple jobs, each job backs up tables in a separate transaction. For example, if you specify `--jobs 2`, the utility creates two processes, each process starts a single transaction, and the utility backs up the tables in parallel using the two processes.

    **Important:** If you specify a value higher than 1, the database must be in a quiescent state while the utility acquires a lock on the tables that are being backed up. If database operations are being performed on tables that are being backed up during the table locking process, consistency between tables that are backed up in different transactions cannot be guaranteed.

:   You cannot use this option in combination with the options `--metadata-only`, `--single-data-file`, or `--plugin-config`.

**--leaf-partition-data**
:   Optional. For partitioned tables, creates one data file per leaf partition instead of one data file for the entire table \(the default\). Using this option also enables you to specify individual leaf partitions to include in a backup, with the `--include-table-file` option. You cannot use this option in combination with `--exclude-table-file` or `--exclude-table`.

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

    **Note:** If you use the `--single-data-file` option to combine table backups into a single file per segment, you cannot set the `gprestore` option `--jobs` to a value higher than 1 to perform a parallel restore operation.

**--verbose**
:   Optional. Print verbose log messages.

**--version**
:   Optional. Print the version number and exit.

**--with-stats**
:   Optional. Include query plan statistics in the backup set.

--help
:   Displays the online help.

## Return Codes 

One of these codes is returned after `gpbackup` completes.

-   **0** – Backup completed with no problems.
-   **1** – Backup completed with non-fatal errors. See log file for more information.
-   **2** – Backup failed with a fatal error. See log file for more information.

## Schema and Table Names 

When specifying the table filtering option `--include-table` or `--include-table-file` to list tables to be backed up, the `gpbackup` utility supports backing up schemas or tables when the name contains upper-case characters or these special characters.

`~ # $ % ^ & * ( ) _ - + [ ]  > < \ | ; : / ? ! ,`

If a name contains an upper-case or special character and is specified on the command line with `--include-table`, the name must be enclosed in single quotes.

```
gpbackup --dbname test --include-table 'my#1schema'.'my_$42_Table'
```

When the table is listed in a file for use with `--include-table-file`, single quotes are not required. For example, this is the contents of a text file that is used with `--include-table-file` to back up two tables.

```
my#1schema.my_$42_Table
my#1schema.my_$590_Table
```

**Note:** The `--include-table` and `--include-table-file` options do not support schema or table names that contain the character double quote \(`"`\), period \(`.`\) , newline \(`\n`\), or space \(``\).

## Examples 

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

## See Also 

[gprestore](gprestore.html), [Parallel Backup with gpbackup and gprestore](../../admin_guide/managing/backup-gpbackup.html) and [Using the S3 Storage Plugin with gpbackup and gprestore](../../admin_guide/managing/backup-s3-plugin.html)

