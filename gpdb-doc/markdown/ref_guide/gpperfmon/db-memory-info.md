# memory\_info 

The `memory_info` view shows per-host memory information from the `system_history` and `segment_history` tables. This allows administrators to compare the total memory available on a segment host, total memory used on a segment host, and dynamic memory used by query processes.

|Column|Type|Description|
|------|----|-----------|
|`ctime`|timestamp\(0\) without time zone|Time this row was created in the `segment_history` table.|
|`hostname`|varchar\(64\)|Segment or master hostname associated with these system memory metrics.|
|`mem_total_mb`|numeric|Total system memory in MB for this segment host.|
|`mem_used_mb`|numeric|Total system memory used in MB for this segment host.|
|`mem_actual_used_mb`|numeric|Actual system memory used in MB for this segment host.|
|`mem_actual_free_mb`|numeric|Actual system memory free in MB for this segment host.|
|`swap_total_mb`|numeric|Total swap space in MB for this segment host.|
|`swap_used_mb`|numeric|Total swap space used in MB for this segment host.|
|`dynamic_memory_used_mb`|numeric|The amount of dynamic memory in MB allocated to query processes running on this segment.|
|`dynamic_memory_available_mb`|numeric|The amount of additional dynamic memory \(in MB\) available to the query processes running on this segment host. Note that this value is a sum of the available memory for all segments on a host. Even though this value reports available memory, it is possible that one or more segments on the host have exceeded their memory limit.|

**Parent topic:** [The gpperfmon Database](../gpperfmon/dbref.html)

