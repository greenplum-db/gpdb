# queries\_\* 

The `queries_*` tables store high-level query status information.

The `tmid`, `ssid` and `ccnt` columns are the composite key that uniquely identifies a particular query.

There are three queries tables, all having the same columns:

-   `queries_now` is an external table whose data files are stored in `$MASTER_DATA_DIRECTORY/gpperfmon/data`. Current query status is stored in `queries_now` during the period between data collection from the `gpperfmon` agents and automatic commitment to the `queries_history` table.
-   `queries_tail` is an external table whose data files are stored in `$MASTER_DATA_DIRECTORY/gpperfmon/data`. This is a transitional table for query status data that has been cleared from `queries_now` but has not yet been committed to `queries_history`. It typically only contains a few minutes worth of data.
-   `queries_history` is a regular table that stores historical query status data. It is pre-partitioned into monthly partitions. Partitions are automatically added in two month increments as needed.

|Column|Type|Description|
|------|----|-----------|
|`ctime`|timestamp|Time this row was created.|
|`tmid`|int|A time identifier for a particular query. All records associated with the query will have the same `tmid`.|
|`ssid`|int|The session id as shown by `gp_session_id`. All records associated with the query will have the same `ssid`.|
|`ccnt`|int|The command number within this session as shown by `gp_command_count`. All records associated with the query will have the same `ccnt`.|
|`username`|varchar\(64\)|Greenplum role name that issued this query.|
|`db`<br/>|varchar\(64\)|Name of the database queried.|
|`cost`|int|Not implemented in this release.|
|`tsubmit`|timestamp|Time the query was submitted.|
|`tstart`|timestamp|Time the query was started.|
|`tfinish`|timestamp|Time the query finished.|
|`status`|varchar\(64\)|Status of the query -- `start`, `done`, or `abort`.|
|`rows_out`|bigint|Rows out for the query.|
|`cpu_elapsed`|bigint|CPU usage by all processes across all segments executing this query \(in seconds\). It is the sum of the CPU usage values taken from all active primary segments in the database system.<br/>Note that the value is logged as 0 if the query runtime is shorter than the value for the quantum. This occurs even if the query runtime is greater than the value for `min_query_time`, and this value is lower than the value for the quantum.<br/>|
|`cpu_currpct`|float|Current CPU percent average for all processes executing this query. The percentages for all processes running on each segment are averaged, and then the average of all those values is calculated to render this metric.<br/>Current CPU percent average is always zero in historical and tail data.<br/>|
|`skew_cpu`|float|Displays the amount of processing skew in the system for this query. Processing/CPU skew occurs when one segment performs a disproportionate amount of processing for a query. This value is the coefficient of variation in the CPU% metric across all segments for this query, multiplied by 100. For example, a value of .95 is shown as 95.<br/>|
|`skew_rows`|float|Displays the amount of row skew in the system. Row skew occurs when one segment produces a disproportionate number of rows for a query. This value is the coefficient of variation for the `rows_in` metric across all segments for this query, multiplied by 100. For example, a value of .95 is shown as 95.|
|`query_hash`|bigint|Not implemented in this release.|
|`query_text`|text|The SQL text of this query.|
|`query_plan`|text|Text of the query plan. Not implemented in this release.|
|`application_name`|varchar\(64\)|The name of the application.|
|`rsqname`|varchar\(64\)|If the resource queue-based resource management scheme is active, this column specifies the name of the resource queue.|
|`rqppriority`|varchar\(64\)|If the resource queue-based resource management scheme is active, this column specifies the priority of the query -- `max, high, med, low, or min`.|

**Parent topic:** [The gpperfmon Database](../gpperfmon/dbref.html)

