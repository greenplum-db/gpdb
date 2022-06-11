# gprestore 

Restore a Greenplum Database backup that was created using the `gpbackup` utility. By default `gprestore` uses backed up metadata files and DDL files located in the Greenplum Database master host data directory, with table data stored locally on segment hosts in CSV data files.

## Synopsis 

```
gprestore --timestamp <YYYYMMDDHHMMSS>
   [--backup-dir <directory>]
   [--create-db]
   [--debug]
   [--exclude-schema <schema_name>]
   [--exclude-table <schema.table>]
   [--exclude-table-file <file_name>]
   [--include-schema <schema_name>]
   [--include-table <schema.table>]
   [--include-table-file <file_name>]
   [--data-only | --metadata-only]
   [--jobs <int>]
   [--on-error-continue]
   [--plugin-config <config_file_location>]
   [--quiet]
   [--redirect-db <database_name>]
   [--verbose]
   [--version]
   [--with-globals]
   [--with-stats]

gprestore --help
```

## Description 

To use `gprestore` to restore from a backup set, you must include the `--timestamp` option to specify the exact timestamp value \(`YYYYMMDDHHMMSS`\) of the backup set to restore. If you specified a custom `--backup-dir` to consolidate the backup files, include the same `--backup-dir` option with `gprestore` to locate the backup files.

If the backup you specify is an incremental backup, you need a complete set of backup files \(a full backup and any required incremental backups\). `gprestore` ensures that the complete backup set is available before attempting to restore a backup.

**Important:** For incremental backup sets, the backups must be on a single device. For example, a backup set must all be on a Data Domain system.

For information about incremental backups, see [Creating Incremental Backups with gpbackup and gprestore](../../admin_guide/managing/backup-gpbackup-incremental.html).

When restoring from a backup set, `gprestore` restores to a database with the same name as the name specified when creating the backup set. If the target database exists and a table being restored exists in the database, the restore operation fails. Include the `--create-db` option if the target database does not exist in the cluster. You can optionally restore a backup set to a different database by using the `--redirect-db` option.

When restoring a backup set that contains data from some leaf partitions of a partitioned tables, the partitioned table is restored along with the data for the leaf partitions. For example, you create a backup with the `gpbackup` option `--include-table-file` and the text file lists some leaf partitions of a partitioned table. Restoring the backup creates the partitioned table and restores the data only for the leaf partitions listed in the file.

Greenplum Database system objects are automatically included in a `gpbackup` backup set, but these objects are only restored if you include the `--with-globals` option to `gprestore`. Similarly, if you backed up query plan statistics using the `--with-stats` option, you can restore those statistics by providing `--with-stats` to `gprestore`. By default, only database objects in the backup set are restored.

Performance of restore operations can be improved by creating multiple parallel connections to restore table data and metadata. By default `gprestore` uses 1 connection, but you can increase this number with the `--jobs` option for large restore operations.

When a restore operation completes, `gprestore` returns a status code. See [Return Codes](#return_codes).

`gprestore` can send status email notifications after a back up operation completes. You specify when the utility sends the mail and the email recipients in a configuration file. See [Configuring Email Notifications](../../admin_guide/managing/backup-gpbackup.html).

**Note:** This utility uses secure shell \(SSH\) connections between systems to perform its tasks. In large Greenplum Database deployments, cloud deployments, or deployments with a large number of segments per host, this utility may exceed the host's maximum threshold for unauthenticated connections. Consider updating the SSH `MaxStartups` and `MaxSessions` configuration parameters to increase this threshold. For more information about SSH configuration options, refer to the SSH documentation for your Linux distribution.

## Options 

**--timestamp** YYYYMMDDHHMMSS
:   Required. Specifies the timestamp of the `gpbackup` backup set to restore. By default `gprestore` tries to locate metadata files for the timestamp on the Greenplum Database master host in the $MASTER\_DATA\_DIRECTORY/backups/YYYYMMDD/YYYYMMDDhhmmss/ directory, and CSV data files in the <seg\_dir\>/backups/YYYYMMDD/YYYYMMDDhhmmss/ directory of each segment host.

**--backup-dir** directory
:   Optional. Sources all backup files \(metadata files and data files\) from the specified directory. You must specify directory as an absolute path \(not relative\). If you do not supply this option, `gprestore` tries to locate metadata files for the timestamp on the Greenplum Database master host in the $MASTER\_DATA\_DIRECTORY/backups/YYYYMMDD/YYYYMMDDhhmmss/ directory. CSV data files must be available on each segment in the <seg\_dir\>/backups/YYYYMMDD/YYYYMMDDhhmmss/ directory. Include this option when you specify a custom backup directory with `gpbackup`.

:   You cannot combine this option with the option `--plugin-config`.

**--create-db**
:   Optional. Creates the database before restoring the database object metadata.

:   The database is created by cloning the empty standard system database `template0`.

**--data-only**
:   Optional. Restores table data from a backup created with the `gpbackup` utility, without creating the database tables. This option assumes the tables exist in the target database. To restore data for a specific set of tables from a backup set, you can specify an option to include tables or schemas or exclude tables or schemas. Specify the `--with-stats` option to restore table statistics from the backup.

:   The backup set must contain the table data to be restored. For example, a backup created with the `gpbackup` option `--metadata-only` does not contain table data.

:   To restore only database tables, without restoring the table data, see the option [--metadata-only](#metadata_only).

**--debug**
:   Optional. Displays verbose and debug log messages during a restore operation.

**--exclude-schema** schema\_name
:   Optional. Specifies a database schema to exclude from the restore operation. You can specify this option multiple times to exclude multiple schemas. You cannot combine this option with the option `--include-schema`, or a table filtering option such as `--include-table`.

**--exclude-table** schema.table
:   Optional. Specifies a table to exclude from the restore operation. The table must be in the format `<schema-name>.<table-name>`. If a table or schema name uses any character other than a lowercase letter, number, or an underscore character, then you must include that name in double quotes. You can specify this option multiple times. If the table is not in the backup set, the restore operation fails. You cannot specify a leaf partition of a partitioned table.

:   You cannot combine this option with the option `--exclude-schema`, or another a table filtering option such as `--include-table`.

**--exclude-table-file** file\_name
:   Optional. Specifies a text file containing a list of tables to exclude from the restore operation. Each line in the text file must define a single table using the format `<schema-name>.<table-name>`. The file must not include trailing lines. If a table or schema name uses any character other than a lowercase letter, number, or an underscore character, then you must include that name in double quotes. If a table is not in the backup set, the restore operation fails. You cannot specify a leaf partition of a partitioned table.

:   You cannot combine this option with the option `--exclude-schema`, or another a table filtering option such as `--include-table`.

**--include-schema** schema\_name
:   Optional. Specifies a database schema to restore. You can specify this option multiple times to include multiple schemas. If you specify this option, any schemas that you specify must be available in the backup set. Any schemas that are not included in subsequent `--include-schema` options are omitted from the restore operation.

:   If a schema that you specify for inclusion exists in the database, the utility issues an error and continues the operation. The utility fails if a table being restored exists in the database.

:   You cannot use this option if objects in the backup set have dependencies on multiple schemas.

:   See [Filtering the Contents of a Backup or Restore](../../admin_guide/managing/backup-gpbackup.html) for more information.

**--include-table** schema.table
:   Optional. Specifies a table to restore. The table must be in the format `<schema-name>.<table-name>`. If a table or schema name uses any character other than a lowercase letter, number, or an underscore character, then you must include that name in double quotes. You can specify this option multiple times. You cannot specify a leaf partition of a partitioned table.

:   You can also specify the qualified name of a sequence or a view.

:   If you specify this option, the utility does not automatically restore dependent objects. You must also explicitly specify the dependent objects that are required. For example if you restore a view, you must also restore the tables that the view uses. If you restore a table that uses a sequence, you must also restore the sequence. The dependent objects must exist in the backup set.

:   You cannot combine this option with a schema filtering option such as `--include-schema`, or another table filtering option such as `--exclude-table-file`.

**--include-table-file** file\_name
:   Optional. Specifies a text file containing a list of tables to restore. Each line in the text file must define a single table using the format `<schema-name>.<table-name>`. The file must not include trailing lines. If a table or schema name uses any character other than a lowercase letter, number, or an underscore character, then you must include that name in double quotes. Any tables not listed in this file are omitted from the restore operation. You cannot specify a leaf partition of a partitioned table.

:   You can also specify the qualified name of a sequence or a view.

:   If you specify this option, the utility does not automatically restore dependent objects. You must also explicitly specify dependent objects that are required. For example if you restore a view, you must also specify the tables that the view uses. If you specify a table that uses a sequence, you must also specify the sequence. The dependent objects must exist in the backup set.

:   If you use the `--include-table-file` option, `gprestore` does not create roles or set the owner of the tables. The utility restores table indexes and rules. Triggers are also restored but are not supported in Greenplum Database.

:   See [Filtering the Contents of a Backup or Restore](../../admin_guide/managing/backup-gpbackup.html) for more information.

**--jobs** int
:   Optional. Specifies the number of parallel connections to use when restoring table data and metadata. By default, `gprestore` uses 1 connection. Increasing this number can improve the speed of restoring data.

    **Note:** If you used the `gpbackup --single-data-file` option to combine table backups into a single file per segment, you cannot set `--jobs` to a value higher than 1 to perform a parallel restore operation.

**--metadata-only**
:   Optional. Creates database tables from a backup created with the `gpbackup` utility, but does not restore the table data. This option assumes the tables do not exist in the target database. To create a specific set of tables from a backup set, you can specify an option to include tables or schemas or exclude tables or schemas. Specify the option `--with-globals` to restore the Greenplum Database system objects.

:   The backup set must contain the DDL for tables to be restored. For example, a backup created with the `gpbackup` option `--data-only` does not contain the DDL for tables.

:   To restore table data after you create the database tables, see the option [--data-only](#data_only).

**--on-error-continue**
:   Optional. Specify this option to continue the restore operation if an SQL error occurs when creating database metadata \(such as tables, roles, or functions\) or restoring data. If another type of error occurs, the utility exits. The utility displays an error summary and writes error information to the `gprestore` log file and continues the restore operation.

:   The default is to exit on the first error.

**--plugin-config** config-file\_location
:   Specify the location of the `gpbackup` plugin configuration file, a YAML-formatted text file. The file contains configuration information for the plugin application that `gprestore` uses during the restore operation.

:   If you specify the `--plugin-config` option when you back up a database, you must specify this option with configuration information for a corresponding plugin application when you restore the database from the backup.

:   You cannot combine this option with the option `--backup-dir`.

:   For information about using storage plugin applications, see [Using gpbackup Storage Plugins](../../admin_guide/managing/backup-plugins.html).

**--quiet**
:   Optional. Suppress all non-warning, non-error log messages.

**--redirect-db** database\_name
:   Optional. Restore to the specified database\_name instead of to the database that was backed up.

**--verbose**
:   Optional. Displays verbose log messages during a restore operation.

**--version**
:   Optional. Print the version number and exit.

**--with-globals**
:   Optional. Restores Greenplum Database system objects in the backup set, in addition to database objects. See [Objects Included in a Backup or Restore](../../admin_guide/managing/backup-gpbackup.html).

**--with-stats**
:   Optional. Restore query plan statistics from the backup set.

--help
:   Displays the online help.

## Return Codes 

One of these codes is returned after `gprestore` completes.

-   **0** – Restore completed with no problems.
-   **1** – Restore completed with non-fatal errors. See log file for more information.
-   **2** – Restore failed with a fatal error. See log file for more information.

## Examples 

Create the demo database and restore all schemas and tables in the backup set for the indicated timestamp:

```
$ dropdb demo
$ gprestore --timestamp 20171103152558 --create-db
```

Restore the backup set to the "demo2" database instead of the "demo" database that was backed up:

```
$ createdb demo2
$ gprestore --timestamp 20171103152558 --redirect-db demo2
```

Restore global Greenplum Database metadata and query plan statistics in addition to the database objects:

```
$ gprestore --timestamp 20171103152558 --create-db --with-globals --with-stats
```

Restore, using backup files that were created in the /home/gpadmin/backup directory, creating 8 parallel connections:

```
$ gprestore --backup-dir /home/gpadmin/backups/ --timestamp 20171103153156 --create-db --jobs 8
```

Restore only the "wikipedia" schema included in the backup set:

```
$ dropdb demo
$ gprestore --include-schema wikipedia --backup-dir /home/gpadmin/backups/ --timestamp 20171103153156 --create-db
```

If you restore from an incremental backup set, all the required files in the backup set must be available to `gprestore`. For example, the following timestamp keys specify an incremental backup set. 20170514054532 is the full backup and the others are incremental backups.

```
20170514054532 (full backup)
20170714095512 
20170914081205 
20171114064330 
20180114051246
```

The following `gprestore` command specifies the timestamp 20121114064330. The incremental backup with the timestamps 20120714095512 and 20120914081205 and the full backup must be available to perform a restore.

```
gprestore --timestamp 20121114064330 --redirect-db mystest --create-db
```

## See Also 

[gpbackup](gpbackup.html), [Parallel Backup with gpbackup and gprestore](../../admin_guide/managing/backup-gpbackup.html) and [Using the S3 Storage Plugin with gpbackup and gprestore](../../admin_guide/managing/backup-s3-plugin.html)

