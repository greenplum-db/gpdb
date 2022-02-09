---
title: Example 2—Hadoop file server (pxf) 
---

```
=# CREATE WRITABLE EXTERNAL TABLE unload_expenses 
   ( LIKE expenses ) 
   LOCATION ('pxf://dir/path?PROFILE=hdfs:text') 
 FORMAT 'TEXT' (DELIMITER ',')
 DISTRIBUTED BY (exp_id);

```

You specify an HDFS directory for a writable external table that you create with the `pxf` protocol.

**Parent topic:**[Defining a File-Based Writable External Table](../../load/topics/g-defining-a-file-based-writable-external-table.html)

