---
title: Monitoring Greenplum Database Log Files 
---

Know the location and content of system log files and monitor them on a regular basis and not just when problems arise.

The following table shows the locations of the various Greenplum Database log files. In file paths:

-   `$GPADMIN_HOME` refers to the home directory of the `gpadmin` operating system user.
-   `$COORDINATOR_DATA_DIRECTORY` refers to the coordinator data directory on the Greenplum Database coordinator host.
-   `$GPDATA_DIR` refers to a data directory on the Greenplum Database segment host.
-   `host` identifies the Greenplum Database segment host name.
-   `segprefix` identifies the segment prefix.
-   `N` identifies the segment instance number.
-   `date` is a date in the format `YYYYMMDD`.



|Path|Description|
|----|-----------|
|`$GPADMIN_HOME/gpAdminLogs/*`|Many different types of log files, directory on each server. `$GPADMIN_HOME` is the default location for the `gpAdminLogs/` directory. You can specify a different location when you run an administrative utility command.|
|`$GPADMIN_HOME/gpAdminLogs/gpinitsystem_date.log`|system initialization log|
|`$GPADMIN_HOME/gpAdminLogs/gpstart_date.log`|start log|
|`$GPADMIN_HOME/gpAdminLogs/gpstop_date.log`|stop log|
|`$GPADMIN_HOME/gpAdminLogs/gpsegstart.py_host:gpadmin_date.log`|segment host start log|
|`$GPADMIN_HOME/gpAdminLogs/gpsegstop.py_host:gpadmin_date.log`|segment host stop log|
|`$COORDINATOR_DATA_DIRECTORY/log/startup.log`, `$GPDATA_DIR/segprefixN/log/startup.log`|segment instance start log|
|`$COORDINATOR_DATA_DIRECTORY/log/*.csv`, `$GPDATA_DIR/segprefixN/log/*.csv`|coordinator and segment database logs|
|`$GPDATA_DIR/mirror/segprefixN/log/*.csv`|mirror segment database logs|
|`$GPDATA_DIR/primary/segprefixN/log/*.csv`|primary segment database logs|
|`/var/log/messages`|Global Linux system messages|

> **Note** If you manually set the server configuration parameter [log_directory](../ref_guide/config_params/guc-list.html#log_directory) to specify a different directory for log files, the items on the table above whose path is under `$COORDINATOR_DATA_DIRECTORY` or `$GPDATA_DIR` should point to the value of `log_directory` instead. Note that when you specify the value as an absolute path, or as a relative path that is outside the data directory, Greenplum appends a subdirectory with a unique identifier (DBID) to the directory specified by this parameter. The unique identifier matches the value of `dbid` from `gp_segment_configuration`. For example, if you set `log_directory` as `/tmp/logs`, Greenplum creates the directories: `/tmp/logs/1` for the coordinator, `/tmp/logs/2` for seg0, `/tmp/logs/3` for seg1, etcetera. 

Use `gplogfilter -t` \(`--trouble`\) first to search the coordinator log for messages beginning with `ERROR:`, `FATAL:`, or `PANIC:`. Messages beginning with `WARNING` may also provide useful information.

To search log files on the segment hosts, use the Greenplum `gplogfilter` utility with `gpssh` to connect to segment hosts from the coordinator host. You can identify corresponding log entries in segment logs by the `statement_id`.

Greenplum Database can be configured to rotate database logs based on the size and/or age of the current log file. The `log_rotation_size` configuration parameter sets the size of an individual log file that triggers rotation. When the current log file size is equal to or greater than this size, the file is closed and a new log file is created. The `log_rotation_age` configuration parameter specifies the age of the current log file that triggers rotation. When the specified time has elapsed since the current log file was created, a new log file is created. The default `log_rotation_age`, 1d, creates a new log file 24 hours after the current log file was created.

**Parent topic:** [System Monitoring and Maintenance](maintenance.html)

