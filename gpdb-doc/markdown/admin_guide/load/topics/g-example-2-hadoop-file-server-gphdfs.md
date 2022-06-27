# Example 2—Hadoop file server \(gphdfs \(Deprecated\)\) 

```
=# CREATE WRITABLE EXTERNAL TABLE unload_expenses 
   ( LIKE expenses ) 
   LOCATION ('gphdfs://hdfslhost-1:8081/path') 
 FORMAT 'TEXT' (DELIMITER ',')
 DISTRIBUTED BY (exp_id);

```

You can only specify a directory for a writable external table with the `gphdfs` protocol. \(You can only specify one file for a readable external table with the `gphdfs` protocol\)

**Note:** The default port number is 9000.

**Parent topic:** [Defining a File-Based Writable External Table](../../load/topics/g-defining-a-file-based-writable-external-table.html)

