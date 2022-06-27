# Grant Privileges for the gphdfs Protocol \(Deprecated\) 

To enable privileges required to create external tables that access files on HDFS using `gphdfs`:

1.  Grant the following privileges on `gphdfs` to the owner of the external table.
    -   Grant `SELECT` privileges to enable creating readable external tables on HDFS.
    -   Grant `INSERT` privileges to enable creating writable external tables on HDFS.

        Use the `GRANT` command to grant read privileges \(`SELECT`\) and, if needed, write privileges \(`INSERT`\) on HDFS to the Greenplum system user \(`gpadmin`\).

        ```
        GRANT INSERT ON PROTOCOL gphdfs TO gpadmin; 
        ```

2.  Greenplum Database uses OS credentials to connect to HDFS. Grant read privileges and, if needed, write privileges to HDFS to the Greenplum administrative user \(`gpadmin` OS user\).

**Parent topic:** [Accessing HDFS Data with gphdfs \(Deprecated\)](../external/g-using-hadoop-distributed-file-system--hdfs--tables.html)

