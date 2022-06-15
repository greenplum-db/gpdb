# interface\_stats\_\* 

The `interface_stats_*` tables store statistical metrics about communications over each active interface for a Greenplum Database instance.

These tables are in place for future use and are not currently populated.

There are three `interface_stats` tables, all having the same columns:

-   `interface_stats_now` is an external table whose data files are stored in `$MASTER_DATA_DIRECTORY/gpperfmon/data`.
-   `interface_stats_tail` is an external table whose data files are stored in `$MASTER_DATA_DIRECTORY/gpperfmon/data`. This is a transitional table for statistical interface metrics that has been cleared from `interface_stats_now` but has not yet been committed to `interface_stats_history`. It typically only contains a few minutes worth of data.
-   `interface_stats_history` is a regular table that stores statistical interface metrics. It is pre-partitioned into monthly partitions. Partitions are automatically added in one month increments as needed.

|Column|Type|Description|
|------|----|-----------|
|`interface_name`|string|Name of the interface. For example: eth0, eth1, lo.|
|`bytes_received`|bigint|Amount of data received in bytes.|
|`packets_received`|bigint|Number of packets received.|
|`receive_errors`|bigint|Number of errors encountered while data was being received.|
|`receive_drops`|bigint|Number of times packets were dropped while data was being received.|
|`receive_fifo_errors`|bigint|Number of times FIFO \(first in first out\) errors were encountered while data was being received.|
|`receive_frame_errors`|bigint|Number of frame errors while data was being received.|
|`receive_compressed_packets`|int|Number of packets received in compressed format.|
|`receive_multicast_packets`|int|Number of multicast packets received.|
|`bytes_transmitted`|bigint|Amount of data transmitted in bytes.|
|`packets_transmitted`|bigint|Amount of data transmitted in bytes.|
|`packets_transmitted`|bigint|Number of packets transmitted.|
|`transmit_errors`|bigint|Number of errors encountered during data transmission.|
|`transmit_drops`|bigint|Number of times packets were dropped during data transmission.|
|`transmit_fifo_errors`|bigint|Number of times fifo errors were encountered during data transmission.|
|`transmit_collision_errors`|bigint|Number of times collision errors were encountered during data transmission.|
|`transmit_carrier_errors`|bigint|Number of times carrier errors were encountered during data transmission.|
|`transmit_compressed_packets`|int|Number of packets transmitted in compressed format.|

**Parent topic:** [The gpperfmon Database](../gpperfmon/dbref.html)

