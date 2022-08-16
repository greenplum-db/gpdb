---
title: Defining External Tables 
---

External tables enable accessing external data as if it were a regular database table. They are often used to move data into and out of a Greenplum database.

To create an external table definition, you specify the format of your input files and the location of your external data sources. For information about input file formats, see [Formatting Data Files](../load/topics/g-formatting-data-files.html).

Use one of the following protocols to access external table data sources. You cannot mix protocols in `CREATE EXTERNAL TABLE` statements:

-   `file://` accesses external data files on segment hosts that the Greenplum Database superuser \(`gpadmin`\) can access. See [file:// Protocol](g-file-protocol.html).
-   `gpfdist://` points to a directory on the file host and serves external data files to all Greenplum Database segments in parallel. See [gpfdist:// Protocol](g-gpfdist-protocol.html).
-   `gpfdists://` is the secure version of `gpfdist`. See [gpfdists:// Protocol](g-gpfdists-protocol.html).
-   The `pxf://` protocol accesses object store systems \(Azure, Google Cloud Storage, Minio, S3\), external Hadoop systems \(HDFS, Hive, HBase\), and SQL databases using the Greenplum Platform Extension Framework \(PXF\). See [pxf:// Protocol](g-pxf-protocol.html).
-   `s3://` accesses files in an Amazon S3 bucket. See [s3:// Protocol](g-s3-protocol.html).

The `pxf://` and `s3://` protocols are custom data access protocols, where the `file://`, `gpfdist://`, and `gpfdists://` protocols are implemented internally in Greenplum Database. The custom and internal protocols differ in these ways:

-   `pxf://` and `s3://` are custom protocols that must be registered using the `CREATE EXTENSION` command \(`pxf`\) or the `CREATE PROTOCOL` command \(`s3`\). Registering the PXF extension in a database creates the `pxf` protocol. \(See [Accessing External Data with PXF](pxf-overview.html).\) To use the `s3` protocol, you must configure the database and register the `s3` protocol. \(See [Configuring the s3 Protocol](g-s3-protocol.html#s3_prereq).\) Internal protocols are always present and cannot be unregistered.
-   When a custom protocol is registered, a row is added to the `pg_extprotocol` catalog table to specify the handler functions that implement the protocol. The protocol's shared libraries must have been installed on all Greenplum Database hosts. The internal protocols are not represented in the `pg_extprotocol` table and have no additional libraries to install.
-   To grant users permissions on custom protocols, you use `GRANT [SELECT | INSERT | ALL] ON PROTOCOL`. To allow \(or deny\) users permissions on the internal protocols, you use `CREATE ROLE` or `ALTER ROLE` to add the `CREATEEXTTABLE` \(or `NOCREATEEXTTABLE`\) attribute to each user's role.

External tables access external files from within the database as if they are regular database tables. External tables defined with the `gpfdist`/`gpfdists`, `pxf`, and `s3` protocols utilize Greenplum parallelism by using the resources of all Greenplum Database segments to load or unload data. The `pxf` protocol leverages the parallel architecture of the Hadoop Distributed File System to access files on that system. The `s3` protocol utilizes the Amazon Web Services \(AWS\) capabilities.

You can query external table data directly and in parallel using SQL commands such as `SELECT`, `JOIN`, or `SORT EXTERNAL TABLE DATA`, and you can create views for external tables.

The steps for using external tables are:

1.  Define the external table.

    To use the `pxf` or `s3` protocol, you must also configure Greenplum Database and enable the protocol. See [pxf:// Protocol](g-pxf-protocol.html) or [s3:// Protocol](g-s3-protocol.html).

2.  Do one of the following:
    -   Start the Greenplum Database file server\(s\) when using the `gpfdist` or `gpdists` protocols.
    -   Verify the configuration for the PXF service and start the service.
    -   Verify the Greenplum Database configuration for the `s3` protocol.
3.  Place the data files in the correct locations.
4.  Query the external table with SQL commands.

Greenplum Database provides readable and writable external tables:

-   Readable external tables for data loading. Readable external tables support:

    -   Basic extraction, transformation, and loading \(ETL\) tasks common in data warehousing
    -   Reading external table data in parallel from multiple Greenplum database segment instances, to optimize large load operations
    -   Filter pushdown. If a query contains a `WHERE` clause, it may be passed to the external data source. Refer to the [gp\_external\_enable\_filter\_pushdown](../../ref_guide/config_params/guc-list.html) server configuration parameter discussion for more information. Note that this feature is currently supported only with the `pxf` protocol \(see [pxf:// Protocol](g-pxf-protocol.html)\).
    Readable external tables allow only `SELECT` operations.

-   Writable external tables for data unloading. Writable external tables support:

    -   Selecting data from database tables to insert into the writable external table
    -   Sending data to an application as a stream of data. For example, unload data from Greenplum Database and send it to an application that connects to another database or ETL tool to load the data elsewhere
    -   Receiving output from Greenplum parallel MapReduce calculations.
    Writable external tables allow only `INSERT` operations.


External tables can be file-based or web-based. External tables using the `file://` protocol are read-only tables.

-   Regular \(file-based\) external tables access static flat files. Regular external tables are rescannable: the data is static while the query runs.
-   Web \(web-based\) external tables access dynamic data sources, either on a web server with the `http://` protocol or by running OS commands or scripts. External web tables are not rescannable: the data can change while the query runs.

Greenplum Database backup and restore operations back up and restore only external and external web table *definitions*, not the data source data.

-   **[file:// Protocol](../external/g-file-protocol.html)**  
The `file://` protocol is used in a URI that specifies the location of an operating system file.
-   **[gpfdist:// Protocol](../external/g-gpfdist-protocol.html)**  
The `gpfdist://` protocol is used in a URI to reference a running `gpfdist` instance.
-   **[gpfdists:// Protocol](../external/g-gpfdists-protocol.html)**  
The `gpfdists://` protocol is a secure version of the `gpfdist:// protocol`.
-   **[pxf:// Protocol](../external/g-pxf-protocol.html)**  
You can use the Greenplum Platform Extension Framework \(PXF\) `pxf://` protocol to access data residing in object store systems \(Azure, Google Cloud Storage, Minio, S3\), external Hadoop systems \(HDFS, Hive, HBase\), and SQL databases.
-   **[s3:// Protocol](../external/g-s3-protocol.html)**  
The `s3` protocol is used in a URL that specifies the location of an Amazon S3 bucket and a prefix to use for reading or writing files in the bucket.
-   **[Using a Custom Protocol](../external/g-accessing-ext-files-custom-protocol.html)**  
A custom protocol allows you to connect Greenplum Database to a data source that cannot be accessed with the `file://`, `gpfdist://`, or `pxf://` protocols.
-   **[Handling Errors in External Table Data](../external/g-handling-errors-ext-table-data.html)**  
By default, if external table data contains an error, the command fails and no data loads into the target database table.
-   **[Creating and Using External Web Tables](../external/g-creating-and-using-web-external-tables.html)**  
External web tables allow Greenplum Database to treat dynamic data sources like regular database tables. Because web table data can change as a query runs, the data is not rescannable.
-   **[Examples for Creating External Tables](../external/g-creating-external-tables---examples.html)**  
These examples show how to define external data with different protocols. Each `CREATE EXTERNAL TABLE` command can contain only one protocol.

**Parent topic:** [Working with External Data](../external/g-working-with-file-based-ext-tables.html)

