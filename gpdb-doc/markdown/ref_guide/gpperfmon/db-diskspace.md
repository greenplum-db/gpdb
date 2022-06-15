# diskspace\_\* 

The `diskspace_*` tables store diskspace metrics.

-   `diskspace_now` is an external table whose data files are stored in `$MASTER_DATA_DIRECTORY/gpperfmon/data`. Current diskspace metrics are stored in `database_now` during the period between data collection from the `gpperfmon` agents and automatic commitment to the `diskspace_history` table.
-   `diskspace_tail` is an external table whose data files are stored in `$MASTER_DATA_DIRECTORY/gpperfmon/data`. This is a transitional table for diskspace metrics that have been cleared from `diskspace_now` but has not yet been committed to `diskspace_history`. It typically only contains a few minutes worth of data.
-   `diskspace_history` is a regular table that stores historical diskspace metrics. It is pre-partitioned into monthly partitions. Partitions are automatically added in two month increments as needed.

|Column|Type|Description|
|------|----|-----------|
|`ctime`|timestamp\(0\) without time zone |Time of diskspace measurement.|
|`hostname`| varchar\(64\)|The hostname associated with the diskspace measurement.|
|`Filesystem`|text|Name of the filesystem for the diskspace measurement.|
|`total_bytes`|bigint|Total bytes in the file system.|
|`bytes_used`|bigint|Total bytes used in the file system.|
|`bytes_available`|bigint|Total bytes available in file system.|

**Parent topic:** [The gpperfmon Database](../gpperfmon/dbref.html)

