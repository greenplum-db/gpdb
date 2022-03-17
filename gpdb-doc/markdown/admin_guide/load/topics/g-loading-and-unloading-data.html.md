---
title: Loading and Unloading Data 
---

The topics in this section describe methods for loading and writing data into and out of a Greenplum Database, and how to format data files.

Greenplum Database supports high-performance parallel data loading and unloading, and for smaller amounts of data, single file, non-parallel data import and export.

Greenplum Database can read from and write to several types of external data sources, including text files, Hadoop file systems, Amazon S3, and web servers.

-   The `COPY` SQL command transfers data between an external text file on the master host, or multiple text files on segment hosts, and a Greenplum Database table.
-   Readable external tables allow you to query data outside of the database directly and in parallel using SQL commands such as `SELECT`, `JOIN`, or `SORT EXTERNAL TABLE DATA`, and you can create views for external tables. External tables are often used to load external data into a regular database table using a command such as `CREATE TABLE table AS SELECT * FROM ext\_table`.
-   External web tables provide access to dynamic data. They can be backed with data from URLs accessed using the HTTP protocol or by the output of an OS script running on one or more segments.
-   The `gpfdist` utility is the Greenplum Database parallel file distribution program. It is an HTTP server that is used with external tables to allow Greenplum Database segments to load external data in parallel, from multiple file systems. You can run multiple instances of `gpfdist` on different hosts and network interfaces and access them in parallel.
-   The `gpload` utility automates the steps of a load task using `gpfdist` and a YAML-formatted control file.
-   You can create readable and writable external tables with the Greenplum Platform Extension Framework \(PXF\), and use these tables to load data into, or offload data from, Greenplum Database. For information about using PXF, refer to [Accessing External Data with PXF](../../external/pxf-overview.html).
-   The Greenplum Streaming Server is an ETL tool and API that you can use to load data into Greenplum Database. For information about using this tool, refer to the [Greenplum Streaming Server](https://greenplum.docs.pivotal.io/streaming-server/1-5/intro.html) documentation.
-   The Greenplum Streaming Server Kafka integration provides high-speed, parallel data transfer from Kafka to Greenplum Database. For more information, refer to the [Greenplum Streaming Server](https://greenplum.docs.pivotal.io/streaming-server/1-5/kafka/intro.html) documentation.
-   The Greenplum Connector for Apache Spark provides high speed, parallel data transfer between Tanzu Greenplum and Apache Spark. For information about using the Connector, refer to the documentation at [https://greenplum-spark.docs.pivotal.io/](https://greenplum-spark.docs.pivotal.io/).
-   The Greenplum Connector for Informatica provides high speed data transfer from an Informatica PowerCenter cluster to a Tanzu Greenplum cluster for batch and streaming ETL operations. For information about using the Connector, refer to the documentation at [https://greenplum-informatica.docs.pivotal.io/](https://greenplum-informatica.docs.pivotal.io/).
-   The Greenplum Connector for Apache NiFi enables you to create data ingestion pipelines that load record-oriented data into Tanzu Greenplum. For information about using this Connector, refer to the [Connector documentation](https://greenplum.docs.pivotal.io/connectors/apache-nifi/1-0/index.html).

The method you choose to load data depends on the characteristics of the source data—its location, size, format, and any transformations required.

In the simplest case, the `COPY` SQL command loads data into a table from a text file that is accessible to the Greenplum Database master instance. This requires no setup and provides good performance for smaller amounts of data. With the `COPY` command, the data copied into or out of the database passes between a single file on the master host and the database. This limits the total size of the dataset to the capacity of the file system where the external file resides and limits the data transfer to a single file write stream.

More efficient data loading options for large datasets take advantage of the Greenplum Database MPP architecture, using the Greenplum Database segments to load data in parallel. These methods allow data to load simultaneously from multiple file systems, through multiple NICs, on multiple hosts, achieving very high data transfer rates. External tables allow you to access external files from within the database as if they are regular database tables. When used with `gpfdist`, the Greenplum Database parallel file distribution program, external tables provide full parallelism by using the resources of all Greenplum Database segments to load or unload data.

Greenplum Database leverages the parallel architecture of the Hadoop Distributed File System to access files on that system.

