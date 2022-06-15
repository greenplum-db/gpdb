# database\_\* 

The `database_*` tables store query workload information for a Greenplum Database instance. There are three database tables, all having the same columns:

-   `database_now` is an external table whose data files are stored in `$MASTER_DATA_DIRECTORY/gpperfmon/data`. Current query workload data is stored in `database_now` during the period between data collection from the data collection agents and automatic commitment to the `database_history` table.
-   `database_tail` is an external table whose data files are stored in `$MASTER_DATA_DIRECTORY/gpperfmon/data`. This is a transitional table for query workload data that has been cleared from `database_now` but has not yet been committed to `database_history`. It typically only contains a few minutes worth of data.
-   `database_history` is a regular table that stores historical database-wide query workload data. It is pre-partitioned into monthly partitions. Partitions are automatically added in two month increments as needed.

|Column|Type|Description|
|------|----|-----------|
|`ctime`|timestamp|Time this row was created.|
|`queries_total`|int|The total number of queries in Greenplum Database at data collection time.|
|`queries_running`|int|The number of active queries running at data collection time.|
|`queries_queued`|int|The number of queries waiting in a resource group or resource queue, depending upon which resource management scheme is active, at data collection time.|

**Parent topic:** [The gpperfmon Database](../gpperfmon/dbref.html)

