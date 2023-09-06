---
title: Accessing External Data with External Tables
---

External tables enable you to access external data as if it were a regular database table. They are often used to move data into and out of a Greenplum database, and can utilize Greenplum parallelism by using the resources of all Greenplum Database segments to load or unload data.

When you create an external table definition, you specify the structure and format of the data, the data access *protocol*, the location of the external data source, and other protocol-specific or format-specific options.

> **Important** Greenplum 7 internally converts external tables to foreign tables, and internally operates on and represents the table using the foreign table data structures and catalog. Refer to [Understanding the External Table to Foreign Table Mapping](map_ext_to_foreign.html) for detailed information about this conversion, and its runtime implications.

**Parent topic:** [Working with External Data](../external/g-working-with-file-based-ext-tables.html)

## <a id="protocols"></a>About the External Table Data Access Protocols

An external table protocol identifies the data access method for an external data source. Greenplum Database supports the following built-in (automatically-enabled) and opt-in (you enable) external table protocols:

| Protocol Name | Description | Type |
|-------|-----------|------|
| `file://` | Use to access external data files on segment hosts that the Greenplum Database superuser \(`gpadmin`\) can access. Refer to [file:// Protocol](g-file-protocol.html). | Built-in |
| `gpfdist://` | Use to serve external data files to all Greenplum Database segments in parallel. See [gpfdist:// Protocol](g-gpfdist-protocol.html). | Built-in |
| `gpfdists://` | The secure version of `gpfdist`. See [gpfdists:// Protocol](g-gpfdists-protocol.html). | Built-in |
| `pxf://` | Use to access external object store systems \(Azure, Google Cloud Storage, Minio, S3-compatible\), Hadoop systems \(HDFS, Hive, HBase\), network file systems, and SQL databases with the Greenplum Platform Extension Framework \(PXF\). See [pxf:// Protocol](g-pxf-protocol.html). | Opt-in |
| `s3://` | Use to access files in an Amazon S3 bucket. See [s3:// Protocol](g-s3-protocol.html). | Opt-in |

A third party can also create a custom protocol that connects Greenplum Database to new external data sources. Refer to [Creating a Custom Protocol](../external/g-accessing-ext-files-custom-protocol.html) for more information.

The opt-in/custom and built-in protocols differ in these ways:

-   You must register opt-in and custom protocols. Built-in protocols are always available and cannot be unregistered.
-   When you register an opt-in or custom protocol, Greenplum adds a row to the `pg_extprotocol` catalog table to specify the handler functions that implement the protocol. The built-in protocols are not represented in this table.
-   You must have installed the opt-in or custom protocol's shared libraries on all Greenplum Database hosts. The built-in protocols have no additional libraries to install.
-   You use `GRANT [SELECT | INSERT | ALL] ON PROTOCOL <name>` to grant users permissions on opt-in and custom protocols. To allow \(or deny\) users access to the built-in protocols, you use the `CREATE ROLE` or `ALTER ROLE` commands to add the `CREATEEXTTABLE` \(or `NOCREATEEXTTABLE`\) attribute to each user's role.


## <a id="ops"></a>About External Table Operations

Greenplum Database provides both readable ([CREATE EXTERNAL TABLE](../../ref_guide/sql_commands/CREATE_EXTERNAL_TABLE.html)) and writable (`CREATE WRITABLE EXTERNAL TABLE`) external tables.

Readable external tables are typically used for data loading, and allow only [SELECT](../../ref_guide/sql_commands/SELECT.html) operations. They support:

-   Basic extraction, transformation, and loading \(ETL\) tasks common in data warehousing
-   Reading external table data in parallel from multiple Greenplum database segment instances, to optimize large load operations
-   Filter pushdown. If a query contains a `WHERE` clause, it may be passed to the external data source. Refer to the [gp\_external\_enable\_filter\_pushdown](../../ref_guide/config_params/guc-list.html) server configuration parameter discussion for more information. Note that this feature is currently supported only with the `pxf` protocol \(see [pxf:// Protocol](g-pxf-protocol.html)\).

Writable external tables are typically used for data unloading, and allow only [INSERT](../../ref_guide/sql_commands/INSERT.html) operations. They support:

-   Selecting data from database tables to insert into the writable external table
-   Sending data to an application as a stream of data. For example, unload data from Greenplum Database and send it to an application that connects to another database or ETL tool to load the data elsewhere.

## <a id="types"></a>About the External Table Types

External tables are typically file-based or web-based:

-   Regular \(file-based\) external tables can access static flat files. Regular external tables are rescannable: the data is static while the query runs.
-   Web \(web-based\) external tables access dynamic data sources, either on a web server with the `http://` protocol or by running OS commands or scripts. External web tables are not rescannable, the data can change while the query runs. Refer to [Creating and Using External Web Tables](../external/g-creating-and-using-web-external-tables.html) for more information.

The `pxf://` protocol can also access other types of external data, such as SQL databases.

## <a id="defining"></a>Creating an External Table

When you create an external table definition, you specify the structure and format of the data, the access protocol, the location of the external data source, and other protocol-specific or format-specific options.

[Examples for Creating External Tables](../external/g-creating-external-tables---examples.html) provides examples for different data types and different built-in protocols.

> **Important** After you create an external table, you must operate on the table using `ALTER EXTERNAL TABLE` and `DROP EXTERNAL TABLE` commands. VMware does not recommend mixing and matching external table and foreign table syntaxes for table maintenance operations.

## <a id="other"></a>Other Considerations

Greenplum Database backup and restore operations back up and restore only external and external web table *definitions*, not the data source data.

By default, if external table data contains an error, the command fails and no data loads into the target database table. See [Handling Errors in External Table Data](../external/g-handling-errors-ext-table-data.html) for more information.

