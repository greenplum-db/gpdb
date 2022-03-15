---
title: Working with External Data 
---

Both external and foreign tables provide access to data stored in data sources outside of Greenplum Database as if the data were stored in regular database tables. You can read data from and write data to external and foreign tables.

An external table is a Greenplum Database table backed with data that resides outside of the database. You create a readable external table to read data from the external data source and create a writable external table to write data to the external source. You can use external tables in SQL commands just as you would a regular database table. For example, you can `SELECT` \(readable external table\), `INSERT` \(writable external table\), and join external tables with other Greenplum tables. External tables are most often used to load and unload database data. Refer to [Defining External Tables](g-external-tables.html) for more information about using external tables to access external data.

[Accessing External Data with PXF](pxf-overview.html) describes using PXF and external tables to access external data sources.

A foreign table is a different kind of Greenplum Database table backed with data that resides outside of the database. You can both read from and write to the same foreign table. You can similarly use foreign tables in SQL commands as described above for external tables. Refer to [Accessing External Data with Foreign Tables](g-foreign.html) for more information about accessing external data using foreign tables.

Web-based external tables provide access to data served by an HTTP server or an operating system process. See [Creating and Using External Web Tables](g-creating-and-using-web-external-tables.html) for more about web-based tables.

-   **[Accessing External Data with PXF](../external/pxf-overview.html)**  
Data managed by your organization may already reside in external sources such as Hadoop, object stores, and other SQL databases. The Greenplum Platform Extension Framework \(PXF\) provides access to this external data via built-in connectors that map an external data source to a Greenplum Database table definition.
-   **[Defining External Tables](../external/g-external-tables.html)**  
External tables enable accessing external data as if it were a regular database table. They are often used to move data into and out of a Greenplum database.
-   **[Accessing External Data with Foreign Tables](../external/g-foreign.html)**  

-   **[Using the Greenplum Parallel File Server \(gpfdist\)](../external/g-using-the-greenplum-parallel-file-server--gpfdist-.html)**  
The gpfdist protocol is used in a `CREATE EXTERNAL TABLE` SQL command to access external data served by the Greenplum Database `gpfdist` file server utility. When external data is served by gpfdist, all segments in the Greenplum Database system can read or write external table data in parallel.

**Parent topic:**[Greenplum Database Administrator Guide](../admin_guide.html)

