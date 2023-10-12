---
title: The gpperfmon Database 
---

The `gpperfmon` database is a dedicated database where data collection agents on Greenplum segment hosts save query and system statistics.

The `gpperfmon` database is created using the `gpperfmon_install` command-line utility. The utility creates the database and the `gpmon` database role and enables the data collection agents on the master and segment hosts. See the `gpperfmon_install` reference in the *Greenplum Database Utility Guide* for information about using the utility and configuring the data collection agents.

>**NOTE**
>VMware Greenplum customers should use Greenplum Command Center, rather than the `gpperfmon` database, to monitor VMware Greenplum. If you are an open source Greenplum Database user and plan to run `gpperfrmon_install`, be aware that it is not supported on Red Hat Linux 8.

The `gpperfmon` database consists of three sets of tables that capture query and system status information at different stages.

-   `_now` tables store current system metrics such as active queries.
-   `_tail` tables are used to stage data before it is saved to the `_history` tables. The `_tail` tables are for internal use only and not to be queried by users.
-   `_history` tables store historical metrics.

The data for `_now` and `_tail` tables are stored as text files on the master host file system, and are accessed in the `gpperfmon` database via external tables. The `history` tables are regular heap database tables in the `gpperfmon` database. History is saved only for queries that run for a minimum number of seconds, 20 by default. You can set this threshold to another value by setting the `min_query_time` parameter in the `$MASTER_DATA_DIRECTORY/gpperfmon/conf/gpperfmon.conf` configuration file. Setting the value to 0 saves history for all queries.

> **Note** `gpperfmon` does not support SQL `ALTER` commands. `ALTER` queries are not recorded in the `gpperfmon` query history tables.

The `history` tables are partitioned by month. See [History Table Partition Retention](#section_et2_wmt_n1b) for information about removing old partitions.

The database contains the following categories of tables:

-   The [database\_\*](db-database.html) tables store query workload information for a Greenplum Database instance.
-   The [diskspace\_\*](db-diskspace.html) tables store diskspace metrics.
-   The [log\_alert\_\*](db-log-alert.html) tables store error and warning messages from `pg_log`.
-   The [queries\_\*](db-queries.html) tables store high-level query status information.
-   The [segment\_\*](db-segment.html) tables store memory allocation statistics for the Greenplum Database segment instances.
-   The [socket\_stats\_\*](db-socket-stats.html) tables store statistical metrics about socket usage for a Greenplum Database instance. These tables are in place for future use and are not currently populated.
-   The [system\_\*](db-system.html) tables store system utilization metrics.

The `gpperfmon` database also contains the following views:

-   The [dynamic\_memory\_info](db-dynamic-memory-info.html) view shows an aggregate of all the segments per host and the amount of dynamic memory used per host.
-   The [memory\_info](db-memory-info.html) view shows per-host memory information from the `system_history` and `segment_history` tables.

## <a id="section_et2_wmt_n1b"></a>History Table Partition Retention 

The `history` tables in the `gpperfmon` database are partitioned by month. Partitions are automatically added in two month increments as needed.

The `partition_age` parameter in the `$MASTER_DATA_DIRECTORY/gpperfmon/conf/gpperfmon.conf` file can be set to the maximum number of monthly partitions to keep. Partitions older than the specified value are removed automatically when new partitions are added.

The default value for `partition_age` is `0`, which means that administrators must manually remove unneeded partitions.

## <a id="section_ok2_wd1_41b"></a>Alert Log Processing and Log Rotation 

When the `gp_enable_gpperfmon` server configuration parameter is set to true, the Greenplum Database syslogger writes alert messages to a `.csv` file in the `$MASTER_DATA_DIRECTORY/gpperfmon/logs` directory.

The level of messages written to the log can be set to `none`, `warning`, `error`, `fatal`, or `panic` by setting the `gpperfmon_log_alert_level` server configuration parameter in `postgresql.conf`. The default message level is `warning`.

The directory where the log is written can be changed by setting the `log_location` configuration variable in the `$MASTER_DATA_DIRECTORY/gpperfmon/conf/gpperfmon.conf` configuration file.

The syslogger rotates the alert log every 24 hours or when the current log file reaches or exceeds 1MB.

A rotated log file can exceed 1MB if a single error message contains a large SQL statement or a large stack trace. Also, the syslogger processes error messages in chunks, with a separate chunk for each logging process. The size of a chunk is OS-dependent; on Red Hat Enterprise Linux, for example, it is 4096 bytes. If many Greenplum Database sessions generate error messages at the same time, the log file can grow significantly before its size is checked and log rotation is triggered.

## <a id="rotation"></a>gpperfmon Data Collection Process 

When Greenplum Database starts up with gpperfmon support enabled, it forks a `gpmmon` agent process. `gpmmon` then starts a `gpsmon` agent process on the master host and every segment host in the Greenplum Database cluster. The Greenplum Database postmaster process monitors the `gpmmon` process and restarts it if needed, and the `gpmmon` process monitors and restarts `gpsmon` processes as needed.

The `gpmmon` process runs in a loop and at configurable intervals retrieves data accumulated by the `gpsmon` processes, adds it to the data files for the `_now` and `_tail` external database tables, and then into the `_history` regular heap database tables.

> **Note** The `log_alert` tables in the `gpperfmon` database follow a different process, since alert messages are delivered by the Greenplum Database system logger instead of through `gpsmon`. See [Alert Log Processing and Log Rotation](#section_ok2_wd1_41b) for more information.

Two configuration parameters in the `$MASTER_DATA_DIRECTORY/gpperfmon/conf/gpperfmon.conf` configuration file control how often `gpmmon` activities are triggered:

-   The `quantum` parameter is how frequently, in seconds, `gpmmon` requests data from the `gpsmon` agents on the segment hosts and adds retrieved data to the `_now` and `_tail` external table data files. Valid values for the `quantum` parameter are 10, 15, 20, 30, and 60. The default is 15.
-   The `harvest_interval` parameter is how frequently, in seconds, data in the `_tail` tables is moved to the `_history` tables. The `harvest_interval` must be at least 30. The default is 120.

See the `gpperfmon_install` management utility reference in the *Greenplum Database Utility Guide* for the complete list of gpperfmon configuration parameters.

The following steps describe the flow of data from Greenplum Database into the `gpperfmon` database when gpperfmon support is enabled.

1.  While executing queries, the Greenplum Database query dispatcher and query executor processes send out query status messages in UDP datagrams. The `gp_gpperfmon_send_interval` server configuration variable determines how frequently the database sends these messages. The default is every second.
2.  The `gpsmon` process on each host receives the UDP packets, consolidates and summarizes the data they contain, and adds additional host metrics, such as CPU and memory usage.
3.  The `gpsmon` processes continue to accumulate data until they receive a dump command from `gpmmon`.
4.  The `gpsmon` processes respond to a dump command by sending their accumulated status data and log alerts to a listening `gpmmon` event handler thread.
5.  The `gpmmon` event handler saves the metrics to `.txt` files in the `$MASTER_DATA_DIRECTORY/gpperfmon/data` directory on the master host.

At each `quantum` interval \(15 seconds by default\), `gpmmon` performs the following steps:

1.  Sends a dump command to the `gpsmon` processes.
2.  Gathers and converts the `.txt` files saved in `the $MASTER_DATA_DIRECTORY/gpperfmon/data` directory into `.dat` external data files for the `_now` and `_tail` external tables in the `gpperfmon` database.

    For example, disk space metrics are added to the `diskspace_now.dat` and `_diskspace_tail.dat` delimited text files. These text files are accessed via the `diskspace_now` and `_diskspace_tail` tables in the `gpperfmon` database.


At each `harvest_interval` \(120 seconds by default\), `gpmmon` performs the following steps for each `_tail` file:

1.  Renames the `_tail` file to a `_stage` file.
2.  Creates a new `_tail` file.
3.  Appends data from the `_stage` file into the `_tail` file.
4.  Runs a SQL command to insert the data from the `_tail` external table into the corresponding `_history` table.

    For example, the contents of the `_database_tail` external table is inserted into the `database_history` regular \(heap\) table.

5.  Deletes the `_tail` file after its contents have been loaded into the database table.
6.  Gathers all of the `gpdb-alert-*.csv` files in the `$MASTER_DATA_DIRECTORY/gpperfmon/logs` directory \(except the most recent, which the syslogger has open and is writing to\) into a single file, `alert_log_stage`.
7.  Loads the `alert_log_stage` file into the `log_alert_history` table in the `gpperfmon` database.
8.  Truncates the `alert_log_stage` file.

The following topics describe the contents of the tables in the `gpperfmon` database.

-   **[database\_\*](../gpperfmon/db-database.html)**  

-   **[diskspace\_\*](../gpperfmon/db-diskspace.html)**  

-   **[interface\_stats\_\*](../gpperfmon/db-interface-stats.html)**  

-   **[log\_alert\_\*](../gpperfmon/db-log-alert.html)**  

-   **[queries\_\*](../gpperfmon/db-queries.html)**  

-   **[segment\_\*](../gpperfmon/db-segment.html)**  

-   **[socket\_stats\_\*](../gpperfmon/db-socket-stats.html)**  

-   **[system\_\*](../gpperfmon/db-system.html)**  

-   **[dynamic\_memory\_info](../gpperfmon/db-dynamic-memory-info.html)**  

-   **[memory\_info](../gpperfmon/db-memory-info.html)**  


**Parent topic:** [Greenplum Database Reference Guide](../ref_guide.html)

