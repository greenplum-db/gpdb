---
title: Monitoring a Greenplum System 
---

You can monitor a Greenplum Database system using a variety of tools included with the system or available as add-ons.

Observing the Greenplum Database system day-to-day performance helps administrators understand the system behavior, plan workflow, and troubleshoot problems. This chapter discusses tools for monitoring database performance and activity.

Also, be sure to review [Recommended Monitoring and Maintenance Tasks](../monitoring/monitoring.html) for monitoring activities you can script to quickly detect problems in the system.

**Parent topic:** [Managing a Greenplum System](../managing/partII.html)

## <a id="topic2"></a>Monitoring Database Activity and Performance 

Greenplum Database includes an optional system monitoring and management database, `gpperfmon`, that administrators can enable. The `gpperfmon_install` command-line utility creates the `gpperfmon` database and enables data collection agents that collect and store query and system metrics in the database. Administrators can query metrics in the `gpperfmon` database. See the documentation for the `gpperfmon` database in the _Greenplum Database Reference Guide_.

VMware Tanzu Greenplum Command Center, an optional web-based interface, provides cluster status information, graphical administrative tools, real-time query monitoring, and historical cluster and query data. Download the Greenplum Command Center package from [VMware Tanzu Network](https://network.pivotal.io/products/pivotal-gpdb) and view the documentation at the [Greenplum Command Center Documentation](http://docs.vmware.com/en/VMware-Tanzu-Greenplum-Command-Center/index.html) web site.

## <a id="topic3"></a>Monitoring System State 

As a Greenplum Database administrator, you must monitor the system for problem events such as a segment going down or running out of disk space on a segment host. The following topics describe how to monitor the health of a Greenplum Database system and examine certain state information for a Greenplum Database system.

-   [Checking System State](#topic12)
-   [Checking Disk Space Usage](#topic15)
-   [Checking for Data Distribution Skew](#topic20)
-   [Checking for and Terminating Overflowed Backends](#overflowed_backends)
-   [Viewing Metadata Information about Database Objects](#topic24)
-   [Viewing Session Memory Usage Information](#topic_slt_ddv_1q)
-   [Viewing Query Workfile Usage Information](#topic27)

## <a id="topic12"></a>Checking System State 

A Greenplum Database system is comprised of multiple PostgreSQL instances \(the master and segments\) spanning multiple machines. To monitor a Greenplum Database system, you need to know information about the system as a whole, as well as status information of the individual instances. The `gpstate` utility provides status information about a Greenplum Database system.

### <a id="topic13"></a>Viewing Master and Segment Status and Configuration 

The default `gpstate` action is to check segment instances and show a brief status of the valid and failed segments. For example, to see a quick status of your Greenplum Database system:

```
$ gpstate

```

To see more detailed information about your Greenplum Database array configuration, use `gpstate` with the `-s` option:

```
$ gpstate -s

```

### <a id="topic14"></a>Viewing Your Mirroring Configuration and Status 

If you are using mirroring for data redundancy, you may want to see the list of mirror segment instances in the system, their current synchronization status, and the mirror to primary mapping. For example, to see the mirror segments in the system and their status:

```
$ gpstate -m 

```

To see the primary to mirror segment mappings:

```
$ gpstate -c

```

To see the status of the standby master mirror:

```
$ gpstate -f

```

## <a id="topic15"></a>Checking Disk Space Usage 

A database administrator's most important monitoring task is to make sure the file systems where the master and segment data directories reside do not grow to more than 70 percent full. A filled data disk will not result in data corruption, but it may prevent normal database activity from continuing. If the disk grows too full, it can cause the database server to shut down.

You can use the `gp_disk_free` external table in the `gp_toolkit` administrative schema to check for remaining free space \(in kilobytes\) on the segment host file systems. For example:

```
=# SELECT * FROM gp_toolkit.gp_disk_free 
   ORDER BY dfsegment;

```

### <a id="topic16"></a>Checking Sizing of Distributed Databases and Tables 

The `gp_toolkit` administrative schema contains several views that you can use to determine the disk space usage for a distributed Greenplum Database database, schema, table, or index.

For a list of the available sizing views for checking database object sizes and disk space, see the *Greenplum Database Reference Guide*.

#### <a id="topic17"></a>Viewing Disk Space Usage for a Database 

To see the total size of a database \(in bytes\), use the *gp\_size\_of\_database* view in the *gp\_toolkit* administrative schema. For example:

```
=> SELECT * FROM gp_toolkit.gp_size_of_database 
   ORDER BY sodddatname;

```

#### <a id="topic18"></a>Viewing Disk Space Usage for a Table 

The *gp\_toolkit* administrative schema contains several views for checking the size of a table. The table sizing views list the table by object ID \(not by name\). To check the size of a table by name, you must look up the relation name \(`relname`\) in the *pg\_class* table. For example:

```
=> SELECT relname AS name, sotdsize AS size, sotdtoastsize 
   AS toast, sotdadditionalsize AS other 
   FROM gp_toolkit.gp_size_of_table_disk as sotd, pg_class 
   WHERE sotd.sotdoid=pg_class.oid ORDER BY relname;

```

For a list of the available table sizing views, see the *Greenplum Database Reference Guide*.

#### <a id="topic19"></a>Viewing Disk Space Usage for Indexes 

The *gp\_toolkit* administrative schema contains a number of views for checking index sizes. To see the total size of all index\(es\) on a table, use the *gp\_size\_of\_all\_table\_indexes* view. To see the size of a particular index, use the *gp\_size\_of\_index* view. The index sizing views list tables and indexes by object ID \(not by name\). To check the size of an index by name, you must look up the relation name \(`relname`\) in the *pg\_class* table. For example:

```
=> SELECT soisize, relname as indexname
   FROM pg_class, gp_toolkit.gp_size_of_index
   WHERE pg_class.oid=gp_size_of_index.soioid 
   AND pg_class.relkind='i';

```

## <a id="topic20"></a>Checking for Data Distribution Skew 

All tables in Greenplum Database are distributed, meaning their data is divided across all of the segments in the system. Unevenly distributed data may diminish query processing performance. A table's distribution policy, set at table creation time, determines how the table's rows are distributed. For information about choosing the table distribution policy, see the following topics:

-   [Viewing a Table's Distribution Key](#topic21)
-   [Viewing Data Distribution](#topic22)
-   [Checking for Query Processing Skew](#topic23)

The *gp\_toolkit* administrative schema also contains a number of views for checking data distribution skew on a table. For information about how to check for uneven data distribution, see the *Greenplum Database Reference Guide*.

### <a id="topic21"></a>Viewing a Table's Distribution Key 

To see the columns used as the data distribution key for a table, you can use the `\d+` meta-command in `psql` to examine the definition of a table. For example:

```
=# `\d+ sales
`                Table "retail.sales"
 Column      |     Type     | Modifiers | Description
-------------+--------------+-----------+-------------
 sale_id     | integer      |           |
 amt         | float        |           |
 date        | date         |           |
Has OIDs: no
Distributed by: (sale_id)

```

When you create a *replicated* table, Greenplum Database stores all rows in the table on every segment. Replicated tables have no distribution key. Where the `\d+` meta-command reports the distribution key for a normally distributed table, it shows `Distributed Replicated` for a replicated table.

### <a id="topic22"></a>Viewing Data Distribution 

To see the data distribution of a table's rows \(the number of rows on each segment\), you can run a query such as:

```
=# SELECT gp_segment_id, count(*) 
   FROM <table_name> GROUP BY gp_segment_id;

```

A table is considered to have a balanced distribution if all segments have roughly the same number of rows.

**Note:** If you run this query on a replicated table, it fails because Greenplum Database does not permit user queries to reference the system column `gp_segment_id` \(or the system columns `ctid`, `cmin`, `cmax`, `xmin`, and `xmax`\) in replicated tables. Because every segment has all of the tables' rows, replicated tables are evenly distributed by definition.

### <a id="topic23"></a>Checking for Query Processing Skew 

When a query is being processed, all segments should have equal workloads to ensure the best possible performance. If you identify a poorly-performing query, you may need to investigate further using the `EXPLAIN` command. For information about using the `EXPLAIN` command and query profiling, see [Query Profiling](../query/topics/query-profiling.html).

Query processing workload can be skewed if the table's data distribution policy and the query predicates are not well matched. To check for processing skew, you can run a query such as:

```
=# SELECT gp_segment_id, count(*) FROM <table_name>
   WHERE <column>='<value>' GROUP BY gp_segment_id;

```

This will show the number of rows returned by segment for the given `WHERE` predicate.

As noted in [Viewing Data Distribution](#topic22), this query will fail if you run it on a replicated table because you cannot reference the `gp_segment_id` system column in a query on a replicated table.

#### <a id="avoidskew"></a>Avoiding an Extreme Skew Warning 

You may receive the following warning message while running a query that performs a hash join operation:

`Extreme skew in the innerside of Hashjoin`

This occurs when the input to a hash join operator is skewed. It does not prevent the query from completing successfully. You can follow these steps to avoid skew in the plan:

1.  Ensure that all fact tables are analyzed.
2.  Verify that any populated temporary table used by the query is analyzed.
3.  View the `EXPLAIN ANALYZE` plan for the query and look for the following:
    -   If there are scans with multi-column filters that are producing more rows than estimated, then set the `gp_selectivity_damping_factor` server configuration parameter to 2 or higher and retest the query.
    -   If the skew occurs while joining a single fact table that is relatively small \(less than 5000 rows\), set the `gp_segments_for_planner` server configuration parameter to 1 and retest the query.
4.  Check whether the filters applied in the query match distribution keys of the base tables. If the filters and distribution keys are the same, consider redistributing some of the base tables with different distribution keys.
5.  Check the cardinality of the join keys. If they have low cardinality, try to rewrite the query with different joining columns or additional filters on the tables to reduce the number of rows. These changes could change the query semantics.

## <a id="overflowed_backends"></a>Checking for and Terminating Overflowed Backends

Subtransaction overflow arises when a Greenplum Database backend creates more than 64 subtransactions, resulting in a high lookup cost for visibility checks. This slows query performance, but even more so when it occurs in combination with long-running transactions, which result in still more lookups. Terminating suboverflowed backends and/or backends with long-running transactions can help prevent and alleviate performance problems. 

Greenplum Database includes an extension -- `gp_subtransaction_overflow` -- and a view -- `gp_suboverflowed_backend` -- that is run over a user-defined function to help users query for suboverflowed backends. Users can use segment id and process id information reported in the view to terminate the offending backends, thereby preventing degradation of performance.

### <a id="check_backends"></a>Steps

Follow these steps below to identify and terminate overflowed backends.

1. Create the extension:

    ```
    CREATE EXTENSION gp_subtransaction_overflow;
    ```

2. Select all from the view the extension created:

    ```
    select * from gp_suboverflowed_backend`;
    ```

    This returns output similar to the following:
    
    ```
   segid |   pids    
   -------+-----------
    -1 | 
     0 | {1731513}
     1 | {1731514}
     2 | {1731515}
   (4 rows)
    ```

3. Connect to the database in utility mode and query `pg_stat_activity` to return the session id for the process id in the output for a segment. For example: 

    ```
    select sess_id from pg_stat_activity where pid=1731513;
    ```

    ```
    sess_id 
    ---------
      10
    (1 row)
    ```

4. Terminate the session, which will terminate all associated backends on all segments:

    ```
    select pg_terminate_backend(pid) from pg_stat_activity where sess_id=10;
    ``` 

5. Verify that there are no more suboverflowed backends:

    ```
    select * from gp_suboverflowed_backend`;
    ```

    
    ```
   segid |   pids    
   -------+-----------
    -1 | 
     0 |
     1 | 
     2 | 
   (4 rows)
    ```

## <a id="topic24"></a>Viewing Metadata Information about Database Objects 

Greenplum Database tracks various metadata information in its system catalogs about the objects stored in a database, such as tables, views, indexes and so on, as well as global objects such as roles and tablespaces.

### <a id="topic25"></a>Viewing the Last Operation Performed 

You can use the system views *pg\_stat\_operations* and *pg\_stat\_partition\_operations* to look up actions performed on an object, such as a table. For example, to see the actions performed on a table, such as when it was created and when it was last vacuumed and analyzed:

```
=> SELECT schemaname as schema, objname as table, 
   usename as role, actionname as action, 
   subtype as type, statime as time 
   FROM pg_stat_operations 
   WHERE objname='cust';
 schema | table | role | action  | type  | time
--------+-------+------+---------+-------+--------------------------
  sales | cust  | main | CREATE  | TABLE | 2016-02-09 18:10:07.867977-08
  sales | cust  | main | VACUUM  |       | 2016-02-10 13:32:39.068219-08
  sales | cust  | main | ANALYZE |       | 2016-02-25 16:07:01.157168-08
(3 rows)

```

### <a id="topic26"></a>Viewing the Definition of an Object 

To see the definition of an object, such as a table or view, you can use the `\d+` meta-command when working in `psql`. For example, to see the definition of a table:

```
=> \d+ <mytable>

```

## <a id="topic_slt_ddv_1q"></a>Viewing Session Memory Usage Information 

You can create and use the *session\_level\_memory\_consumption* view that provides information about the current memory utilization for sessions that are running queries on Greenplum Database. The view contains session information and information such as the database that the session is connected to, the query that the session is currently running, and memory consumed by the session processes.

-   [Creating the session\_level\_memory\_consumption View](#topic_nby_j1b_dq)
-   [The session\_level\_memory\_consumption View](#topic7)

### <a id="topic_nby_j1b_dq"></a>Creating the session\_level\_memory\_consumption View 

To create the *session\_state.session\_level\_memory\_consumption* view in a Greenplum Database, run the script `CREATE EXTENSION gp_internal_tools;` once for each database. For example, to install the view in the database `testdb`, use this command:

```
$ psql -d testdb -c "CREATE EXTENSION gp_internal_tools;"
```

### <a id="topic7"></a>The session\_level\_memory\_consumption View 

The *session\_state.session\_level\_memory\_consumption* view provides information about memory consumption and idle time for sessions that are running SQL queries.

When resource queue-based resource management is active, the column `is_runaway` indicates whether Greenplum Database considers the session a runaway session based on the vmem memory consumption of the session's queries. Under the resource queue-based resource management scheme, Greenplum Database considers the session a runaway when the queries consume an excessive amount of memory. The Greenplum Database server configuration parameter `runaway_detector_activation_percent` governs the conditions under which Greenplum Database considers a session a runaway session.

The `is_runaway`, `runaway_vmem_mb`, and `runaway_command_cnt` columns are not applicable when resource group-based resource management is active.

|column|type|references|description|
|------|----|----------|-----------|
|`datname`|name| |Name of the database that the session is connected to.|
|`sess_id`|integer| |Session ID.|
|`usename`|name| |Name of the session user.|
|`query`|text| |Current SQL query that the session is running.|
|`segid`|integer| |Segment ID.|
|`vmem_mb`|integer| |Total vmem memory usage for the session in MB.|
|`is_runaway`|boolean| |Session is marked as runaway on the segment.|
|`qe_count`|integer| |Number of query processes for the session.|
|`active_qe_count`|integer| |Number of active query processes for the session.|
|`dirty_qe_count`|integer| |Number of query processes that have not yet released their memory. The value is `-1` for sessions that are not running.|
|`runaway_vmem_mb`|integer| |Amount of vmem memory that the session was consuming when it was marked as a runaway session.|
|`runaway_command_cnt`|integer| |Command count for the session when it was marked as a runaway session.|
|`idle_start`|timestamptz| |The last time a query process in this session became idle.|

## <a id="topic27"></a>Viewing Query Workfile Usage Information 

The Greenplum Database administrative schema *gp\_toolkit* contains views that display information about Greenplum Database workfiles. Greenplum Database creates workfiles on disk if it does not have sufficient memory to run the query in memory. This information can be used for troubleshooting and tuning queries. The information in the views can also be used to specify the values for the Greenplum Database configuration parameters `gp_workfile_limit_per_query` and `gp_workfile_limit_per_segment`.

These are the views in the schema *gp\_toolkit*:

-   The gp\_workfile\_entries view contains one row for each operator using disk space for workfiles on a segment at the current time.
-   The gp\_workfile\_usage\_per\_query view contains one row for each query using disk space for workfiles on a segment at the current time.
-   The gp\_workfile\_usage\_per\_segment view contains one row for each segment. Each row displays the total amount of disk space used for workfiles on the segment at the current time.

For information about using *gp\_toolkit*, see [Using gp\_toolkit](#topic31).

## <a id="topic28"></a>Viewing the Database Server Log Files 

Every database instance in Greenplum Database \(master and segments\) runs a PostgreSQL database server with its own server log file. Log files are created in the `log` directory of the master and each segment data directory.

### <a id="topic29"></a>Log File Format 

The server log files are written in comma-separated values \(CSV\) format. Some log entries will not have values for all log fields. For example, only log entries associated with a query worker process will have the `slice_id` populated. You can identify related log entries of a particular query by the query's session identifier \(`gp_session_id`\) and command identifier \(`gp_command_count`\).

The following fields are written to the log:

|Number|Field Name|Data Type|Description|
|------|----------|---------|-----------|
|1|event\_time|timestamp with time zone|Time that the log entry was written to the log|
|2|user\_name|varchar\(100\)|The database user name|
|3|database\_name|varchar\(100\)|The database name|
|4|process\_id|varchar\(10\)|The system process ID \(prefixed with "p"\)|
|5|thread\_id|varchar\(50\)|The thread count \(prefixed with "th"\)|
|6|remote\_host|varchar\(100\)|On the master, the hostname/address of the client machine. On the segment, the hostname/address of the master.|
|7|remote\_port|varchar\(10\)|The segment or master port number|
|8|session\_start\_time|timestamp with time zone|Time session connection was opened|
|9|transaction\_id|int|Top-level transaction ID on the master. This ID is the parent of any subtransactions.|
|10|gp\_session\_id|text|Session identifier number \(prefixed with "con"\)|
|11|gp\_command\_count|text|The command number within a session \(prefixed with "cmd"\)|
|12|gp\_segment|text|The segment content identifier \(prefixed with "seg" for primaries or "mir" for mirrors\). The master always has a content ID of -1.|
|13|slice\_id|text|The slice ID \(portion of the query plan being executed\)|
|14|distr\_tranx\_id|text|Distributed transaction ID|
|15|local\_tranx\_id|text|Local transaction ID|
|16|sub\_tranx\_id|text|Subtransaction ID|
|17|event\_severity|varchar\(10\)|Values include: LOG, ERROR, FATAL, PANIC, DEBUG1, DEBUG2|
|18|sql\_state\_code|varchar\(10\)|SQL state code associated with the log message|
|19|event\_message|text|Log or error message text|
|20|event\_detail|text|Detail message text associated with an error or warning message|
|21|event\_hint|text|Hint message text associated with an error or warning message|
|22|internal\_query|text|The internally-generated query text|
|23|internal\_query\_pos|int|The cursor index into the internally-generated query text|
|24|event\_context|text|The context in which this message gets generated|
|25|debug\_query\_string|text|User-supplied query string with full detail for debugging. This string can be modified for internal use.|
|26|error\_cursor\_pos|int|The cursor index into the query string|
|27|func\_name|text|The function in which this message is generated|
|28|file\_name|text|The internal code file where the message originated|
|29|file\_line|int|The line of the code file where the message originated|
|30|stack\_trace|text|Stack trace text associated with this message|

### <a id="topic30"></a>Searching the Greenplum Server Log Files 

Greenplum Database provides a utility called `gplogfilter` can search through a Greenplum Database log file for entries matching the specified criteria. By default, this utility searches through the Greenplum Database master log file in the default logging location. For example, to display the last three lines of each of the log files under the master directory:

```
$ gplogfilter -n 3

```

To search through all segment log files simultaneously, run `gplogfilter` through the `gpssh` utility. For example, to display the last three lines of each segment log file:

```
$ gpssh -f seg_host_file

```

```
=> source /usr/local/greenplum-db/greenplum_path.sh
=> gplogfilter -n 3 /gpdata/gp*/log/gpdb*.log
```

## <a id="topic31"></a>Using gp\_toolkit 

Use the Greenplum Database administrative schema *gp\_toolkit* to query the system catalogs, log files, and operating environment for system status information. The `gp_toolkit` schema contains several views you can access using SQL commands. The *gp\_toolkit* schema is accessible to all database users. Some objects require superuser permissions. Use a command similar to the following to add the *gp\_toolkit* schema to your schema search path:

```
=> ALTER ROLE myrole SET search_path TO myschema,gp_toolkit;
```

For a description of the available administrative schema views and their usages, see the *Greenplum Database Reference Guide*.

## <a id="topic_pyh_sqg_kp"></a>SQL Standard Error Codes 

The following table lists all the defined error codes. Some are not used, but are defined by the SQL standard. The error classes are also shown. For each error class there is a standard error code having the last three characters 000. This code is used only for error conditions that fall within the class but do not have any more-specific code assigned.

The PL/pgSQL condition name for each error code is the same as the phrase shown in the table, with underscores substituted for spaces. For example, code 22012, DIVISION BY ZERO, has condition name DIVISION\_BY\_ZERO. Condition names can be written in either upper or lower case.

**Note:** PL/pgSQL does not recognize warning, as opposed to error, condition names; those are classes 00, 01, and 02.

|Error Code|Meaning|Constant|
|----------|-------|--------|
|**Class 00**— Successful Completion|
|00000|SUCCESSFUL COMPLETION|successful\_completion|
|Class 01 — Warning|
|01000|WARNING|warning|
|0100C|DYNAMIC RESULT SETS RETURNED|dynamic\_result\_sets\_returned|
|01008|IMPLICIT ZERO BIT PADDING|implicit\_zero\_bit\_padding|
|01003|NULL VALUE ELIMINATED IN SET FUNCTION|null\_value\_eliminated\_in\_set\_function|
|01007|PRIVILEGE NOT GRANTED|privilege\_not\_granted|
|01006|PRIVILEGE NOT REVOKED|privilege\_not\_revoked|
|01004|STRING DATA RIGHT TRUNCATION|string\_data\_right\_truncation|
|01P01|DEPRECATED FEATURE|deprecated\_feature|
|**Class 02** — No Data \(this is also a warning class per the SQL standard\)|
|02000|NO DATA|no\_data|
|02001|NO ADDITIONAL DYNAMIC RESULT SETS RETURNED|no\_additional\_dynamic\_result\_sets\_returned|
|**Class 03** — SQL Statement Not Yet Complete|
|03000|SQL STATEMENT NOT YET COMPLETE|sql\_statement\_not\_yet\_complete|
|**Class 08** — Connection Exception|
|08000|CONNECTION EXCEPTION|connection\_exception|
|08003|CONNECTION DOES NOT EXIST|connection\_does\_not\_exist|
|08006|CONNECTION FAILURE|connection\_failure|
|08001|SQLCLIENT UNABLE TO ESTABLISH SQLCONNECTION|sqlclient\_unable\_to\_establish\_sqlconnection|
|08004|SQLSERVER REJECTED ESTABLISHMENT OF SQLCONNECTION|sqlserver\_rejected\_establishment\_of\_sqlconnection|
|08007|TRANSACTION RESOLUTION UNKNOWN|transaction\_resolution\_unknown|
|08P01|PROTOCOL VIOLATION|protocol\_violation|
|**Class 09** — Triggered Action Exception|
|09000|TRIGGERED ACTION EXCEPTION|triggered\_action\_exception|
|**Class 0A** — Feature Not Supported|
|0A000|FEATURE NOT SUPPORTED|feature\_not\_supported|
|**Class 0B** — Invalid Transaction Initiation|
|0B000|INVALID TRANSACTION INITIATION|invalid\_transaction\_initiation|
|**Class 0F** — Locator Exception|
|0F000|LOCATOR EXCEPTION|locator\_exception|
|0F001|INVALID LOCATOR SPECIFICATION|invalid\_locator\_specification|
|**Class 0L** — Invalid Grantor|
|0L000|INVALID GRANTOR|invalid\_grantor|
|0LP01|INVALID GRANT OPERATION|invalid\_grant\_operation|
|**Class 0P** — Invalid Role Specification|
|0P000|INVALID ROLE SPECIFICATION|invalid\_role\_specification|
|**Class 21** — Cardinality Violation|
|21000|CARDINALITY VIOLATION|cardinality\_violation|
|**Class 22** — Data Exception|
|22000|DATA EXCEPTION|data\_exception|
|2202E|ARRAY SUBSCRIPT ERROR|array\_subscript\_error|
|22021|CHARACTER NOT IN REPERTOIRE|character\_not\_in\_repertoire|
|22008|DATETIME FIELD OVERFLOW|datetime\_field\_overflow|
|22012|DIVISION BY ZERO|division\_by\_zero|
|22005|ERROR IN ASSIGNMENT|error\_in\_assignment|
|2200B|ESCAPE CHARACTER CONFLICT|escape\_character\_conflict|
|22022|INDICATOR OVERFLOW|indicator\_overflow|
|22015|INTERVAL FIELD OVERFLOW|interval\_field\_overflow|
|2201E|INVALID ARGUMENT FOR LOGARITHM|invalid\_argument\_for\_logarithm|
|2201F|INVALID ARGUMENT FOR POWER FUNCTION|invalid\_argument\_for\_power\_function|
|2201G|INVALID ARGUMENT FOR WIDTH BUCKET FUNCTION|invalid\_argument\_for\_width\_bucket\_function|
|22018|INVALID CHARACTER VALUE FOR CAST|invalid\_character\_value\_for\_cast|
|22007|INVALID DATETIME FORMAT|invalid\_datetime\_format|
|22019|INVALID ESCAPE CHARACTER|invalid\_escape\_character|
|2200D|INVALID ESCAPE OCTET|invalid\_escape\_octet|
|22025|INVALID ESCAPE SEQUENCE|invalid\_escape\_sequence|
|22P06|NONSTANDARD USE OF ESCAPE CHARACTER|nonstandard\_use\_of\_escape\_character|
|22010|INVALID INDICATOR PARAMETER VALUE|invalid\_indicator\_parameter\_value|
|22020|INVALID LIMIT VALUE|invalid\_limit\_value|
|22023|INVALID PARAMETER VALUE|invalid\_parameter\_value|
|2201B|INVALID REGULAR EXPRESSION|invalid\_regular\_expression|
|22009|INVALID TIME ZONE DISPLACEMENT VALUE|invalid\_time\_zone\_displacement\_value|
|2200C|INVALID USE OF ESCAPE CHARACTER|invalid\_use\_of\_escape\_character|
|2200G|MOST SPECIFIC TYPE MISMATCH|most\_specific\_type\_mismatch|
|22004|NULL VALUE NOT ALLOWED|null\_value\_not\_allowed|
|22002|NULL VALUE NO INDICATOR PARAMETER|null\_value\_no\_indicator\_parameter|
|22003|NUMERIC VALUE OUT OF RANGE|numeric\_value\_out\_of\_range|
|22026|STRING DATA LENGTH MISMATCH|string\_data\_length\_mismatch|
|22001|STRING DATA RIGHT TRUNCATION|string\_data\_right\_truncation|
|22011|SUBSTRING ERROR|substring\_error|
|22027|TRIM ERROR|trim\_error|
|22024|UNTERMINATED C STRING|unterminated\_c\_string|
|2200F|ZERO LENGTH CHARACTER STRING|zero\_length\_character\_string|
|22P01|FLOATING POINT EXCEPTION|floating\_point\_exception|
|22P02|INVALID TEXT REPRESENTATION|invalid\_text\_representation|
|22P03|INVALID BINARY REPRESENTATION|invalid\_binary\_representation|
|22P04|BAD COPY FILE FORMAT|bad\_copy\_file\_format|
|22P05|UNTRANSLATABLE CHARACTER|untranslatable\_character|
|**Class 23** — Integrity Constraint Violation|
|23000|INTEGRITY CONSTRAINT VIOLATION|integrity\_constraint\_violation|
|23001|RESTRICT VIOLATION|restrict\_violation|
|23502|NOT NULL VIOLATION|not\_null\_violation|
|23503|FOREIGN KEY VIOLATION|foreign\_key\_violation|
|23505|UNIQUE VIOLATION|unique\_violation|
|23514|CHECK VIOLATION|check\_violation|
|**Class 24** — Invalid Cursor State|
|24000|INVALID CURSOR STATE|invalid\_cursor\_state|
|**Class 25** — Invalid Transaction State|
|25000|INVALID TRANSACTION STATE|invalid\_transaction\_state|
|25001|ACTIVE SQL TRANSACTION|active\_sql\_transaction|
|25002|BRANCH TRANSACTION ALREADY ACTIVE|branch\_transaction\_already\_active|
|25008|HELD CURSOR REQUIRES SAME ISOLATION LEVEL|held\_cursor\_requires\_same\_isolation\_level|
|25003|INAPPROPRIATE ACCESS MODE FOR BRANCH TRANSACTION|inappropriate\_access\_mode\_for\_branch\_transaction|
|25004|INAPPROPRIATE ISOLATION LEVEL FOR BRANCH TRANSACTION|inappropriate\_isolation\_level\_for\_branch\_transaction|
|25005|NO ACTIVE SQL TRANSACTION FOR BRANCH TRANSACTION|no\_active\_sql\_transaction\_for\_branch\_transaction|
|25006|READ ONLY SQL TRANSACTION|read\_only\_sql\_transaction|
|25007|SCHEMA AND DATA STATEMENT MIXING NOT SUPPORTED|schema\_and\_data\_statement\_mixing\_not\_supported|
|25P01|NO ACTIVE SQL TRANSACTION|no\_active\_sql\_transaction|
|25P02|IN FAILED SQL TRANSACTION|in\_failed\_sql\_transaction|
|**Class 26** — Invalid SQL Statement Name|
|26000|INVALID SQL STATEMENT NAME|invalid\_sql\_statement\_name|
|**Class 27** — Triggered Data Change Violation|
|27000|TRIGGERED DATA CHANGE VIOLATION|triggered\_data\_change\_violation|
|**Class 28** — Invalid Authorization Specification|
|28000|INVALID AUTHORIZATION SPECIFICATION|invalid\_authorization\_specification|
|**Class 2B** — Dependent Privilege Descriptors Still Exist|
|2B000|DEPENDENT PRIVILEGE DESCRIPTORS STILL EXIST|dependent\_privilege\_descriptors\_still\_exist|
|2BP01|DEPENDENT OBJECTS STILL EXIST|dependent\_objects\_still\_exist|
|**Class 2D** — Invalid Transaction Termination|
|2D000|INVALID TRANSACTION TERMINATION|invalid\_transaction\_termination|
|**Class 2F** — SQL Routine Exception|
|2F000|SQL ROUTINE EXCEPTION|sql\_routine\_exception|
|2F005|FUNCTION EXECUTED NO RETURN STATEMENT|function\_executed\_no\_return\_statement|
|2F002|MODIFYING SQL DATA NOT PERMITTED|modifying\_sql\_data\_not\_permitted|
|2F003|PROHIBITED SQL STATEMENT ATTEMPTED|prohibited\_sql\_statement\_attempted|
|2F004|READING SQL DATA NOT PERMITTED|reading\_sql\_data\_not\_permitted|
|**Class 34** — Invalid Cursor Name|
|34000|INVALID CURSOR NAME|invalid\_cursor\_name|
|**Class 38** — External Routine Exception|
|38000|EXTERNAL ROUTINE EXCEPTION|external\_routine\_exception|
|38001|CONTAINING SQL NOT PERMITTED|containing\_sql\_not\_permitted|
|38002|MODIFYING SQL DATA NOT PERMITTED|modifying\_sql\_data\_not\_permitted|
|38003|PROHIBITED SQL STATEMENT ATTEMPTED|prohibited\_sql\_statement\_attempted|
|38004|READING SQL DATA NOT PERMITTED|reading\_sql\_data\_not\_permitted|
|**Class 39** — External Routine Invocation Exception|
|39000|EXTERNAL ROUTINE INVOCATION EXCEPTION|external\_routine\_invocation\_exception|
|39001|INVALID SQLSTATE RETURNED|invalid\_sqlstate\_returned|
|39004|NULL VALUE NOT ALLOWED|null\_value\_not\_allowed|
|39P01|TRIGGER PROTOCOL VIOLATED|trigger\_protocol\_violated|
|39P02|SRF PROTOCOL VIOLATED|srf\_protocol\_violated|
|**Class 3B** — Savepoint Exception|
|3B000|SAVEPOINT EXCEPTION|savepoint\_exception|
|3B001|INVALID SAVEPOINT SPECIFICATION|invalid\_savepoint\_specification|
|**Class 3D** — Invalid Catalog Name|
|3D000|INVALID CATALOG NAME|invalid\_catalog\_name|
|**Class 3F** — Invalid Schema Name|
|3F000|INVALID SCHEMA NAME|invalid\_schema\_name|
|**Class 40** — Transaction Rollback|
|40000|TRANSACTION ROLLBACK|transaction\_rollback|
|40002|TRANSACTION INTEGRITY CONSTRAINT VIOLATION|transaction\_integrity\_constraint\_violation|
|40001|SERIALIZATION FAILURE|serialization\_failure|
|40003|STATEMENT COMPLETION UNKNOWN|statement\_completion\_unknown|
|40P01|DEADLOCK DETECTED|deadlock\_detected|
|**Class 42** — Syntax Error or Access Rule Violation|
|42000|SYNTAX ERROR OR ACCESS RULE VIOLATION|syntax\_error\_or\_access\_rule\_violation|
|42601|SYNTAX ERROR|syntax\_error|
|42501|INSUFFICIENT PRIVILEGE|insufficient\_privilege|
|42846|CANNOT COERCE|cannot\_coerce|
|42803|GROUPING ERROR|grouping\_error|
|42830|INVALID FOREIGN KEY|invalid\_foreign\_key|
|42602|INVALID NAME|invalid\_name|
|42622|NAME TOO LONG|name\_too\_long|
|42939|RESERVED NAME|reserved\_name|
|42804|DATATYPE MISMATCH|datatype\_mismatch|
|42P18|INDETERMINATE DATATYPE|indeterminate\_datatype|
|42809|WRONG OBJECT TYPE|wrong\_object\_type|
|42703|UNDEFINED COLUMN|undefined\_column|
|42883|UNDEFINED FUNCTION|undefined\_function|
|42P01|UNDEFINED TABLE|undefined\_table|
|42P02|UNDEFINED PARAMETER|undefined\_parameter|
|42704|UNDEFINED OBJECT|undefined\_object|
|42701|DUPLICATE COLUMN|duplicate\_column|
|42P03|DUPLICATE CURSOR|duplicate\_cursor|
|42P04|DUPLICATE DATABASE|duplicate\_database|
|42723|DUPLICATE FUNCTION|duplicate\_function|
|42P05|DUPLICATE PREPARED STATEMENT|duplicate\_prepared\_statement|
|42P06|DUPLICATE SCHEMA|duplicate\_schema|
|42P07|DUPLICATE TABLE|duplicate\_table|
|42712|DUPLICATE ALIAS|duplicate\_alias|
|42710|DUPLICATE OBJECT|duplicate\_object|
|42702|AMBIGUOUS COLUMN|ambiguous\_column|
|42725|AMBIGUOUS FUNCTION|ambiguous\_function|
|42P08|AMBIGUOUS PARAMETER|ambiguous\_parameter|
|42P09|AMBIGUOUS ALIAS|ambiguous\_alias|
|42P10|INVALID COLUMN REFERENCE|invalid\_column\_reference|
|42611|INVALID COLUMN DEFINITION|invalid\_column\_definition|
|42P11|INVALID CURSOR DEFINITION|invalid\_cursor\_definition|
|42P12|INVALID DATABASE DEFINITION|invalid\_database\_definition|
|42P13|INVALID FUNCTION DEFINITION|invalid\_function\_definition|
|42P14|INVALID PREPARED STATEMENT DEFINITION|invalid\_prepared\_statement\_definition|
|42P15|INVALID SCHEMA DEFINITION|invalid\_schema\_definition|
|42P16|INVALID TABLE DEFINITION|invalid\_table\_definition|
|42P17|INVALID OBJECT DEFINITION|invalid\_object\_definition|
|**Class 44** — WITH CHECK OPTION Violation|
|44000|WITH CHECK OPTION VIOLATION|with\_check\_option\_violation|
|**Class 53** — Insufficient Resources|
|53000|INSUFFICIENT RESOURCES|insufficient\_resources|
|53100|DISK FULL|disk\_full|
|53200|OUT OF MEMORY|out\_of\_memory|
|53300|TOO MANY CONNECTIONS|too\_many\_connections|
|**Class 54** — Program Limit Exceeded|
|54000|PROGRAM LIMIT EXCEEDED|program\_limit\_exceeded|
|54001|STATEMENT TOO COMPLEX|statement\_too\_complex|
|54011|TOO MANY COLUMNS|too\_many\_columns|
|54023|TOO MANY ARGUMENTS|too\_many\_arguments|
|**Class 55** — Object Not In Prerequisite State|
|55000|OBJECT NOT IN PREREQUISITE STATE|object\_not\_in\_prerequisite\_state|
|55006|OBJECT IN USE|object\_in\_use|
|55P02|CANT CHANGE RUNTIME PARAM|cant\_change\_runtime\_param|
|55P03|LOCK NOT AVAILABLE|lock\_not\_available|
|**Class 57** — Operator Intervention|
|57000|OPERATOR INTERVENTION|operator\_intervention|
|57014|QUERY CANCELED|query\_canceled|
|57P01|ADMIN SHUTDOWN|admin\_shutdown|
|57P02|CRASH SHUTDOWN|crash\_shutdown|
|57P03|CANNOT CONNECT NOW|cannot\_connect\_now|
|**Class 58** — System Error \(errors external to Greenplum Database \)|
|58030|IO ERROR|io\_error|
|58P01|UNDEFINED FILE|undefined\_file|
|58P02|DUPLICATE FILE|duplicate\_file|
|Class F0 — Configuration File Error|
|F0000|CONFIG FILE ERROR|config\_file\_error|
|F0001|LOCK FILE EXISTS|lock\_file\_exists|
|**Class P0** — PL/pgSQL Error|
|P0000|PLPGSQL ERROR|plpgsql\_error|
|P0001|RAISE EXCEPTION|raise\_exception|
|P0002|NO DATA FOUND|no\_data\_found|
|P0003|TOO MANY ROWS|too\_many\_rows|
|**Class XX** — Internal Error|
|XX000|INTERNAL ERROR|internal\_error|
|XX001|DATA CORRUPTED|data\_corrupted|
|XX002|INDEX CORRUPTED|index\_corrupted|

