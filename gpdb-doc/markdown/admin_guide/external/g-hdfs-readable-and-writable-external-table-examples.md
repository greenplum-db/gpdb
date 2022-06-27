# HDFS Readable and Writable External Table Examples \(Deprecated\) 

**Note:** The `gphdfs` external table protocol is deprecated and will be removed in the next major release of Greenplum Database.

The following code defines a readable external table for an HDFS file named `filename.txt` on port 8081.

```
=# CREATE EXTERNAL TABLE ext_expenses ( 
        name text,
        date date,
        amount float4,
        category text,
        desc1 text )
   LOCATION ('gphdfs://hdfshost-1:8081/data/filename.txt') 
   FORMAT 'TEXT' (DELIMITER ',');

```

The following code defines a set of readable external tables that have a custom format located in the same HDFS directory on port 8081.

```
=# CREATE EXTERNAL TABLE ext_expenses (
        name text,
        date date,
        amount float4,
        category text,
        desc1 text )
   LOCATION ('gphdfs://hdfshost-1:8081/data/custdat*.dat') 
   FORMAT 'custom' (formatter='gphdfs_import');

```

The following code defines an HDFS directory for a writable external table on port 8081 with all compression options specified.

```
=# CREATE WRITABLE EXTERNAL TABLE ext_expenses (
        name text,
        date date,
        amount float4,
        category text,
        desc1 text )
   LOCATION ('gphdfs://hdfshost-1:8081/data/?compress=true&compression_type=RECORD
   &codec=org.apache.hadoop.io.compress.DefaultCodec') 
   FORMAT 'custom' (formatter='gphdfs_export');

```

Because the previous code uses the default compression options for `compression_type` and `codec`, the following command is equivalent.

```
=# CREATE WRITABLE EXTERNAL TABLE ext_expenses (
        name text,
        date date,
        amount float4,
        category text,
        desc1 text )
   LOCATION    ('gphdfs://hdfshost-1:8081/data?compress=true')
   FORMAT 'custom' (formatter='gphdfs_export');

```

**Parent topic:** [Accessing HDFS Data with gphdfs \(Deprecated\)](../external/g-using-hadoop-distributed-file-system--hdfs--tables.html)

