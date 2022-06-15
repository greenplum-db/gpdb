# log\_alert\_\* 

The `log_alert_*` tables store `pg_log` errors and warnings.

See [Alert Log Processing and Log Rotation](dbref.md#section_ok2_wd1_41b) for information about configuring the system logger for `gpperfmon`.

There are three `log_alert` tables, all having the same columns:

-   `log_alert_now` is an external table whose data is stored in `.csv` files in the `$MASTER_DATA_DIRECTORY/gpperfmon/logs` directory. Current `pg_log` errors and warnings data are available in `log_alert_now` during the period between data collection from the `gpperfmon` agents and automatic commitment to the `log_alert_history` table.
-   `log_alert_tail` is an external table with data stored in `$MASTER_DATA_DIRECTORY/gpperfmon/logs/alert_log_stage`. This is a transitional table for data that has been cleared from `log_alert_now` but has not yet been committed to `log_alert_history`. The table includes records from all alert logs except the most recent. It typically contains only a few minutes' worth of data.
-   `log_alert_history` is a regular table that stores historical database-wide errors and warnings data. It is pre-partitioned into monthly partitions. Partitions are automatically added in two month increments as needed.

|Column|Type|Description|
|------|----|-----------|
|`logtime`|timestamp with time zone|Timestamp for this log|
|`loguser`|text|User of the query|
|`logdatabase`|text|The accessed database|
|`logpid`|text|Process id|
|`logthread`|text|Thread number|
|`loghost`|text|Host name or ip address|
|`logport`|text|Port number|
|`logsessiontime`|timestamp with time zone|Session timestamp|
|`logtransaction`|integer|Transaction id|
|`logsession`|text|Session id|
|`logcmdcount`|text|Command count|
|`logsegment`|text|Segment number|
|`logslice`|text|Slice number|
|`logdistxact`|text|Distributed transaction|
|`loglocalxact`|text|Local transaction|
|`logsubxact`|text|Subtransaction|
|`logseverity`|text|Log severity|
|`logstate`|text|State|
|`logmessage`|text|Log message|
|`logdetail`|text|Detailed message|
|`loghint`|text|Hint info|
|`logquery`|text|Executed query|
|`logquerypos`|text|Query position|
|`logcontext`|text|Context info|
|`logdebug`|text|Debug|
|`logcursorpos`|text|Cursor position|
|`logfunction`|text|Function info|
|`logfile`|text|Source code file|
|`logline`|text|Source code line|
|`logstack`|text|Stack trace|

**Parent topic:** [The gpperfmon Database](../gpperfmon/dbref.html)

