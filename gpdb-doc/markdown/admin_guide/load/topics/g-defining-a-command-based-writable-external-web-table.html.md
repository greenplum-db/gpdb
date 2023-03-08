---
title: Defining a Command-Based Writable External Web Table 
---

You can define writable external web tables to send output rows to an application or script. The application must accept an input stream, reside in the same location on all of the Greenplum segment hosts, and be executable by the `gpadmin` user. All segments in the Greenplum system run the application or script, whether or not a segment has output rows to process.

Use `CREATE WRITABLE EXTERNAL WEB TABLE` to define the external table and specify the application or script to run on the segment hosts. Commands run from within the database and cannot access environment variables \(such as `$PATH`\). Set environment variables in the `EXECUTE` clause of your writable external table definition. For example:

```
=# CREATE WRITABLE EXTERNAL WEB TABLE output (output text) 
    EXECUTE 'export PATH=$PATH:/home/`gpadmin`
            /programs;
    myprogram.sh' 
    FORMAT 'TEXT'
    DISTRIBUTED RANDOMLY;

```

The following Greenplum Database variables are available for use in OS commands run by a web or writable external table. Set these variables as environment variables in the shell that runs the command\(s\). They can be used to identify a set of requests made by an external table statement across the Greenplum Database array of hosts and segment instances.

|Variable|Description|
|--------|-----------|
|$GP\_CID|Command count of the transaction running the external table statement.|
|$GP\_DATABASE|The database in which the external table definition resides.|
|$GP\_DATE|The date on which the external table command ran.|
|$GP\_MASTER\_HOST|The host name of the Greenplum coordinator host from which the external table statement was dispatched.|
|$GP\_MASTER\_PORT|The port number of the Greenplum coordinator instance from which the external table statement was dispatched.|
|$GP\_QUERY\_STRING|The SQL command \(DML or SQL query\) run by Greenplum Database.|
|$GP\_SEG\_DATADIR|The location of the data directory of the segment instance running the external table command.|
|$GP\_SEG\_PG\_CONF|The location of the `postgresql.conf` file of the segment instance running the external table command.|
|$GP\_SEG\_PORT|The port number of the segment instance running the external table command.|
|$GP\_SEGMENT\_COUNT|The total number of primary segment instances in the Greenplum Database system.|
|$GP\_SEGMENT\_ID|The ID number of the segment instance running the external table command \(same as `content` in `gp_segment_configuration`\).|
|$GP\_SESSION\_ID|The database session identifier number associated with the external table statement.|
|$GP\_SN|Serial number of the external table scan node in the query plan of the external table statement.|
|$GP\_TIME|The time the external table command was run.|
|$GP\_USER|The database user running the external table statement.|
|$GP\_XID|The transaction ID of the external table statement.|

-   **[Deactivating EXECUTE for Web or Writable External Tables](../../load/topics/g-disabling-execute-for-web-or-writable-external-tables.html)**  


**Parent topic:** [Unloading Data from Greenplum Database](../../load/topics/g-unloading-data-from-greenplum-database.html)

