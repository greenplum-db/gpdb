# Accessing HDFS Data with gphdfs \(Deprecated\) 

Greenplum Database leverages the parallel architecture of a Hadoop Distributed File System to read and write data files efficiently using the `gphdfs` protocol.

**Note:** The `gphdfs` external table protocol is deprecated and will be removed in the next major release of Greenplum Database. Consider using the Greenplum Platform Extension Framework \(PXF\) `pxf` external table protocol to access data stored in a Hadoop file system.

There are three steps to using the gphdfs protocol with HDFS:

-   [One-time gphdfs Protocol Installation \(Deprecated\)](g-one-time-hdfs-protocol-installation.html)
-   [Grant Privileges for the gphdfs Protocol \(Deprecated\)](g-grant-privileges-for-the-hdfs-protocol.html)
-   [Specify gphdfs Protocol in an External Table Definition \(Deprecated\)](g-specify-hdfs-data-in-an-external-table-definition.html)

For information about using Greenplum Database external tables with Amazon EMR when Greenplum Database is installed on Amazon Web Services \(AWS\), also see [Using Amazon EMR with Greenplum Database installed on AWS \(Deprecated\)](g-hdfs-emr-config.html).

-   **[One-time gphdfs Protocol Installation \(Deprecated\)](../external/g-one-time-hdfs-protocol-installation.html)**  

-   **[Grant Privileges for the gphdfs Protocol \(Deprecated\)](../external/g-grant-privileges-for-the-hdfs-protocol.html)**  

-   **[Specify gphdfs Protocol in an External Table Definition \(Deprecated\)](../external/g-specify-hdfs-data-in-an-external-table-definition.html)**  

-   **[gphdfs Support for Avro Files \(Deprecated\)](../external/g-hdfs-avro-format.html)**  

-   **[gphdfs Support for Parquet Files \(Deprecated\)](../external/g-hdfs-parquet-format.html)**  

-   **[HDFS Readable and Writable External Table Examples \(Deprecated\)](../external/g-hdfs-readable-and-writable-external-table-examples.html)**  

-   **[Reading and Writing Custom-Formatted HDFS Data with gphdfs \(Deprecated\)](../external/g-reading-and-writing-custom-formatted-hdfs-data.html)**  

-   **[Using Amazon EMR with Greenplum Database installed on AWS \(Deprecated\)](../external/g-hdfs-emr-config.html)**  


**Parent topic:** [Working with External Data](../external/g-working-with-file-based-ext-tables.html)

