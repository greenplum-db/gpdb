# Example 5—TEXT Format on a Hadoop Distributed File Server 

**Note:** The `gphdfs` external table protocol is deprecated and will be removed in the next major release of Greenplum Database.

Creates a readable external table, *ext\_expenses,* using the `gphdfs` protocol. The column delimiter is a pipe \( \| \).

```
=# CREATE EXTERNAL TABLE ext_expenses ( name text, 
   date date,  amount float4, category text, desc1 text ) 
   LOCATION ('gphdfs://hdfshost-1:8081/data/filename.txt') 
   FORMAT 'TEXT' (DELIMITER '|');

```

gphdfs requires only one data path.

For examples of reading and writing custom formatted data on a Hadoop Distributed File System, see [Reading and Writing Custom-Formatted HDFS Data with gphdfs \(Deprecated\)](g-reading-and-writing-custom-formatted-hdfs-data.html).

**Parent topic:** [Examples for Creating External Tables](../external/g-creating-external-tables---examples.html)

