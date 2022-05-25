# gprestore 

Restore a Greenplum Database backup that was created using the `gpbackup` utility. By default `gprestore` uses backed up metadata files and DDL files located in the Greenplum Database master host data directory, with table data stored locally on segment hosts in CSV data files.

## <a id="synopsis"></a>Synopsis 

```
gprestore --timestamp <YYYYMMDDHHMMSS>
   [--backup-dir <directory>]
   [--copy-queue-size <int>
   [--create-db]
   [--debug]
   [--exclude-schema <schema_name> [--exclude-schema <schema_name> ...]]
   [--exclude-table <schema.table> [--exclude-table <schema.table> ...]]
   [--exclude-table-file <file_name>]
   [--exclude-schema-file <file_name>]
   [--include-schema <schema_name> [--include-schema <schema_name> ...]]
   [--include-table <schema.table> [--include-table <schema.table> ...]]
   [--include-schema-file <file_name>]
   [--include-table-file <file_name>]
   [--truncate-table]
   [--redirect-schema <schema_name>]
   [--data-only | --metadata-only]
   [--incremental]
   [--jobs <int>]
   [--on-error-continue]
   [--plugin-config <config_file_location>]
   [--quiet]
   [--redirect-db <database_name>]
   [--verbose]
   [--version]
   [--with-globals]
   [--with-stats]
   [--run-analyze]

gprestore --help
```

## <a id="desc"></a>Description 

To use `gprestore` to restore from a backup set, you must include the `--timestamp` option to specify the exact timestamp value \(`YYYYMMDDHHMMSS`\) of the backup set to restore. If you specified a custom `--backup-dir` to consolidate the backup files, include the same `--backup-dir` option with `gprestore` to locate the backup files.

If the backup you specify is an incremental backup, you need a complete set of backup files \(a full backup and any required incremental backups\). `gprestore` ensures that the complete backup set is available before attempting to restore a backup.

**Important:** For incremental backup sets, the backups must be on a single device. For example, a backup set must all be on a Data Domain system.

For information about incremental backups, see [Creating and Using Incremental Backups with gpbackup and gprestore](../../admin_guide/managing/backup-gpbackup-incremental.html).

When restoring from a backup set, `gprestore` restores to a database with the same name as the name specified when creating the backup set. If the target database exists and a table being restored exists in the database, the restore operation fails. Include the `--create-db` option if the target database does not exist in the cluster. You can optionally restore a backup set to a different database by using the `--redirect-db` option.

When restoring a backup set that contains data from some leaf partitions of a partitioned tables, the partitioned table is restored along with the data for the leaf partitions. For example, you create a backup with the `gpbackup` option `--include-table-file` and the text file lists some leaf partitions of a partitioned table. Restoring the backup creates the partitioned table and restores the data only for the leaf partitions listed in the file.

By default, only database objects in the backup set are restored. Greenplum Database system objects are automatically included in a `gpbackup` backup set, but these objects are only restored if you include the `--with-globals` option to `gprestore`.

During a restore operation, automatic updating of table statistics is disabled for the tables being restored. If you backed up query plan statistics using the `--with-stats` option, you can restore those statistics by providing `--with-stats` to `gprestore`. If you did not use `--with-stats` during a backup, or you want to collect current statistics during the restore operation, you can use the `--run-analyze` option to run `ANALYZE` on the restored tables.

When a materialized view is restored, the data is not restored. To populate the materialized view with data, use `REFRESH MATERIALIZED VIEW`. The tables that are referenced by the materialized view definition must be available. The `gprestore` log file lists the materialized views that were restored and the `REFRESH MATERIALIZED VIEW` commands that are used to populate the materialized views with data.

Performance of restore operations can be improved by creating multiple parallel connections to restore table data and metadata. By default `gprestore` uses 1 connection, but you can increase this number with the `--jobs` option for large restore operations.

When a restore operation completes, `gprestore` returns a status code. See [Return Codes](#return_codes).

`gprestore` can send status email notifications after a back up operation completes. You specify when the utility sends the mail and the email recipients in a configuration file. See [Configuring Email Notifications](../../admin_guide/managing/backup-gpbackup.html).

**Note:** This utility uses secure shell \(SSH\) connections between systems to perform its tasks. In large Greenplum Database deployments, cloud deployments, or deployments with a large number of segments per host, this utility may exceed the host's maximum threshold for unauthenticated connections. Consider updating the SSH `MaxStartups` and `MaxSessions` configuration parameters to increase this threshold. For more information about SSH configuration options, refer to the SSH documentation for your Linux distribution.

## <a id="opts"></a>Options 

**--timestamp** YYYYMMDDHHMMSS
:   Required. Specifies the timestamp of the `gpbackup` backup set to restore. By default `gprestore` tries to locate metadata files for the timestamp on the Greenplum Database master host in the $MASTER\_DATA\_DIRECTORY/backups/YYYYMMDD/YYYYMMDDhhmmss/ directory, and CSV data files in the <seg\_dir\>/backups/YYYYMMDD/YYYYMMDDhhmmss/ directory of each segment host.

**--backup-dir** directory
:   Optional. Sources all backup files \(metadata files and data files\) from the specified directory. You must specify directory as an absolute path \(not relative\). If you do not supply this option, `gprestore` tries to locate metadata files for the timestamp on the Greenplum Database master host in the $MASTER\_DATA\_DIRECTORY/backups/YYYYMMDD/YYYYMMDDhhmmss/ directory. CSV data files must be available on each segment in the <seg\_dir\>/backups/YYYYMMDD/YYYYMMDDhhmmss/ directory. Include this option when you specify a custom backup directory with `gpbackup`.

:   You cannot combine this option with the option `--plugin-config`.

**--create-db**
:   Optional. Creates the database before restoring the database object metadata.

:   The database is created by cloning the empty standard system database `template0`.

**--copy-queue-size** int
:   Optional. Specifies the number of `COPY` commands `gprestore` should enqueue when restoring a backup set. This option optimizes restore performance by reducing the amount of time spent initializing `COPY` commands. If you do not set this option to 2 or greater, `gprestore` enqueues 1 `COPY` command at a time.

**--data-only**
:   Optional. Restores table data from a backup created with the `gpbackup` utility, without creating the database tables. This option assumes the tables exist in the target database. To restore data for a specific set of tables from a backup set, you can specify an option to include tables or schemas or exclude tables or schemas. Specify the `--with-stats` option to restore table statistics from the backup.

:   The backup set must contain the table data to be restored. For example, a backup created with the `gpbackup` option `--metadata-only` does not contain table data.

:   `SEQUENCE` values are updated to match the values taken at the time of the backup.

:   To restore only database tables, without restoring the table data, see the option [--metadata-only](#metadata_only).

**--debug**
:   Optional. Displays verbose and debug log messages during a restore operation.

**--exclude-schema** schema\_name
:   Optional. Specifies a database schema to exclude from the restore operation. You can specify this option multiple times. You cannot combine this option with the option `--include-schema`, `--include-schema-file`, or a table filtering option such as `--include-table`.

**--exclude-schema-file** file\_name
:   Optional. Specifies a text file containing a list of schemas to exclude from the backup. Each line in the text file must define a single schema. The file must not include trailing lines. If a schema name uses any character other than a lowercase letter, number, or an underscore character, then you must include that name in double quotes. You cannot combine this option with the option `--include-schema` or `--include-schema-file`, or a table filtering option such as `--include-table`.

**--exclude-table** schema.table
:   Optional. Specifies a table to exclude from the restore operation. You can specify this option multiple times. The table must be in the format `<schema-name>.<table-name>`. If a table or schema name uses any character other than a lowercase letter, number, or an underscore character, then you must include that name in double quotes. You can specify this option multiple times. If the table is not in the backup set, the restore operation fails. You cannot specify a leaf partition of a partitioned table.

:   You cannot combine this option with the option `--exclude-schema`, `--exclude-schema-file`, or another a table filtering option such as `--include-table`.

**--exclude-table-file** file\_name
:   Optional. Specifies a text file containing a list of tables to exclude from the restore operation. Each line in the text file must define a single table using the format `<schema-name>.<table-name>`. The file must not include trailing lines. If a table or schema name uses any character other than a lowercase letter, number, or an underscore character, then you must include that name in double quotes. If a table is not in the backup set, the restore operation fails. You cannot specify a leaf partition of a partitioned table.

:   You cannot combine this option with the option `--exclude-schema`, `--exclude-schema-file`, or another a table filtering option such as `--include-table`.

**--include-schema** schema\_name
:   Optional. Specifies a database schema to restore. You can specify this option multiple times. If you specify this option, any schemas that you specify must be available in the backup set. Any schemas that are not included in subsequent `--include-schema` options are omitted from the restore operation.

:   If a schema that you specify for inclusion exists in the database, the utility issues an error and continues the operation. The utility fails if a table being restored exists in the database.

:   You cannot use this option if objects in the backup set have dependencies on multiple schemas.

:   See [Filtering the Contents of a Backup or Restore](../../admin_guide/managing/backup-gpbackup.html) for more information.

**--include-schema-file** file\_name
:   Optional. Specifies a text file containing a list of schemas to restore. Each line in the text file must define a single schema. The file must not include trailing lines. If a schema name uses any character other than a lowercase letter, number, or an underscore character, then you must include that name in double quotes.

:   The schemas must exist in the backup set. Any schemas not listed in this file are omitted from the restore operation.

:   You cannot use this option if objects in the backup set have dependencies on multiple schemas.

**--include-table** schema.table
:   Optional. Specifies a table to restore. The table must be in the format <schema-name\>.<table-name\>. You can specify this option multiple times. You cannot specify a leaf partition of a partitioned table. For information on specifying special characters in schema and table names, see the `gpbackup` [Schema and Table Names](gpbackup.html) section.

:   You can also specify the qualified name of a sequence, a view, or a materialized view.

:   If you specify this option, the utility does not automatically restore dependent objects. You must also explicitly specify the dependent objects that are required. For example if you restore a view or a materialized view, you must also restore the tables that the view or the materialized view uses. If you restore a table that uses a sequence, you must also restore the sequence. The dependent objects must exist in the backup set.

:   You cannot combine this option with a schema filtering option such as `--include-schema`, or another table filtering option such as `--exclude-table-file`.

**--include-table-file** file\_name
:   Optional. Specifies a text file containing a list of tables to restore. Each line in the text file must define a single table using the format `<schema-name>.<table-name>`. The file must not include trailing lines. Any tables not listed in this file are omitted from the restore operation. You cannot specify a leaf partition of a partitioned table. For information on specifying special characters in schema and table names, see the `gpbackup` [Schema and Table Names](gpbackup.html) section.

:   You can also specify the qualified name of a sequence, a view, or a materialized view.

:   If you specify this option, the utility does not automatically restore dependent objects. You must also explicitly specify dependent objects that are required. For example if you restore a view or a materialized view, you must also specify the tables that the view or the materialized uses. If you specify a table that uses a sequence, you must also specify the sequence. The dependent objects must exist in the backup set.

:   For a materialized view, the data is not restored. To populate the materialized view with data, you must use `REFRESH MATERIALIZED VIEW` and the tables that are referenced by the materialized view definition must be available.

:   If you use the `--include-table-file` option, `gprestore` does not create roles or set the owner of the tables. The utility restores table indexes and rules. Triggers are also restored but are not supported in Greenplum Database.

:   See [Filtering the Contents of a Backup or Restore](../../admin_guide/managing/backup-gpbackup.html) for more information.

--incremental \(Beta\)
:   Optional. Requires the `--data-only option`. Restores only the table data in the incremental backup specified by the `--timestamp` option. Table data is not restored from previous incremental backups in the backup set. For information about incremental backups, see [Creating and Using Incremental Backups with gpbackup and gprestore](../../admin_guide/managing/backup-gpbackup-incremental.html).
    <br/><br/>**Warning:** This is a Beta featureand is not supported in a production environment.

:   An incremental backup contains the following table data that can be restored.

:   -   Data from all heap tables.
-   Data from append-optimized tables that have been modified since the previous backup.
-   Data from leaf partitions that have been modified from the previous backup.

:   When this option is specified, `gprestore` restores table data by truncating the table and reloading data into the table. `SEQUENCE` values are then updated to match the values taken at the time of the backup.

:   Before performing the restore operation, `gprestore` ensures that the tables being restored exist. If a table does not exist, `gprestore` returns an error and exits. If the `--on-error-continue` option is specified, `gprestore` logs missing tables and attempts to complete the restore operation.

:   **Warning:** When this option is specified, `gpbackup` assumes that no changes have been made to the table definitions of the tables being restored, such as adding or removing columns.

**--truncate-table**
:   Optional. Truncate data from a set of tables before restoring the table data from a backup. This option lets you replace table data with data from a backup. Otherwise, table data might be duplicated.

:   You must specify the set of tables with either the option `--include-table` or `--include-table-file`. You must also specify `--data-only` to restore table data without creating the tables.

:   You can use this option with the `--redirect-db` option. You cannot use this option with `--redirect-schema`.

**--redirect-schema** schema\_name
:   Optional. Restore data in the specified schema instead of the original schemas. The specified schema must already exist. If the data being restored is in multiple schemas, all the data is redirected into the specified schema.

:   This option must be used with an option that includes tables or schemas: `--include-table`, `--include-table-file`, `--include-schema`, or `--include-schema-file`.

:   You cannot use this option with an option that excludes schemas or tables such as `--exclude-schema` or `--exclude-table`.

:   You can use this option with the `--metadata-only` or `--data-only` options.

**--jobs** int
:   Optional. Specifies the number of parallel connections to use when restoring table data and metadata. By default, `gprestore` uses 1 connection. Increasing this number can improve the speed of restoring data.
    <br/><br/>**Note:** If you used the `gpbackup --single-data-file` option to combine table backups into a single file per segment, you cannot set `--jobs` to a value higher than 1 to perform a parallel restore operation.

**--metadata-only**
:   Optional. Creates database tables from a backup created with the `gpbackup` utility, but does not restore the table data. This option assumes the tables do not exist in the target database. To create a specific set of tables from a backup set, you can specify an option to include tables or schemas or exclude tables or schemas. Specify the option `--with-globals` to restore the Greenplum Database system objects.

:   The backup set must contain the DDL for tables to be restored. For example, a backup created with the `gpbackup` option `--data-only` does not contain the DDL for tables.

:   To restore table data after you create the database tables, see the option [--data-only](#data_only).

**--on-error-continue**
:   Optional. Specify this option to continue the restore operation if an SQL error occurs when creating database metadata \(such as tables, roles, or functions\) or restoring data. If another type of error occurs, the utility exits. The default is to exit on the first error.

:   When this option is included, the utility displays an error summary and writes error information to the `gprestore` log file and continues the restore operation. The utility also creates text files in the backup directory that contain the list of tables that generated SQL errors.
    -   Tables with metadata errors - `gprestore_<backup-timestamp>_<restore-time>_error_tables_metadata`
    -   Tables with data errors - `gprestore_<backup-timestamp>_<restore-time>_error_tables_data`

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
:   Optional. Restore query plan statistics from the backup set. If the backup set was not created with the `--with-stats` option, an error is returned. Restored tables will only have statistics from the backup. You cannot use this option with `--run-analyze`.

:   To collect current statistics for the restored tables during the restore operation, use the `--run-analyze` option. As an alternative, you can run the `ANALYZE` command on the tables after the tables are restored.

**--run-analyze**
:   Optional. Run `ANALYZE` on the tables that are restored. For a partitioned table, `ANALYZE` is run on the root partitioned table. If `--with-stats` was specified for the backup, those statistics are ignored. You cannot use this option with `--with-stats`.

:   If the backup being restored used the `gpbackup` option [--leaf-partition-data](gpbackup.html#backup_leaf_part), `gprestore` runs `ANALYZE` only on the individual leaf partitions that are restored, not the root partitioned table. For Greenplum Database 5, `ANALYZE` updates the root partitioned table statistics when all leaf partitions have statistics as the default. For Greenplum Database 4, you must run `ANALYZE` on the root partitioned table to update the root partition statistics.

:   Depending the tables being restored, running `ANALYZE` on restored tables might increase the duration of the restore operation.

--help
:   Displays the online help.

## <a id="return_codes"></a>Return Codes 

One of these codes is returned after `gprestore` completes.

-   **0** – Restore completed with no problems.
-   **1** – Restore completed with non-fatal errors. See log file for more information.
-   **2** – Restore failed with a fatal error. See log file for more information.

## <a id="examples"></a>Examples 

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

## <a id="section9"></a>See Also 

[gpbackup](gpbackup.html), [Parallel Backup with gpbackup and gprestore](../../admin_guide/managing/backup-gpbackup.html) and [Using the S3 Storage Plugin with gpbackup and gprestore](../../admin_guide/managing/backup-s3-plugin.html)

**Parent topic:** [Backup Utility Reference](../../backup-utilities.html)

