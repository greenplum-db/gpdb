# dynamic\_memory\_info 

The `dynamic_memory_info` view shows a sum of the used and available dynamic memory for all segment instances on a segment host. Dynamic memory refers to the maximum amount of memory that Greenplum Database instance will allow the query processes of a single segment instance to consume before it starts cancelling processes. This limit, determined by the currently active resource management scheme \(resource group-based or resource queue-based\), is evaluated on a per-segment basis.

|Column|Type|Description|
|------|----|-----------|
|`ctime`|timestamp\(0\) without time zone|Time this row was created in the `segment_history` table.|
|`hostname`|varchar\(64\)|Segment or master hostname associated with these system memory metrics.|
|`dynamic_memory_used_mb`|numeric|The amount of dynamic memory in MB allocated to query processes running on this segment.|
|`dynamic_memory_available_mb`|numeric|The amount of additional dynamic memory \(in MB\) available to the query processes running on this segment host. Note that this value is a sum of the available memory for all segments on a host. Even though this value reports available memory, it is possible that one or more segments on the host have exceeded their memory limit.|

**Parent topic:** [The gpperfmon Database](../gpperfmon/dbref.html)

