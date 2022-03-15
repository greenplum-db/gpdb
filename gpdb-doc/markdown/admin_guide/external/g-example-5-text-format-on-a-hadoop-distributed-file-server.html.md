---
title: Example 5—TEXT Format on a Hadoop Distributed File Server 
---

Creates a readable external table, *ext\_expenses,* using the `pxf` protocol. The column delimiter is a pipe \( \| \).

```
=# CREATE EXTERNAL TABLE ext_expenses ( name text, 
   date date,  amount float4, category text, desc1 text ) 
   LOCATION ('pxf://dir/data/filename.txt?PROFILE=hdfs:text') 
   FORMAT 'TEXT' (DELIMITER '|');

```

Refer to [Accessing External Data with PXF](pxf-overview.html) for information about using the Greenplum Platform Extension Framework \(PXF\) to access data on a Hadoop Distributed File System.

**Parent topic:**[Examples for Creating External Tables](../external/g-creating-external-tables---examples.html)

