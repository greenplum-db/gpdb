---
title: How Greenplum Database Detects a Failed Segment 
---

With segment mirroring enabled, Greenplum Database automatically fails over to a mirror segment instance when a primary segment instance goes down. Provided one segment instance is online per portion of data, users may not realize a segment is down. If a transaction is in progress when a fault occurs, the in-progress transaction rolls back and restarts automatically on the reconfigured set of segments. The [gpstate](../../../utility_guide/ref/gpstate.html) utility can be used to identify failed segments. The utility displays information from the catalog tables including [gp\_segment\_configuration](../../../ref_guide/system_catalogs/gp_segment_configuration.html).

If the entire Greenplum Database system becomes nonoperational due to a segment failure \(for example, if mirroring is not enabled or not enough segments are online to access all user data\), users will see errors when trying to connect to a database. The errors returned to the client program may indicate the failure. For example:

```
ERROR: All segment databases are unavailable
```

## <a id="howseg"></a>How a Segment Failure is Detected and Managed 

On the Greenplum Database master host, the Postgres `postmaster` process forks a fault probe process, `ftsprobe`. This is also known as the FTS \(Fault Tolerance Server\) process. The `postmaster` process restarts the FTS if it fails.

The FTS runs in a loop with a sleep interval between each cycle. On each loop, the FTS probes each primary segment instance by making a TCP socket connection to the segment instance using the hostname and port registered in the `gp_segment_configuration` table. If the connection succeeds, the segment performs a few simple checks and reports back to the FTS. The checks include running a `stat` system call on critical segment directories and checking for internal faults in the segment instance. If no issues are detected, a positive reply is sent to the FTS and no action is taken for that segment instance.

If the connection cannot be made, or if a reply is not received in the timeout period, then a retry is attempted for the segment instance. If the configured maximum number of probe attempts fail, the FTS probes the segment's mirror to ensure that it is up, and then updates the `gp_segment_configuration` table, marking the primary segment "down" and setting the mirror to act as the primary. The FTS updates the [gp\_configuration\_history](../../../ref_guide/system_catalogs/gp_configuration_history.html) table with the operations performed.

When there is only an active primary segment and the corresponding mirror is down, the primary goes into the *Not In Sync* state and continues logging database changes, so the mirror can be synchronized without performing a full copy of data from the primary to the mirror.

## <a id="confts"></a>Configuring FTS Behavior 

There is a set of server configuration parameters that affect FTS behavior:

gp\_fts\_probe\_interval
:   How often, in seconds, to begin a new FTS loop. For example if the setting is 60 and the probe loop takes 10 seconds, the FTS process sleeps 50 seconds. If the setting is 60 and probe loop takes 75 seconds, the process sleeps 0 seconds. The default is 60, and the maximum is 3600.

gp\_fts\_probe\_timeout
:   Probe timeout between master and segment, in seconds. The default is 20, and the maximum is 3600.

gp\_fts\_probe\_retries
:   The number of attempts to probe a segment. For example if the setting is 5 there will be 4 retries after the first attempt fails. Default: 5

gp\_log\_fts
:   Logging level for FTS. The value may be "off", "terse", "verbose", or "debug". The "verbose" setting can be used in production to provide useful data for troubleshooting. The "debug" setting should not be used in production. Default: "terse"

gp\_segment\_connect\_timeout
:   The maximum time \(in seconds\) allowed for a mirror to respond. Default: 180 \(3 minutes\)

In addition to the fault checking performed by the FTS, a primary segment that is unable to send data to its mirror can change the status of the mirror to down. The primary queues up the data and after `gp_segment_connect_timeout` seconds pass, indicates a mirror failure, causing the mirror to be marked down and the primary to go into `Not In Sync` mode.

-   **[Checking for Failed Segments](../../highavail/topics/g-checking-for-failed-segments.html)**  


**Parent topic:**[Enabling High Availability and Data Consistency Features](../../highavail/topics/g-enabling-high-availability-features.html)

