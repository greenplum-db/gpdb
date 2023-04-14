---
title: Defining a File-Based Writable External Table 
---

Writable external tables that output data to files can use the Greenplum parallel file server program, gpfdist, or the Greenplum Platform Extension Framework \(PXF\), Greenplum's interface to Hadoop.

Use the `CREATE WRITABLE EXTERNAL TABLE` command to define the external table and specify the location and format of the output files. See [Using the Greenplum Parallel File Server \(gpfdist\)](../../external/g-using-the-greenplum-parallel-file-server--gpfdist-.html) for instructions on setting up gpfdist for use with an external table and [Accessing External Data with PXF](../../external/pxf-overview.html) for instructions on setting up PXF for use with an external table

-   With a writable external table using the gpfdist protocol, the Greenplum segments send their data to gpfdist, which writes the data to the named file. gpfdist must run on a host that the Greenplum segments can access over the network. gpfdist points to a file location on the output host and writes data received from the Greenplum segments to the file. To divide the output data among multiple files, list multiple gpfdist URIs in your writable external table definition.
-   A writable external web table sends data to an application as a stream of data. For example, unload data from Greenplum Database and send it to an application that connects to another database or ETL tool to load the data elsewhere. Writable external web tables use the `EXECUTE` clause to specify a shell command, script, or application to run on the segment hosts and accept an input stream of data. See [Defining a Command-Based Writable External Web Table](g-defining-a-command-based-writable-external-web-table.html) for more information about using `EXECUTE` commands in a writable external table definition.

You can optionally declare a distribution policy for your writable external tables. By default, writable external tables use a random distribution policy. If the source table you are exporting data from has a hash distribution policy, defining the same distribution key column\(s\) for the writable external table improves unload performance by eliminating the requirement to move rows over the interconnect. If you unload data from a particular table, you can use the `LIKE` clause to copy the column definitions and distribution policy from the source table.

-   **[Example 1—Greenplum file server \(gpfdist\)](../../load/topics/g-example-1-greenplum-file-server-gpfdist.html)**  

-   **[Example 2—Hadoop file server \(pxf\)](../../load/topics/g-example-2-hadoop-file-server.html)**  


**Parent topic:** [Unloading Data from Greenplum Database](../../load/topics/g-unloading-data-from-greenplum-database.html)

