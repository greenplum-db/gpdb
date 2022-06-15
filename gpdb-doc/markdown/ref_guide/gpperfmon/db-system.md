# system\_\* 

The `system_*` tables store system utilization metrics. There are three system tables, all having the same columns:

-   `system_now` is an external table whose data files are stored in `$MASTER_DATA_DIRECTORY/gpperfmon/data`. Current system utilization data is stored in `system_now` during the period between data collection from the `gpperfmon` agents and automatic commitment to the `system_history` table.
-   `system_tail` is an external table whose data files are stored in `$MASTER_DATA_DIRECTORY/gpperfmon/data`. This is a transitional table for system utilization data that has been cleared from `system_now` but has not yet been committed to `system_history`. It typically only contains a few minutes worth of data.
-   `system_history` is a regular table that stores historical system utilization metrics. It is pre-partitioned into monthly partitions. Partitions are automatically added in two month increments as needed.

|Column|Type|Description|
|------|----|-----------|
|`ctime`|timestamp|Time this row was created.|
|`hostname`|varchar\(64\)|Segment or master hostname associated with these system metrics.|
|`mem_total`|bigint|Total system memory in Bytes for this host.|
|`mem_used`|bigint|Used system memory in Bytes for this host.|
|`mem_actual_used`|bigint|Used actual memory in Bytes for this host \(not including the memory reserved for cache and buffers\).|
|`mem_actual_free`|bigint|Free actual memory in Bytes for this host \(not including the memory reserved for cache and buffers\).|
|`swap_total`|bigint|Total swap space in Bytes for this host.|
|`swap_used`|bigint|Used swap space in Bytes for this host.|
|`swap_page_in`|bigint|Number of swap pages in.|
|`swap_page_out`|bigint|Number of swap pages out.|
|`cpu_user`|float|CPU usage by the Greenplum system user.|
|`cpu_sys`|float|CPU usage for this host.|
|`cpu_idle`|float|Idle CPU capacity at metric collection time.|
|`load0`|float|CPU load average for the prior one-minute period.|
|`load1`|float|CPU load average for the prior five-minute period.|
|`load2`|float|CPU load average for the prior fifteen-minute period.|
|`quantum`|int|Interval between metric collection for this metric entry.|
|`disk_ro_rate`|bigint|Disk read operations per second.|
|`disk_wo_rate`|bigint|Disk write operations per second.|
|`disk_rb_rate`|bigint|Bytes per second for disk read operations.|
|`disk_wb_rate`|bigint|Bytes per second for disk write operations.|
|`net_rp_rate`|bigint|Packets per second on the system network for read operations.|
|`net_wp_rate`|bigint|Packets per second on the system network for write operations.|
|`net_rb_rate`|bigint|Bytes per second on the system network for read operations.|
|`net_wb_rate`|bigint|Bytes per second on the system network for write operations.|

**Parent topic:** [The gpperfmon Database](../gpperfmon/dbref.html)

