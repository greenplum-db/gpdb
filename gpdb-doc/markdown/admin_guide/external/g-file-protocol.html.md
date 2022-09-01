---
title: file:// Protocol 
---

You can use the `file://` protocol with a Greenplum Database external table to read from one or more files located on each Greenplum Database segment host. The `file://` protocol does not support writing to files.

When reading, the `file://` protocol uncompresses `gzip` \(`.gz`\), `bzip2` \(.`bz2`\), and `zstd` \(`.zst`\) files automatically.

You must provide a URI that specifies the location of an operating system file(s). The URI includes the host name, port, and path to the file. Each file must reside on a segment host in a location accessible by the Greenplum Database superuser \(`gpadmin`\). The host name used in the URI must match a segment host name registered in the `gp_segment_configuration` system catalog table.

The `LOCATION` clause can have multiple URIs, as shown in this example:

```
CREATE EXTERNAL TABLE ext_expenses (
   name text, date date, amount float4, category text, desc1 text ) 
LOCATION ('file://host1:5432/data/expense/*.csv', 
          'file://host2:5432/data/expense/*.csv', 
          'file://host3:5432/data/expense/*.csv') 
FORMAT 'CSV' (HEADER); 
```

The number of URIs you specify in the `LOCATION` clause is the number of segment instances that will work in parallel to access the external table. For each URI, Greenplum assigns a primary segment on the specified host to the file. For maximum parallelism when loading data, divide the data into as many equally sized files as you have primary segments. This ensures that all segments participate in the load. The number of external files per segment host cannot exceed the number of primary segment instances on that host. For example, if your array has four primary segment instances per segment host, you can place four external files on each segment host. Tables based on the `file://` protocol can only be readable tables.

The system view `pg_max_external_files` shows how many external table files are permitted per external table. This view lists the available file slots per segment host when using the `file://` protocol. The view is only applicable for the `file:// protocol`. For example:

```
SELECT * FROM pg_max_external_files;
```

**Parent topic:** [Defining External Tables](../external/g-external-tables.html)

