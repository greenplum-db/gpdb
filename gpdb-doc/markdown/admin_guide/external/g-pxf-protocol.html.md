---
title: pxf:// Protocol 
---

You can use the Greenplum Platform Extension Framework \(PXF\) `pxf://` protocol to access data residing in object store systems \(Azure, Google Cloud Storage, Minio, S3\), external Hadoop systems \(HDFS, Hive, HBase\), and SQL databases.

The PXF `pxf` protocol is packaged as a Greenplum Database extension. The `pxf` protocol supports reading from external data stores. You can also write text, binary, and parquet-format data with the `pxf` protocol.

When you use the `pxf` protocol to query an external data store, you specify the directory, file, or table that you want to access. PXF requests the data from the data store and delivers the relevant portions in parallel to each Greenplum Database segment instance serving the query.

You must explicitly initialize and start PXF before you can use the `pxf` protocol to read or write external data. You must also enable PXF in each database in which you want to allow users to create external tables to access external data, and grant permissions on the `pxf` protocol to those Greenplum Database users.

For detailed information about configuring and using PXF and the `pxf` protocol, refer to [Accessing External Data with PXF](pxf-overview.html).

**Parent topic:** [Defining External Tables](../external/g-external-tables.html)

