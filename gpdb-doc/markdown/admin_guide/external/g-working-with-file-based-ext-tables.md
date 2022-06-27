# Working with External Data 

External tables provide access to data stored in data sources outside of Greenplum Database as if the data were stored in regular database tables. Data can be read from or written to external tables.

An external table is a Greenplum database table backed with data that resides outside of the database. An external table is either readable or writable. It can be used like a regular database table in SQL commands such as `SELECT` and `INSERT` and joined with other tables. External tables are most often used to load and unload database data.

Web-based external tables provide access to data served by an HTTP server or an operating system process. See [Creating and Using External Web Tables](g-creating-and-using-web-external-tables.html) for more about web-based tables.

-   **[Defining External Tables](../external/g-external-tables.html)**  
External tables enable accessing external data as if it were a regular database table. They are often used to move data into and out of a Greenplum database.
-   **[Accessing External Data with PXF](../external/pxf-overview.html)**  
Data managed by your organization may already reside in external sources such as Hadoop, object stores, and other SQL databases. The Greenplum Platform Extension Framework \(PXF\) provides access to this external data via built-in connectors that map an external data source to a Greenplum Database table definition.
-   **[Accessing HDFS Data with gphdfs \(Deprecated\)](../external/g-using-hadoop-distributed-file-system--hdfs--tables.html)**  
Greenplum Database leverages the parallel architecture of a Hadoop Distributed File System to read and write data files efficiently using the `gphdfs` protocol.
-   **[Using the Greenplum Parallel File Server \(gpfdist\)](../external/g-using-the-greenplum-parallel-file-server--gpfdist-.html)**  
The gpfdist protocol is used in a `CREATE EXTERNAL TABLE` SQL command to access external data served by the Greenplum Database `gpfdist` file server utility. When external data is served by gpfdist, all segments in the Greenplum Database system can read or write external table data in parallel.

**Parent topic:** [Greenplum Database Administrator Guide](../admin_guide.html)

