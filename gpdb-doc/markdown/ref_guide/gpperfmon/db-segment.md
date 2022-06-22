# segment\_\* 

The `segment_*` tables contain memory allocation statistics for the Greenplum Database segment instances. This tracks the amount of memory consumed by all postgres processes of a particular segment instance, and the remaining amount of memory available to a segment as per the settings configured by the currently active resource management scheme \(resource group-based or resource queue-based\). See the *Greenplum Database Administrator Guide* for more information about resource management schemes.

There are three segment tables, all having the same columns:

-   `segment_now` is an external table whose data files are stored in `$MASTER_DATA_DIRECTORY/gpperfmon/data`. Current memory allocation data is stored in `segment_now` during the period between data collection from the `gpperfmon` agents and automatic commitment to the segment\_history table.
-   `segment_tail` is an external table whose data files are stored in `$MASTER_DATA_DIRECTORY/gpperfmon/data`. This is a transitional table for memory allocation data that has been cleared from `segment_now` but has not yet been committed to `segment_history`. It typically only contains a few minutes worth of data.
-   `segment_history` is a regular table that stores historical memory allocation metrics. It is pre-partitioned into monthly partitions. Partitions are automatically added in two month increments as needed.

A particular segment instance is identified by its `hostname` and `dbid` \(the unique segment identifier as per the `gp_segment_configuration` system catalog table\).

|Column|Type|Description|
|------|----|-----------|
|`ctime`|timestamp\(0\)<br/>\(without time zone\)<br/>|The time the row was created.<br/>|
|`dbid`|int|The segment ID \(`dbid` from `gp_segment_configuration`\).|
|`hostname`|charvar\(64\)|The segment hostname.|
|`dynamic_memory_used`|bigint|The amount of dynamic memory \(in bytes\) allocated to query processes running on this segment.|
|`dynamic_memory_available`|bigint|The amount of additional dynamic memory \(in bytes\) that the segment can request before reaching the limit set by the currently active resource management scheme \(resource group-based or resource queue-based\).|

See also the views `memory_info` and `dynamic_memory_info` for aggregated memory allocation and utilization by host.

**Parent topic:** [The gpperfmon Database](../gpperfmon/dbref.html)

