# socket\_stats\_\* 

The `socket_stats_*` tables store statistical metrics about socket usage for a Greenplum Database instance. There are three system tables, all having the same columns:

These tables are in place for future use and are not currently populated.

-   `socket_stats_now` is an external table whose data files are stored in `$MASTER_DATA_DIRECTORY/gpperfmon/data`.
-   `socket_stats_tail` is an external table whose data files are stored in `$MASTER_DATA_DIRECTORY/gpperfmon/data`. This is a transitional table for socket statistical metrics that has been cleared from `socket_stats_now` but has not yet been committed to `socket_stats_history`. It typically only contains a few minutes worth of data.
-   `socket_stats_history` is a regular table that stores historical socket statistical metrics. It is pre-partitioned into monthly partitions. Partitions are automatically added in two month increments as needed.

|Column|Type|Description|
|------|----|-----------|
|`total_sockets_used`|int|Total sockets used in the system.|
|`tcp_sockets_inuse`|int|Number of TCP sockets in use.|
|`tcp_sockets_orphan`|int|Number of TCP sockets orphaned.|
|`tcp_sockets_timewait`|int|Number of TCP sockets in Time-Wait.|
|`tcp_sockets_alloc`|int|Number of TCP sockets allocated.|
|`tcp_sockets_memusage_inbytes`|int|Amount of memory consumed by TCP sockets.|
|`udp_sockets_inuse`|int|Number of UDP sockets in use.|
|`udp_sockets_memusage_inbytes`|int|Amount of memory consumed by UDP sockets.|
|`raw_sockets_inuse`|int|Number of RAW sockets in use.|
|`frag_sockets_inuse`|int|Number of FRAG sockets in use.|
|`frag_sockets_memusage_inbytes`|int|Amount of memory consumed by FRAG sockets.|

**Parent topic:** [The gpperfmon Database](../gpperfmon/dbref.html)

