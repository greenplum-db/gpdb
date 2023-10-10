---
title: Working with External Data 
---

Both external and foreign tables provide access to data stored in data sources outside of Greenplum Database as if the data were stored in regular database tables. You can read data from and write data to external and foreign tables.

> **Important** Greenplum 7 internally converts external tables to foreign tables, and internally operates on and represents the table using the foreign table data structures and catalog. Refer to [Understanding the External Table to Foreign Table Mapping](map_ext_to_foreign.html) for detailed information about this conversion, and its runtime implications.

**Parent topic:** [Greenplum Database Administrator Guide](../admin_guide.html)

## <a id="foreign"></a>About Foreign Tables

A foreign table is a Greenplum Database table backed with data that resides outside of the database. Foreign tables are often used to load and unload database data. You can both read from and write to the same foreign table. You can use foreign tables in SQL commands just as you would a regular database table. For example, you can `SELECT`, `INSERT`, and `JOIN` foreign tables with other Greenplum tables.

Refer to [Accessing External Data with Foreign Tables](g-foreign.html) for more information about accessing external data using foreign tables.

## <a id="external"></a>About External Tables

An external table is different kind of Greenplum Database table backed with data that resides outside of the database. External tables are often used to load and unload database data. You create a readable external table to read data from the external data source and create a writable external table to write data to the external source. You can use external tables in SQL commands just as you would a regular database table. For example, you can `SELECT` \(readable external table\), `INSERT` \(writable external table\), and `JOIN` external tables with other Greenplum tables.

Refer to [Acessing External Data with External Tables](g-external-tables.html) for more information about using external tables to access external data.

### <a id="gpfdist"></a>About gpfdist External Tables

You can use the `gpfdist` file server utility and external tables to serve up external data to Greenplum. When external data is served by `gpfdist`, all segments in the Greenplum Database system can read or write external table data in parallel. Refer to [Using the Greenplum Parallel File Server \(gpfdist\)](../external/g-using-the-greenplum-parallel-file-server--gpfdist-.html) for more information.

### <a id="web_external"></a>About Web-Based External Tables

Web-based external tables provide access to data served by an HTTP server or an operating system process. See [Creating and Using External Web Tables](g-creating-and-using-web-external-tables.html) for more about web-based tables.

## <a id="pxf"></a>About Accessing External Data with pxf

Data managed by your organization may already reside in external sources such as Hadoop, object stores, and other SQL databases. The Greenplum Platform Extension Framework \(PXF\) provides access to this external data via built-in connectors that map an external data source to a Greenplum Database table definition. Refer to [Accessing External Data with PXF](../external/pxf-overview.html) for more information about using PXF.

