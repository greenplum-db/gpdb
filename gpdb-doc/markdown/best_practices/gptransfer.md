# Migrating Data with gptransfer 

Information about the `gptransfer` migration utility, which transfers Greenplum Database metadata and data from one Greenplum database to another.

**Note:** Greenplum Database provides two utilities for migrating data between Greenplum Database installations, [gpcopy](../admin_guide/managing/../../utility_guide/admin_utilities/gpcopy.html) and `gptransfer`. Use `gpcopy` to migrate data to a Greenplum Database cluster version 5.9.0 and later. Use the `gptransfer` utility to migrate data between Greenplum Database installations running software version 5.8.0 or earlier.

The `gptransfer` utiltiy is deprecated and will be removed in the next major release of Greenplum Database. For information about migrating data with `gpcopy`, see [Migrating Data with gpcopy](../admin_guide/managing/gpcopy-migrate.html).

The `gptransfer` migration utility transfers Greenplum Database metadata and data from one Greenplum database to another Greenplum database, allowing you to migrate the entire contents of a database, or just selected tables, to another database. The source and destination databases may be in the same or a different cluster. Data is transferred in parallel across all the segments, using the `gpfdist` data loading utility to attain the highest transfer rates.

`gptransfer` handles the setup and execution of the data transfer. Participating clusters must already exist, have network access between all hosts in both clusters, and have certificate-authenticated ssh access between all hosts in both clusters.

The interface includes options to transfer one or more full databases, or one or more database tables. A full database transfer includes the database schema, table data, indexes, views, roles, user-defined functions, and resource queues. Configuration files, including `postgresql.conf` and `pg_hba.conf`, must be transferred manually by an administrator. Extensions installed in the database with `gppkg`, such as MADlib, must be installed in the destination database by an administrator.

See the *Greenplum Database Utility Guide* for complete syntax and usage information for the `gptransfer` utility.

## Prerequisites 

-   The `gptransfer` utility can only be used with Greenplum Database. Apache HAWQ is not supported as a source or destination.
-   The source and destination Greenplum clusters must both be version 4.2 or higher.
-   At least one Greenplum instance must include the `gptransfer` utility in its distribution. If neither the source or destination includes `gptransfer`, you must upgrade one of the clusters to use `gptransfer`.
-   Run the `gptransfer` utility either from the source or the destination cluster.
-   If you are transferring data between two different clusters, the number of *segment instances* in the destination cluster must be greater than or equal to the number of segment *hosts* in the source cluster. The number of segment hosts in the destination cluster may be smaller than the number of segment hosts in the source, but the data will transfer at a slower rate.
-   The segment hosts in both clusters must have network connectivity with each other.
-   Every host in both clusters must be able to connect to every other host with certificate-authenticated SSH. You can use the `gpssh_exkeys` utility to exchange public keys between the hosts of both clusters.

## What gptransfer Does 

`gptransfer` uses writable and readable external tables, the Greenplum `gpfdist` parallel data-loading utility, and named pipes to transfer data from the source database to the destination database. Segments on the source cluster select from the source database table and insert into a writable external table. Segments in the destination cluster select from a readable external table and insert into the destination database table. The writable and readable external tables are backed by named pipes on the source cluster's segment hosts, and each named pipe has a `gpfdist` process serving the pipe's output to the readable external table on the destination segments.

`gptransfer` orchestrates the process by processing the database objects to be transferred in batches. For each table to be transferred, it performs the following tasks:

-   creates a writable external table in the source database
-   creates a readable external table in the destination database
-   creates named pipes and `gpfdist` processes on segment hosts in the source cluster
-   executes a `SELECT INTO` statement in the source database to insert the source data into the writable external table
-   executes a `SELECT INTO` statement in the destination database to insert the data from the readable external table into the destination table
-   optionally validates the data by comparing row counts or MD5 hashes of the rows in the source and destination
-   cleans up the external tables, named pipes, and `gpfdist` processes

## Fast Mode and Slow Mode 

`gptransfer` sets up data transfer using the `gpfdist` parallel file serving utility, which serves the data evenly to the destination segments. Running more `gpfdist` processes increases the parallelism and the data transfer rate. When the destination cluster has the same or a greater number of segments than the source cluster, `gptransfer` sets up one named pipe and one `gpfdist` process for each source segment. This is the configuration for optimal data transfer rates and is called *fast mode*. The following figure illustrates a setup on a segment host when the destination cluster has at least as many segments as the source cluster.

![](../admin_guide/managing/../graphics/gptransfer-fast.png)

The configuration of the input end of the named pipes differs when there are fewer segments in the destination cluster than in the source cluster. `gptransfer` handles this alternative setup automatically. The difference in configuration means that transferring data into a destination cluster with fewer segments than the source cluster is not as fast as transferring into a destination cluster of the same or greater size. It is called *slow mode* because there are fewer `gpfdist` processes serving the data to the destination cluster, although the transfer is still quite fast with one `gpfdist` per segment host.

When the destination cluster is smaller than the source cluster, there is one named pipe per segment host and all segments on the host send their data through it. The segments on the source host write their data to a writable external web table connected to a `gpfdist` process on the input end of the named pipe. This consolidates the table data into a single named pipe. A `gpfdist` process on the output of the named pipe serves the consolidated data to the destination cluster. The following figure illustrates this configuration.

![](../admin_guide/managing/../graphics/gptransfer-slow.png)

On the destination side, `gptransfer` defines a readable external table with the `gpfdist` server on the source host as input and selects from the readable external table into the destination table. The data is distributed evenly to all the segments in the destination cluster.

## Batch Size and Sub-batch Size 

The degree of parallelism of a `gptransfer` execution is determined by two command-line options: `--batch-size` and `--sub-batch-size`. The `--batch-size` option specifies the number of tables to transfer in a batch. The default batch size is 2, which means that two table transfers are in process at any time. The minimum batch size is 1 and the maximum is 10. The `--sub-batch-size` parameter specifies the maximum number of parallel sub-processes to start to do the work of transferring a table. The default is 25 and the maximum is 50. The product of the batch size and sub-batch size is the amount of parallelism. If set to the defaults, for example, `gptransfer` can perform 50 concurrent tasks. Each thread is a Python process and consumes memory, so setting these values too high can cause a Python Out of Memory error. For this reason, the batch sizes should be tuned for your environment.

## Preparing Hosts for gptransfer 

When you install a Greenplum Database cluster, you set up all the master and segment hosts so that the Greenplum Database administrative user \(`gpadmin`\) can connect with SSH from every host in the cluster to any other host in the cluster without providing a password. The `gptransfer` utility requires this capability between every host in the source and destination clusters. First, ensure that the clusters have network connectivity with each other. Then, prepare a hosts file containing a list of all the hosts in both clusters, and use the `gpssh-exkeys` utility to exchange keys. See the reference for `gpssh-exkeys` in the *Greenplum Database Utility Guide*.

The host map file is a text file that lists the segment hosts in the source cluster. It is used to enable communication between the hosts in Greenplum clusters. The file is specified on the `gptransfer` command line with the `--source-map-file=*host\_map\_file*` command option. It is a required option when using `gptransfer` to copy data between two separate Greenplum clusters.

The file contains a list in the following format:

```
host1_name,host1_ip_addr
host2_name,host2_ipaddr
...
```

The file uses IP addresses instead of host names to avoid any problems with name resolution between the clusters.

## Limitations 

`gptransfer` transfers data from user databases only; the `postgres`, `template0`, and `template1` databases cannot be transferred. Administrators must transfer configuration files manually and install extensions into the destination database with `gppkg`.

The destination cluster must have at least as many segments as the source cluster has segment hosts. Transferring data to a smaller cluster is not as fast as transferring data to a larger cluster.

Transferring small or empty tables can be unexpectedly slow. There is significant fixed overhead in setting up external tables and communications processes for parallel data loading between segments that occurs whether or not there is actual data to transfer. It can be more efficient to transfer the schema and smaller tables to the destination database using other methods, then use `gptransfer` with the `-t` option to transfer large tables.

When transferring data between databases, you can run only one instance of `gptransfer` at a time.Running multiple, concurrent instances of `gptransfer` is not supported.

## Full Mode and Table Mode 

When run with the `--full` option, `gptransfer` copies all user-created databases, tables, views, indexes, roles, user-defined functions, and resource queues in the source cluster to the destination cluster. The destination system cannot contain any user-defined databases, only the default databases postgres, template0, and template1. If `gptransfer` finds a database on the destination it fails with a message like the following:

```
[ERROR]:- gptransfer: error: --full option specified but tables exist on destination system
```

**Note:** The `--full` option cannot be specified with the `-t`, `-d`, `-f`, or `--partition-transfer` options.

To copy tables individually, specify the tables using either the `-t` command-line option \(one option per table\) or by using the `-f` command-line option to specify a file containing a list of tables to transfer. Tables are specified in the fully-qualified format `*database*.*schema*.*table*`. The table definition, indexes, and table data are copied. The database must already exist on the destination cluster.

By default, `gptransfer` fails if you attempt to transfer a table that already exists in the destination database:

```
[INFO]:-Validating transfer table set...
[CRITICAL]:- gptransfer failed. (Reason='Table *database.schema.table* exists in database *database* .') exiting...
```

Override this behavior with the `--skip-existing`, `--truncate`, or `--drop` options.

The following table shows the objects that are copied in full mode and table mode.

|Object|Full Mode|Table Mode|
|------|---------|----------|
|Data|Yes|Yes|
|Indexes|Yes|Yes|
|Roles|Yes|No|
|Functions|Yes|No|
|Resource Queues|Yes|No|
|`postgresql.conf`|No|No|
|`pg_hba.conf`|No|No|
|`gppkg`|No|No|

The `--full` option and the `--schema-only` option can be used together if you want to copy databases in phases, for example, during scheduled periods of downtime or low activity. Run `gptransfer --full --schema-only ...` to create the databases on the destination cluster, but with no data. Then you can transfer the tables in stages during scheduled down times or periods of low activity. Be sure to include the `--truncate` or `--drop` option when you later transfer tables to prevent the transfer from failing because the table already exists at the destination.

## Locking 

The `-x` option enables table locking. An exclusive lock is placed on the source table until the copy and validation, if requested, are complete.

## Validation 

By default, `gptransfer` does not validate the data transferred. You can request validation using the `--validate=*type*` option. The validation *type* can be one of the following:

-   `count` – Compares the row counts for the tables in the source and destination databases.
-   `md5` – Sorts tables on both source and destination, and then performs a row-by-row comparison of the MD5 hashes of the sorted rows.

If the database is accessible during the transfer, be sure to add the `-x` option to lock the table. Otherwise, the table could be modified during the transfer, causing validation to fail.

## Failed Transfers 

A failure on a table does not end the `gptransfer` job. When a transfer fails, `gptransfer` displays an error message and adds the table name to a failed transfers file. At the end of the `gptransfer` session, `gptransfer` writes a message telling you there were failures, and providing the name of the failed transfer file. For example:

```
[WARNING]:-Some tables failed to transfer. A list of these tables
[WARNING]:-has been written to the file failed_transfer_tables_20140808_101813.txt
[WARNING]:-This file can be used with the -f option to continue
```

The failed transfers file is in the format required by the `-f` option, so you can use it to start a new `gptransfer` session to retry the failed transfers.

## Best Practices 

`gptransfer` creates a configuration that allows transferring large amounts of data at very high rates. For small or empty tables, however, the `gptransfer` set up and clean up are too expensive. The best practice is to use `gptransfer` for large tables and to use other methods for copying smaller tables.

1.  Before you begin to transfer data, replicate the schema or schemas from the source cluster to the destination cluster. Options for copying the schema include:
    -   Use the PostgreSQL `pg_dump` or `pg_dumpall` utility with the `–-schema-only` option.
    -   DDL scripts, or any other method for recreating schema in the destination database.
2.  Divide the non-empty tables to be transferred into large and small categories, using criteria of your own choice. For example, you could decide large tables have more than one million rows or a raw data size greater than 1GB.
3.  Transfer data for small tables using the SQL `COPY` command. This eliminates the warm-up/cool-down time each table incurs when using the `gptransfer` utility.
    -   Optionally, write or reuse existing shell scripts to loop through a list of table names to copy with the `COPY` command.
4.  Use `gptransfer` to transfer the large table data in batches of tables.
    -   It is best to transfer to the same size cluster or to a larger cluster so that `gptransfer` runs in fast mode.
    -   If any indexes exist, drop them before starting the transfer process.
    -   Use the `gptransfer` table \(`-t`\) or file \(`-f`\) options to execute the migration in batches of tables. Do not run `gptransfer` using the full mode; the schema and smaller tables have already been transferred.
    -   Perform test runs of the `gptransfer` process before the production migration. This ensures tables can be transferred successfully. You can experiment with the `--batch-size` and `--sub-batch-size` options to obtain maximum parallelism. Determine proper batching of tables for iterative `gptransfer` runs.
    -   Include the `--skip-existing` option because the schema already exists on the destination cluster.
    -   Use only fully qualified table names. Be aware that periods \(.\), whitespace, quotes \('\) and double quotes \("\) in table names may cause problems.
    -   If you decide to use the `--validation` option to validate the data after transfer, be sure to also use the `-x` option to place an exclusive lock on the source table.
5.  After all tables are transferred, perform the following tasks:
    -   Check for and correct any failed transfers.
    -   Recreate the indexes that were dropped before the transfer.
    -   Ensure any roles, functions, and resource queues are created in the destination database. These objects are not transferred when you use the `gptransfer -t` option.
    -   Copy the `postgresql.conf` and `pg_hba.conf` configuration files from the source to the destination cluster.
    -   Install needed extensions in the destination database with `gppkg`.

**Parent topic:** [Greenplum Database Best Practices](intro.html)

