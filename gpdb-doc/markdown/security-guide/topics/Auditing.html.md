# Auditing 

Describes Greenplum Database events that are logged and should be monitored to detect security threats.

Greenplum Database is capable of auditing a variety of events, including startup and shutdown of the system, segment database failures, SQL statements that result in an error, and all connection attempts and disconnections. Greenplum Database also logs SQL statements and information regarding SQL statements, and can be configured in a variety of ways to record audit information with more or less detail. The `log_error_verbosity` configuration parameter controls the amount of detail written in the server log for each message that is logged.  Similarly, the `log_min_error_statement` parameter allows administrators to configure the level of detail recorded specifically for SQL statements, and the `log_statement` parameter determines the kind of SQL statements that are audited. Greenplum Database records the username for all auditable events, when the event is initiated by a subject outside the Greenplum Database.

Greenplum Database prevents unauthorized modification and deletion of audit records by only allowing administrators with an appropriate role to perform any operations on log files.  Logs are stored in a proprietary format using comma-separated values \(CSV\).  Each segment and the master stores its own log files, although these can be accessed remotely by an administrator.  Greenplum Database also authorizes overwriting of old log files via the `log_truncate_on_rotation` parameter.  This is a local parameter and must be set on each segment and master configuration file.

Greenplum provides an administrative schema called `gp_toolkit` that you can use to query log files, as well as system catalogs and operating environment for system status information. For more information, including usage, refer to *The gp\_tookit Administrative Schema* appendix in the *Greenplum Database Reference Guide*.

## <a id="viewing"></a>Viewing the Database Server Log Files 

Every database instance in Greenplum Database \(master and segments\) is a running PostgreSQL database server with its own server log file. Daily log files are created in the `pg_log` directory of the master and each segment data directory.

The server log files are written in comma-separated values \(CSV\) format. Not all log entries will have values for all of the log fields. For example, only log entries associated with a query worker process will have the `slice_id` populated. Related log entries of a particular query can be identified by its session identifier \(`gp_session_id`\) and command identifier \(`gp_command_count`\).

|\# |Field Name |Data Type |Description |
|:--|:----------|:---------|:-----------|
|1 |event\_time |timestamp with time zone |Time that the log entry was written to the log |
|2 |user\_name |varchar\(100\) |The database user name |
|3 |database\_name |varchar\(100\) |The database name |
|4 |process\_id |varchar\(10\) |The system process id \(prefixed with "p"\) |
|5 |thread\_id |varchar\(50\) |The thread count \(prefixed with "th"\) |
|6 |remote\_host |varchar\(100\) |On the master, the hostname/address of the client machine. On the segment, the hostname/address of the master. |
|7 |remote\_port |varchar\(10\) |The segment or master port number |
|8 |session\_start\_time |timestamp with time zone |Time session connection was opened |
|9 |transaction\_id |int |Top-level transaction ID on the master. This ID is the parent of any subtransactions. |
|10 |gp\_session\_id |text |Session identifier number \(prefixed with "con"\) |
|11 |gp\_command\_count |text |The command number within a session \(prefixed with "cmd"\) |
|12 |gp\_segment |text |The segment content identifier \(prefixed with "seg" for primaries or "mir" for mirrors\). The master always has a content id of -1. |
|13 |slice\_id |text |The slice id \(portion of the query plan being run\) |
|14 |distr\_tranx\_id text |Distributed transaction ID | 
|15 |local\_tranx\_id |text |Local transaction ID |
|16 |sub\_tranx\_id |text |Subtransaction ID |
|17 |event\_severity  |varchar\(10\) |Values include: LOG, ERROR, FATAL, PANIC, DEBUG1, DEBUG2 |
|18 |sql\_state\_code |varchar\(10\) |SQL state code associated with the log message |
|19 |event\_message |text |Log or error message text |
|20 |event\_detail |text |Detail message text associated with an error or warning message |
|21 |event\_hint |text |Hint message text associated with an error or warning message |
|22 |internal\_query |text |The internally-generated query text |
|23 |internal\_query\_pos |int |The cursor index into the internally-generated query text |
|24 |event\_context |text |The context in which this message gets generated |
|25 |debug\_query\_string |text |User-supplied query string with full detail for debugging. This string can be modified for internal use. |
|26 |error\_cursor\_pos |int |The cursor index into the query string |
|27 |func\_name |text |The function in which this message is generated |
|28 |file\_name |text |The internal code file where the message originated |
|29 |file\_line |int |The line of the code file where the message originated |
|30 |stack\_trace |text |Stack trace text associated with this message |

Greenplum provides a utility called `gplogfilter` that can be used to search through a Greenplum Database log file for entries matching the specified criteria. By default, this utility searches through the Greenplum master log file in the default logging location. For example, to display the last three lines of the master log file:

```
$ gplogfilter -n 3
```

You can also use `gplogfilter` to search through all segment log files at once by running it through the `gpssh` utility. For example, to display the last three lines of each segment log file:

```
$ gpssh -f seg_host_file
  => source /usr/local/greenplum-db/greenplum_path.sh
  => gplogfilter -n 3 /data*/*/gp*/pg_log/gpdb*.csv
```

The following are the Greenplum security-related audit \(or logging\) server configuration parameters that are set in the postgresql.conf configuration file:

|Field Name |Value Range |Default |Description |
|:-----------|:------------|:--------|:------------|
|log\_connections |Boolean |off |This outputs a line to the server log detailing each successful connection. Some client programs, like psql, attempt to connect twice while determining if a password is required, so duplicate “connection received” messages do not always indicate a problem. |
|log\_disconnections |Boolean |off |This outputs a line in the server log at termination of a client session, and includes the duration of the session. |
|log\_statement |NONE<br/>DDL<br/>MOD<br/>ALL |ALL |Controls which SQL statements are logged. DDL logs all data definition commands like CREATE, ALTER, and DROP commands. MOD logs all DDL statements, plus INSERT, UPDATE, DELETE, TRUNCATE, and COPY FROM. PREPARE and EXPLAIN ANALYZE statements are also logged if their contained command is of an appropriate type. |
|log\_hostname |Boolean |off |By default, connection log messages only show the IP address of the connecting host. Turning on this option causes logging of the host name as well. Note that depending on your host name resolution setup this might impose a non-negligible performance penalty. |
|log\_duration |Boolean |off |Causes the duration of every completed statement which satisfies log\_statement to be logged. |
|log\_error\_verbosity |TERSE<br/>DEFAULT<br/>VERBOSE |DEFAULT |Controls the amount of detail written in the server log for each message that is logged. |
|log\_min\_duration\_statement |number of milliseconds, 0, -1 |-1 |Logs the statement and its duration on a single log line if its duration is greater than or equal to the specified number of milliseconds. Setting this to 0 will print all statements and their durations. -1 deactivates the feature. For example, if you set it to 250 then all SQL statements that run 250ms or longer will be logged. Enabling this option can be useful in tracking down unoptimized queries in your applications. |
|log\_min\_messages |DEBUG5<br/>DEBUG4<br/>DEBUG3<br/>DEBUG2<br/>DEBUG1<br/>INFO<br/>NOTICE<br/>WARNING<br/>ERROR<br/>LOG<br/>FATAL<br/>PANIC |NOTICE |Controls which message levels are written to the server log. Each level includes all the levels that follow it. The later the level, the fewer messages are sent to the log. |
|log\_rotation\_size |0 - INT\_MAX/1024 kilobytes|1048576|When greater than 0, a new log file is created when this number of kilobytes have been written to the log. Set to zero to deactivate size-based creation of new log files. |
|log\_rotation\_age |Any valid time expression \(number and unit\) |1d |Determines the lifetime of an individual log file. When this amount of time has elapsed since the current log file was created, a new log file will be created. Set to zero to deactivate time-based creation of new log files. |
|log\_statement\_stats |Boolean |off |For each query, write total performance statistics of the query parser, planner, and executor to the server log. This is a crude profiling instrument. |
|log\_truncate\_on\_rotation |Boolean |off |Truncates \(overwrites\), rather than appends to, any existing log file of the same name. Truncation will occur only when a new file is being opened due to time-based rotation. For example, using this setting in combination with a log\_filename such as gpseg\#-%H.log would result in generating twenty-four hourly log files and then cyclically overwriting them. When off, pre-existing files will be appended to in all cases. |

**Parent topic:** [Greenplum Database Security Configuration Guide](../topics/preface.html)

