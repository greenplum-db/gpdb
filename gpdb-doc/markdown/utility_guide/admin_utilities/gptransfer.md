# gptransfer 

The `gptransfer` utility copies objects from databases in a source Greenplum Database system to databases in a destination Greenplum Database system.

**Note:** Greenplum Database provides two utilities for migrating data between Greenplum Database installations, [gpcopy](./gpcopy.html) and `gptransfer`. Use `gpcopy` to migrate data to a Greenplum Database cluster version 5.9.0 and later. Use the `gptransfer` utility to migrate data between Greenplum Database installations running software version 5.8.0 or earlier.

The `gptransfer` utiltiy is deprecated and will be removed in the next major release of Greenplum Database. For information about migrating data with `gpcopy`, see [Migrating Data with gpcopy](../../admin_guide/managing/gpcopy-migrate.html).

## Synopsis 

```
gptransfer
   { --full |
   { [-d <database1> [ -d <database2> ... ]] |
   [-t <db>.<schema>.<table> [ -t <db>.<schema1>.<table1> ... ]] |
   [-f <table-file> [--partition-transfer
     | --partition-transfer-non-partition-target ]]
   [-T <db>.<schema>.<table> [ -T <db>.<schema1>.<table1> ... ]]
   [-F <table-file>] } }
   [--skip-existing | --truncate | --drop]
   [--analyze] [--validate=<type>] [-x] [--dry-run]
   [--schema-only ]
   [--source-host=<source_host> [--source-port=<source_port>]
   [--source-user=<source_user>]]
   [--base-port=<base_gpfdist_port>]
   [--dest-host=<dest_host> --source-map-file=<host_map_file>
   [--dest-port=<port>] [--dest-user=<dest_user>] ]
   [--dest-database=<dest_database_name>]
   [--batch-size=<batch_size>] [--sub-batch-size=<sub_batch_size>]
   [--timeout=<seconds>]
   [--max-line-length=<length>]
   [--work-base-dir=<work_dir>] [-l <log_dir>]
   [--format=[CSV|TEXT] ]
   [--quote=<character> ]
   [--no-final-count ]

   [-v | --verbose]
   [-q | --quiet]
   [--gpfdist-verbose]
   [--gpfdist-very-verbose]
   [-a]

gptransfer --version

gptransfer -h | -? | --help
```

## Description 

The `gptransfer` utility copies database objects from a source Greenplum Database system to a destination system. You can perform one of the following types of operations:

-   Copy a Greenplum Database system with the `--full` option.

    This option copies all user created databases in a source system to a different destination system. If you specify the `--full` option, you must specify both a source and destination system. The destination system cannot contain any user-defined databases, only the default databases `postgres`, `template0`, and `template1`.

-   Copy a set of user defined database tables to a destination system. The `-f`, and `-t` options copy a specified set of user defined tables, table data, and re-creates the table indexes. The `-d` option copies all user defined tables, table data, and re-creates the table indexes from a specified database.

    If the destination system is the same as the source system, you must also specify a destination database with the `--dest-database` option. When you specify a destination database, the source database tables are copied into the specified destination database.

    For partitioned tables, you can specify the `--partition-transfer` or the `--partition-transfer-non-partition-target` option with `-f` option to copy specific leaf child partitions of partitioned tables from a source database. The leaf child partitions are the lowest level partitions of a partitioned database. For the `--partition-transfer` option, the destination tables are leaf child partitions. For the `--partition-transfer-non-partition-target` option, the destination tables are non-partitioned tables.


If an invalid set of `gptransfer` options are specified, or if a specified source table or database does not exist, `gptransfer` returns an error and quits. No data is copied.

To copy database objects between Greenplum Database systems `gptransfer` utility uses:

-   The Greenplum Database utility `gpfdist` on the source database system. The `gpfdists` protocol is not supported.
-   Writable external tables on the source database system and readable external tables on the destination database system. Note that `gptransfer` creates external tables with the `execute` protocol, so the `--source_user` account must have superuser privileges \(the default is `gpadmin`\).
-   Named pipes that transfer the data between a writable external table and a readable external table.

When copying data into the destination system, it is redistributed on the Greenplum Database segments of the destination system. This is the flow of data when `gptransfer` copies database data:

writable external table \> gpfdist \> named pipe \> gpfdist \> readable external table

For information about transferring data with `gptransfer`, see "Migrating Data with Gptransfer" in the *Greenplum Database Administrator Guide*.

## About Database, Schema, and Table Names 

When you transfer an entire database, schema and table names in the database can contain only alphanumeric characters and the underscore character \(`_`\). Also, Unicode characters are not supported. The same naming restrictions apply when you specify a database, schema, or table as an option or in a file.

## Notes 

The gptransfer utility efficiently transfers tables with large amounts of data. Because of the overhead required to set up parallel transfers, the utility is not recommended for transferring tables with small amounts of data. It might be more efficient to copy the schema and smaller tables to the destination database using other methods, such as the SQL COPY command, and then use `gptransfer` to transfer large tables in batches.

The maximum length of a data row that the `gptransfer` utility can copy is 256 MB.

When transferring data between databases, you can run only one instance of `gptransfer` at a time.Running multiple, concurrent instances of `gptransfer` is not supported.

When copying database data between different Greenplum Database systems, `gptransfer` requires a text file that lists all the source segment host names and IP addresses. Specify the name and location of the file with the `--source-map-file` option. If the file is missing or not all segment hosts are listed, `gptransfer` returns an error and quits. See the description of the option for file format information.

The source and destination Greenplum Database segment hosts need to be able to communicate with each other. To ensure that the segment hosts can communicate, you can use a tool such as the Linux `netperf` utility.

If a filespace has been created for a source Greenplum Database system, a corresponding filespace must exist on the target system.

SSH keys must be exchanged between the two systems before using `gptransfer`. The `gptransfer` utility connects to the source system with SSH to create the named pipes and start the `gpfdist` instances. You can use the Greenplum Database `gpssh-exkeys` utility with a list of all the source and destination primary hosts to exchange keys between Greenplum Database hosts.

Source and destination systems must be able to access the `gptransfer` work directory. The default directory is the user's home directory. You can specify a different directory with the `--work-base-dir` option.

The `gptransfer` utility logs messages in the `~/gpAdminLogs` directory on the master host. `gptransfer` creates a log file with the name `gptransfer_date.log` and appends messages to it each time it runs on that day. You can specify a different directory for the log file using the `-l log\_directory` option.

Work directories named `~/gptransfer_process\_id` are created on segment hosts in the source cluster. Log files for the `gpfdist` instances that `gptransfer` creates are in these directories. Adding the `--gpfdist-verbose` or `--gpfdist-very-verbose` options to the `gptransfer` command line increases the `gpfdist` logging level.

The `gptransfer` utility does not move configuration files such as `postgresql.conf` and `pg_hba.conf`. You must set up the destination system configuration separately.

The `gptransfer` utility does not move external objects such as Greenplum Database extensions, third party jar files, and shared object files. You must install the external objects separately.

The `gptransfer` utility does not move dependent database objects unless you specify the `--full` option. For example, if a table has a default value on a column that is a user-defined function, that function must exist in the destination system database when using the `-t`, `-d`, or `-f` options.

If you move a set of database tables with the `-d`, `-t`, or `-f` option, and the destination table or database does not exist, `gptransfer` creates it. The utility re-creates any indexes on tables before copying data.

If a table exists on the destination system and one of the options `--skip-existing`, `--truncate`, or `--drop` is not specified, `gptransfer` returns an error and quits.

If an error occurs when during the process of copying a table, or table validation fails, `gptransfer` continues copying the other specified tables. After `gptransfer` finishes, it displays a list of tables where an error occurred, writes the names of tables that failed into a text file, and then prints the name of the file. You can use this file with the `gptransfer` -f option to retry copying tables.

The name of the file that contains the list of tables where errors occurred is `failed_migrated_tables_`yyyymmdd\_hhmmss`.txt`. The yyyymmdd\_hhmmss is a time stamp when the `gptransfer` process was started. The file is created in the directory were `gptransfer` is executed.

After `gptransfer` completes copying database objects, the utility compares the row count of each table copied to the destination databases with the table in the source database. The utility returns the validation results for each table. You can disable the table row count validation by specifying the `--no-final-count` option.

**Note:** If the number of rows do not match, the table is not added to the file that lists the tables where transfer errors occurred.

The `gp_external_max_segs` server configuration parameter controls the number of segment instances that can access a single `gpfdist` instance simultaneously. Setting a low value might affect `gptransfer` performance. For information about the parameter, see the *Greenplum Database Reference Guide*.

**Limitation for the Source and Destination Systems**

If you are copying data from a system with a larger number of segments to a system with a fewer number of segment hosts, then the total number of primary segments on the destination system must be greater than or equal to the total number of segment hosts on the source system.

For example, assume a destination system has a total of 24 primary segments. This means that the source system cannot have more than 24 segment hosts.

When you copy data from a source Greenplum Database system with a larger number of primary segment instances than on the destination system, the data transfer might be slower when compared to a transfer where the source system has fewer segment instances than the destination system. The `gptransfer` utility uses a different configuration of named pipes and `gpfdist` instances in the two situations.

**Validating Table Data with SHA-256**

Validating table data with SHA-256 \(specifying the option `--validate=sha256`\) requires the Greenplum Database pgcrypto extension. The extension is included with Tanzu Greenplum 5.x. When copying data from a supported Tanzu Greenplum 4.3.x system, the extension package must be installed on the 4.3.x system. You do not need to run `pgcrypto.sql` to install the pgcrypto functions in a Greenplum 4.3.x database.

## Options 

-a
:   Quiet mode, do not prompt the user for confirmation.

--analyze
:   Run the `ANALYZE` command on non-system tables. The default is to not run the `ANALYZE` command.

--base-port=base\_gpfdist\_port
:   Base port for `gpfdist` on source segment systems. If not specified, the default is 8000.

--batch-size=batch\_size
:   Sets the maximum number of tables that `gptransfer` concurrently copies to the destination database. If not specified, the default is 2. The maximum is 10.

    **Note:** If the order of the transfer is important, specify a value of 1. The tables are transferred sequentially based on the order specified in the `-t` and `-f` options.

-d database
:   A source database to copy. This option can be specified multiple times to copy multiple databases to the destination system. All the user defined tables and table data are copied to the destination system.

:   A set of databases can be specified using the Python regular expression syntax. The regular expression pattern must be enclosed in slashes \(`/RE\_pattern/`\). If you use a regular expression, the name must be enclosed in double quotes \("\). This example `-d "demo/.*/"` specifies all databases in the Greenplum Database installation that begin with `demo`.

    **Note:** Note the following two examples for the `-d` option are equivalent. They both specify a set of databases that begins with `demo` and ends with zero or more digits.

    ```
    -d "demo/[0-9]*/"
    -d "/demo[0-9]*/"
    ```

:   If the source database does not exist, `gptransfer` returns an error and quits. If a destination database does not exist a database is created.

:   Not valid with the `--full`, `-f`, `-t`, `--partition-transfer`, or `--partition-transfer-non-partition-target` options.

:   Alternatively, specify the `-t` or `-f` option to copy a specified set of tables.

--delimiter=delim
:   Delimiter to use for writable external tables created by `gptransfer`. Specify a single ASCII character that separates columns within each row of data. The default value is a comma \(`,`\). If delim is a comma \(`,`\) or if this option is not specified, `gptransfer` uses the `CSV` format for writable external tables. Otherwise, `gptransfer` uses the `TEXT` format.

:   If `--delimiter`, `--format`, and `--quote` options are not specified, these are settings for writable external tables:

:   `FORMAT 'CSV' ( DELIMITER ',' QUOTE E'\001' )`

:   You can specify a delimiter character such as a non-printing character with the format `"\digits"` \(octal\). A backslash followed by the octal value for the character. The octal format must be enclosed in double quotes. This example specifies the octal character `\001`, the `SOH` character:

    ```
    --delimiter="\001"
    ```

--dest-database=dest\_database\_name
:   The database in the destination Greenplum Database system. If not specified, the source tables are copied into a destination system database with the same name as the source system database.

:   This option is required if the source and destination Greenplum Database systems are the same.

:   If destination database does not exist, it is created.

:   Not valid with the `--full`, `--partition-transfer`, or `--partition-transfer-non-partition-target` options.

--dest-host=dest\_host
:   Destination Greenplum Database hostname or IP address. If not specified, the default is the host the system running `gptransfer` \(127.0.0.1\)

--dest-port=dest\_port
:   Destination Greenplum Database port number, If not specified, the default is 5432.

--dest-user=dest\_user
:   User ID that is used to connect to the destination Greenplum Database system. If not specified, the default is the user gpadmin.

--drop
:   Specify this option to drop the table that is in the destination database if it already exists. Before copying table data, `gptransfer` drops the table and creates it again.

:   At most, only one of the options can be specified `--skip-existing`, `--truncate`, or `--drop`. If one of them is not specified and the table exists in the destination system, `gptransfer` returns an error and quits.

:   Not valid with the `--full`, `--partition-transfer`, or `--partition-transfer-non-partition-target` options.

--dry-run
:   When you specify this option, `gptransfer` generates a list of the migration operations that would have been performed with the specified options. The data is not migrated.

:   The information is displayed at the command line and written to the log file.

-f table-file
:   The location and name of file containing list of fully qualified table names to copy from the Greenplum Database source system. In the text file, you specify a single fully qualified table per line \(database.schema.table\).

:   A set of tables can be specified using the Python regular expression syntax. See the `-d` option for information about using regular expressions.

:   If the source table does not exist, `gptransfer` returns an error and quits. If the destination database or table does not exist, it is created.

:   Only the table and table data are copied and indexes are re-created. Dependent objects are not copied.

:   You cannot specify views, or system catalog tables. The `--full` option copies user defined views.

:   If you specify the `-d` option to copy all the tables from a database, you cannot specify individual tables from the database.

:   Not valid with the `--full`, `-d`, or `-t` options.

:   --partition-transfer \(partitioned destination table\)
:   Specify this option with the `-f` option to copy data from leaf child partition tables of partitioned tables from a source database to the leaf child partition tables in a destination database. The text file specified by the `-f` option contains a list of fully qualified leaf child partition table names with this syntax.

:   ```
<src_db>.<src_schema>.<src_prt_tbl>[, <dst_db>.<dst_schema>.<dst_prt_tbl>]
```

:   Wildcard characters are not supported in the fully qualified table names. The destination partitioned table must exist. If the destination leaf child partition table is not specified in the file, `gptransfer` copies the data to the same fully qualified table name \(db\_name.schema.table\) in the destination Greenplum Database system. If the source and destination Greenplum Database systems are the same, you must specify a destination table where at least one of the following must be different between the source and destination table: db\_name, schema, or table.

:   If either the source or destination table is not a leaf child partition, the utility returns an error and no data are transferred.

:   These characteristics must be the same for the partitioned table in the source and destination database.

    -   Number of table columns and the order of the column data types \(the source and destination table names and table column names can be different\)
    -   Partition level of the specified source and destination tables
    -   Partitioning criteria of the specified source and destination leaf child partitions and child partitions above them in the hierarchy \(partition type and partition column\)

:   This option is not valid with these options: `-d`, `--dest-database`, `--drop`, `-F`, `--full`, `--schema-only`, `-T`, `-t`.

    **Note:** If a destination table is not empty or the data in the source or destination table changes during a transfer operation \(rows are inserted or deleted\), the table row count validation fails due to row count mismatch.

    If the destination table is not empty, you can specify the `-truncate` option to truncate the table before the transfer operation.

    You can specify the `-x` option to acquire exclusive locks on the tables during a transfer operation.

--partition-transfer-non-partition-target \(non-partitioned destination table\)
:   Specify this option with the `-f` option to copy data from leaf child partition tables of partitioned tables in a source database to non-partitioned tables in a destination database. The text file specified by the `-f` option contains a list of fully qualified leaf child partition table names in the source database and non-partitioned tables names in the destination database with this syntax.

    ```
    <src_db>.<src_schema>.<src_part_tbl>, <dest_db>.<dest_schema>.<dest_tbl>
    ```

:   Wildcard characters are not supported in the fully qualified table names. The destination tables must exist, and both source and destination table names are required in the file.

:   If a source table is not a leaf child partition table or a destination table is not a normal \(non-partitioned\) table, the utility returns an error and no data are transferred.

:   If the source and destination Greenplum Database systems are the same, you must specify a destination table where at least one of the following must be different between the source and destination table: db\_name, schema, or table.

:   For the partitioned table in the source database and the table in the destination database, the number of table columns and the order of the column data types must be the same \(the source and destination table column names can be different\).

:   The same destination table can be specified in the file for multiple source leaf child partition tables that belong to a single partitioned table. Transferring data from source leaf child partition tables that belong to different partitioned tables to a single non-partitioned table is not supported.

:   This option is not valid with these options: `-d`, `--dest-database`, `--drop`, `-F`, `--full`, `--schema-only`, `-T`, `-t`, `--truncate`, `--validate`.

:   **Note:** If the data in the source or destination table changes during a transfer operation \(rows are inserted or deleted\), the table row count validation fails due to row count mismatch.

You can specify the `-x` option to acquire exclusive locks on the tables during a transfer operation.

-F table-file
:   The location and name of file containing list of fully qualified table names to exclude from transferring to the destination system. In the text file, you specify a single fully qualified table per line.

:   A set of tables can be specified using the Python regular expression syntax. See the `-d` option for information about using regular expressions.

:   The utility removes the excluded tables from the list of tables that are being transferred to the destination database before starting the transfer. If excluding tables results in no tables being transferred, the database or schema is not created in the destination system.

:   If a source table does not exist, `gptransfer` displays a warning.

:   Only the specified tables are excluded. To exclude dependent objects, you must explicitly specify them.

:   You cannot specify views, or system catalog tables.

:   Not valid with the `--full`, `--partition-transfer`, or `--partition-transfer-non-partition-target` options.

:   You can specify the `--dry-run` option to test the command. The `-v` option, displays and logs the excluded tables.

--format=\[CSV \| TEXT\]
:   Specify the format of the writable external tables that are created by `gptransfer` to transfer data. Values are `CSV` for comma separated values, or `TEXT` for plain text. The default value is `CSV`.

:   If the options `--delimiter`, `--format`, and `--quote` are not specified, these are default settings for writable external tables:

:   `FORMAT 'CSV' ( DELIMITER ',' QUOTE E'\001' )`

:   If you specify `TEXT`, you must also specify a non-comma delimiter with the `--delimiter=delim` option. These are settings for writable external tables:

:   `FORMAT 'TEXT' ( DELIMITER delim ESCAPE 'off' )`

--full
:   Full migration of a Greenplum Database source system to a destination system. You must specify the options for the destination system, the `--source-map-file` option, the `--dest-host` option, and if necessary, the other destination system options.

:   The `--full` option cannot be specified with the `-t`, `-d`, `-f`, `--partition-transfer`, or `--partition-transfer-non-partition-target` options.

:   A full migration copies all database objects including, tables, indexes, views, users, roles, functions, and resource queues for all user defined databases. The default databases, postgres, template0 and template1 are not moved.

:   If a database exists in the destination system, besides the default postgres, template0 and template1 databases, `gptransfer` returns an error and quits.

    **Note:** The `--full` option is recommended only when the databases contain a large number of tables with large amounts of data. Because of the overhead required to set up parallel transfers, the utility is not recommended when the databases contain tables with small amounts of data. For more information, see [Notes](#section4).

--gpfdist-verbose
:   Set the logging level for `gpfdist` processes to verbose \(`-v`\). Cannot be specified with `--gpfdist-very-verbose`.

:   The output is recorded in `gpfdist` log files in the `~/gptransfer_process\_id` directory on segment hosts in the source Greenplum Database cluster.

--gpfdist-very-verbose
:   Set the logging level for `gpfdist` processes to very verbose \(`-V`\). Cannot be specified with `--gpfdist-verbose`.

:   The output is recorded in `gpfdist` log files in the `~/gptransfer_process\_id` directory on segment hosts in the source Greenplum Database cluster.

-l log\_dir
:   Specify the `gptransfer` log file directory. If not specified, the default is `~/gpAdminLogs`. This directory is created on the master host in the source Greenplum cluster.

--max-line-length=length
:   Sets the maximum allowed data row length in bytes for the `gpfdist` utility. If not specified, the default is 10485760. Valid range is 32768 \(32K\) to 268435456 \(256MB\).

:   Should be used when user data includes very wide rows \(or when `line too long` error message occurs\). Should not be used otherwise as it increases resource allocation.

--no-final-count
:   Disable table row count validation that is performed after `gptransfer` completes copying database objects to the target database. The default is to compare the row count of tables copied to the destination databases with the tables in the source database.

-q \| --quiet
:   If specified, suppress status messages. Messages are only sent to the log file.

--quote=character
:   The quotation character when `gptransfer` creates writable external tables with the `CSV` format. Specify a single ASCII character that is used to enclose column data. The default value is the octal character `\001`, the `SOH` character.

:   You can specify a delimiter character such as a non-printing character with the format `"\digits"` \(octal\). A backslash followed by the octal value for the character. The octal value must be enclosed in double quotes.

--schema-only
:   Create only the schemas specified by the command. Data is not transferred.

:   If specified with the `--full` option, `gptransfer` replicates the complete database schema, including all tables, indexes, views, user defined types \(UDT\), and user defined functions \(UDF\) for the source databases. No data is transferred.

:   If you specify databases with the `-d` option or tables with the `-t` or `-f` options, `gptransfer` creates only the tables and indexes. No data is transferred.

:   Not valid with the `--partition-transfer`, `--partition-transfer-non-partition-target`, or `--truncate` options.

    **Note:** Because of the overhead required to set up parallel transfers, the `--schema-only` option is not recommended when transferring information for a large number of tables. For more information, see [Notes](#section4).

--skip-existing
:   Specify this option to skip copying a table from the source database if the table already exists in the destination database.

:   At most, only one of the options can be specified `--skip-existing`, `--truncate`, or `--drop`. If one of them is not specified and the table exists in the destination system, `gptransfer` returns an error and quits.

:   Not valid with the `--full` option.

--source-host=source\_host
:   Source Greenplum Database host name or IP address. If not specified, the default host is the system running `gptransfer` \(127.0.0.1\).

--source-map-file=host\_map\_file
:   File that lists source segment host name and IP addresses. If the file is missing or not all segment hosts are listed, `gptransfer` returns an error and quits.

:   Each line of the file contains a source host name and the host IP address separated by a comma: `hostname,IPaddress`. This example lists four Greenplum Database hosts and their IP addresses.

:   ```
sdw1,192.0.2.1
sdw2,192.0.2.2
sdw3,192.0.2.3
sdw4,192.0.2.4
```

:   This option is required if the `--full` option is specified or if the source Greenplum Database system is different than the destination system. This option is not required if source and destination systems are the same.

--source-port=source\_port
:   Source Greenplum Database port number. If not specified, the default is 5432.

--source-user=source\_user
:   User ID that is used to connect to the source Greenplum Database system. If not specified, the default is the user gpadmin.

    **Note:** `gptransfer` creates external tables with the `execute` protocol, so the `--source_user` that you specify must have superuser privileges.

--sub-batch-size=sub\_batch\_size
:   Specifies the maximum degree of parallelism of the operations performed when migrating a table such as starting gpfdist instances, creating named pipes for the move operations. If not specified, the default is 25. The maximum is 50.

:   Specify the `--batch-size` option to control the maximum number of tables that `gptransfer` concurrently processes.

-t db.schema.table
:   A table from the source database system to copy. The fully qualified table name must be specified.

:   A set of tables can be specified using the Python regular expression syntax. See the `-d` option for information about using regular expressions.

:   If the destination table or database does not exist, it is created. This option can be specified multiple times to include multiple tables. Only the table and table data are copied and indexes are re-created. Dependent objects are not copied.

:   If the source table does not exist, `gptransfer` returns an error and quits.

:   If you specify the `-d` option to copy all the tables from a database, you do not need to specify individual tables from the database.

:   Not valid with the `--full`, `-d`, `-f`, `--partition-transfer`, or `--partition-transfer-non-partition-target` options.

-T db.schema.table
:   A table from the source database system to exclude from transfer. The fully qualified table name must be specified.

:   A set of tables can be specified using the Python regular expression syntax. See the `-d` option for information about using regular expressions.

:   This option can be specified multiple times to include multiple tables. Only the specified tables are excluded. To exclude dependent objects, you must explicitly specify them.

:   The utility removes the excluded tables from the list of tables that are being transferred to the destination database before starting the transfer. If excluding tables results in no tables being transferred, the database or schema is not created in the destination system.

:   If a source table does not exist, `gptransfer` displays a warning.

:   Not valid with the `--full`, `--partition-transfer`, or `--partition-transfer-non-partition-target`options.

:   You can specify the `--dry-run` option to test the command. The `-v` option displays and logs the excluded tables.

--timeout seconds
:   Specify the time out value in seconds that `gptransfer` passes the `gpfdist` processes that `gptransfer` uses. The value is the time allowed for Greenplum Database to establish a connection to a `gpfdist` process. You might need to increase this value when operating on high-traffic networks.

:   The default value is 300 seconds \(5 minutes\). The minimum value is 2 seconds, the maximum value is 600 seconds.

--truncate
:   Specify this option to truncate the table that is in the destination database if it already exists.

:   At most, only one of the options can be specified `--skip-existing`, `--truncate`, or `--drop`. If one of them is not specified and the table exists in the destination system, `gptransfer` returns an error and quits.

:   Not valid with the `--full` option.

--validate=type
:   Perform data validation on table data. These are the supported types of validation.

:   `count` - Specify this value to compare row counts between source and destination table data.

:   `MD5` - Specify this value to compare MD5 values between source and destination table data.

:   If validation for a table fails, `gptransfer` displays the name of the table and writes the file name to the text file `failed_migrated_tables_`yyyymmdd\_hhmmss`.txt`. The yyyymmdd\_hhmmss is a time stamp when the `gptransfer` process was started. The file is created in the directory where `gptransfer` is executed.

    **Note:** The file contains the table names where validation failed or other errors occurred during table migration.

-v \| --verbose
:   If specified, sets the logging level to verbose. Additional log information is written to the log file and the command line during command execution.

--work-base-dir=work\_dir
:   Specify the directory that `gptransfer` uses to store temporary working files such as PID files and named pipes. The default directory is the user's home directory.

:   Source and destination systems must be able to access the `gptransfer` work directory.

-x
:   Acquire an exclusive lock on tables during the migration to prevent insert or updates.

:   On the source database, an exclusive lock is acquired when `gptransfer` inserts into the external table and is released after validation.

:   On the destination database, an exclusive lock is acquired when `gptransfer` selects from external table and released after validation.

:   If `-x` option is not specified and `--validate` is specified, validation failures occur if data is inserted into either the source or destination table during the migration process. The `gptransfer` utility displays messages if validation errors occur.

-h \| -? \| --help
:   Displays the online help.

--version
:   Displays the version of this utility.

## Examples 

This command copies the table `public.t1` from the database `db1` and all tables in the database `db2` to the system `mytest2`.

```
gptransfer -t db1.public.t1 -d db2 --dest-host=mytest2 \
 --source-map-file=gp-source-hosts --truncate
```

If the databases `db1` and `db2` do not exist on the system `mytest2`, they are created. If any of the source tables exist on the destination system, `gptransfer` truncates the table and copies the data from the source to the destination table.

This command copies leaf child partition tables from a source system to a destination system.

```
gptransfer -f input_file --partition-transfer --source-host=source_host \
 --source-user=source_user --source-port=source_port --dest-host=dest_host \
 --dest-user=dest_user --dest-port=dest_port --source-map-file=host_map_file
```

This line in `input_file` copies a leaf child partition from the source system to the destination system.

```
 srcdb.people.person_1_prt_experienced, destdb.public.employee_1_prt_seniors
```

The line assumes partitioned tables in the source and destination systems similar to the following tables.

-   In the *people* schema of the *srcdb* database of the source system, a partitioned table with a leaf child partition table `person_1_prt_experienced`. This `CREATE TABLE` command creates a partitioned table with the leaf child partition table.

    ```
    CREATE TABLE person(id int, title char(1))
      DISTRIBUTED BY (id)
      PARTITION BY list (title)
      (PARTITION experienced VALUES ('S'),
        PARTITION entry_level VALUES ('J'),
        DEFAULT PARTITION other );
    ```

-   In the *public* schema of the *destdb* database of the source system, a partitioned table with a leaf child partition table `public.employee_1_prt_seniors`. This `CREATE TABLE` command creates a partitioned table with the leaf child partition table.

    ```
    CREATE TABLE employee(id int, level char(1))
      DISTRIBUTED BY (id)
      PARTITION BY list (level)
      (PARTITION seniors VALUES ('S'),
        PARTITION juniors VALUES ('J'),
        DEFAULT PARTITION other );
    ```


This example uses Python regular expressions in a filter file to specify the set of tables to transfer. This command specifies the `-f` option with the filter file `/tmp/filter_file` to limit the tables that are transferred.

```
gptransfer -f /tmp/filter_file --source-port 5432 --source-host test4 \
 --source-user gpadmin --dest-user gpadmin --dest-port 5432 --dest-host test1 \
 --source-map-file /home/gpadmin/source_map_file
```

This is the contents of `/tmp/filter_file`.

```
"test1.arc/.*/./.*/"
"test1.c/(..)/y./.*/"
```

In the first line, the regular expressions for the schemas, `arc/.*/`, and for the tables, `/.*/`, limit the transfer to all tables with the schema names that start with `arc`.

In the second line, the regular expressions for the schemas, `c/(..)/y`, and for the tables, `/.*/`, limit the transfer to all tables with the schema names that are four characters long and that start with `c` and end with `y`, for example, `crty`.

When the command is run, tables in the database `test1` that satisfy either condition are transferred to the destination database.

## See Also 

[gpfdist](gpfdist.html)

For information about loading and unloading data, see the *Greenplum Database Administrator Guide*.

