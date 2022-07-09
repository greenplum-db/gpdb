# Auditing 

Describes Greenplum Database events that are logged and should be monitored to detect security threats.

Greenplum Database is capable of auditing a variety of events, including startup and shutdown of the system, segment database failures, SQL statements that result in an error, and all connection attempts and disconnections. Greenplum Database also logs SQL statements and information regarding SQL statements, and can be configured in a variety of ways to record audit information with more or less detail. The `log_error_verbosity` configuration parameter controls the amount of detail written in the server log for each message that is logged.  Similarly, the `log_min_error_statement` parameter allows administrators to configure the level of detail recorded specifically for SQL statements, and the `log_statement` parameter determines the kind of SQL statements that are audited. Greenplum Database records the username for all auditable events, when the event is initiated by a subject outside the Greenplum Database.

Greenplum Database prevents unauthorized modification and deletion of audit records by only allowing administrators with an appropriate role to perform any operations on log files.  Logs are stored in a proprietary format using comma-separated values \(CSV\).  Each segment and the master stores its own log files, although these can be accessed remotely by an administrator.  Greenplum Database also authorizes overwriting of old log files via the `log_truncate_on_rotation` parameter.  This is a local parameter and must be set on each segment and master configuration file.

Greenplum provides an administrative schema called `gp_toolkit` that you can use to query log files, as well as system catalogs and operating enviroment for system status information. For more information, including usage, refer to *The gp\_tookit Administrative Schema* appendix in the *Greenplum Database Reference Guide*.

## Viewing the Database Server Log Files 

Every database instance in Greenplum Database \(master and segments\) is a running PostgreSQL database server with its own server log file. Daily log files are created in the `pg_log` directory of the master and each segment data directory.

The server log files are written in comma-separated values \(CSV\) format. Not all log entries will have values for all of the log fields. For example, only log entries associated with a query worker process will have the `slice_id` populated. Related log entries of a particular query can be identified by its session identifier \(`gp_session_id`\) and command identifier \(`gp_command_count`\).

<div class="tablenoborder">
<table cellpadding="4" cellspacing="0" summary="" class="table" frame="border" border="1" rules="all"> 
          <thead class="thead" align="left">
            <tr class="row">
              <th class="entry" align="left" valign="top" width="11.76470588235294%" id="d26e83">
                <p class="p"> # </p>
              </th>
              <th class="entry" align="left" valign="top" width="23.52941176470588%" id="d26e89">
                <p class="p"> Field Name </p>
              </th>
              <th class="entry" align="left" valign="top" width="23.52941176470588%" id="d26e95">
                <p class="p"> Data Type </p>
              </th>
              <th class="entry" align="left" valign="top" width="41.17647058823529%" id="d26e101">
                <p class="p"> Description </p>
              </th>
            </tr>
          </thead>
          <tbody class="tbody">
            <tr class="row">
              <td class="entry" align="left" valign="top" width="11.76470588235294%" headers="d26e83 ">
                <p class="p"> 1 </p>
              </td>
              <td class="entry" align="left" valign="top" width="23.52941176470588%" headers="d26e89 ">
                <p class="p"> event_time </p>
              </td>
              <td class="entry" align="left" valign="top" width="23.52941176470588%" headers="d26e95 ">
                <p class="p"> timestamp with time zone </p>
              </td>
              <td class="entry" align="left" valign="top" width="41.17647058823529%" headers="d26e101 ">
                <p class="p"> Time that the log entry was written to the log </p>
              </td>
            </tr>
            <tr class="row">
              <td class="entry" align="left" valign="top" width="11.76470588235294%" headers="d26e83 ">
                <p class="p"> 2 </p>
              </td>
              <td class="entry" align="left" valign="top" width="23.52941176470588%" headers="d26e89 ">
                <p class="p"> user_name </p>
              </td>
              <td class="entry" align="left" valign="top" width="23.52941176470588%" headers="d26e95 ">
                <p class="p"> varchar(100) </p>
              </td>
              <td class="entry" align="left" valign="top" width="41.17647058823529%" headers="d26e101 ">
                <p class="p"> The database user name </p>
              </td>
            </tr>
            <tr class="row">
              <td class="entry" align="left" valign="top" width="11.76470588235294%" headers="d26e83 ">
                <p class="p"> 3 </p>
              </td>
              <td class="entry" align="left" valign="top" width="23.52941176470588%" headers="d26e89 ">
                <p class="p"> database_name </p>
              </td>
              <td class="entry" align="left" valign="top" width="23.52941176470588%" headers="d26e95 ">
                <p class="p"> varchar(100) </p>
              </td>
              <td class="entry" align="left" valign="top" width="41.17647058823529%" headers="d26e101 ">
                <p class="p"> The database name </p>
              </td>
            </tr>
            <tr class="row">
              <td class="entry" align="left" valign="top" width="11.76470588235294%" headers="d26e83 ">
                <p class="p"> 4 </p>
              </td>
              <td class="entry" align="left" valign="top" width="23.52941176470588%" headers="d26e89 ">
                <p class="p"> process_id </p>
              </td>
              <td class="entry" align="left" valign="top" width="23.52941176470588%" headers="d26e95 ">
                <p class="p"> varchar(10) </p>
              </td>
              <td class="entry" align="left" valign="top" width="41.17647058823529%" headers="d26e101 ">
                <p class="p"> The system process id (prefixed with "p") </p>
              </td>
            </tr>
            <tr class="row">
              <td class="entry" align="left" valign="top" width="11.76470588235294%" headers="d26e83 ">
                <p class="p"> 5 </p>
              </td>
              <td class="entry" align="left" valign="top" width="23.52941176470588%" headers="d26e89 ">
                <p class="p"> thread_id </p>
              </td>
              <td class="entry" align="left" valign="top" width="23.52941176470588%" headers="d26e95 ">
                <p class="p"> varchar(50) </p>
              </td>
              <td class="entry" align="left" valign="top" width="41.17647058823529%" headers="d26e101 ">
                <p class="p"> The thread count (prefixed with "th") </p>
              </td>
            </tr>
            <tr class="row">
              <td class="entry" align="left" valign="top" width="11.76470588235294%" headers="d26e83 ">
                <p class="p"> 6 </p>
              </td>
              <td class="entry" align="left" valign="top" width="23.52941176470588%" headers="d26e89 ">
                <p class="p"> remote_host </p>
              </td>
              <td class="entry" align="left" valign="top" width="23.52941176470588%" headers="d26e95 ">
                <p class="p"> varchar(100) </p>
              </td>
              <td class="entry" align="left" valign="top" width="41.17647058823529%" headers="d26e101 ">
                <p class="p"> On the master, the hostname/address of the client machine. On the segment, the
                  hostname/address of the master. </p>
              </td>
            </tr>
            <tr class="row">
              <td class="entry" align="left" valign="top" width="11.76470588235294%" headers="d26e83 ">
                <p class="p"> 7 </p>
              </td>
              <td class="entry" align="left" valign="top" width="23.52941176470588%" headers="d26e89 ">
                <p class="p"> remote_port </p>
              </td>
              <td class="entry" align="left" valign="top" width="23.52941176470588%" headers="d26e95 ">
                <p class="p"> varchar(10) </p>
              </td>
              <td class="entry" align="left" valign="top" width="41.17647058823529%" headers="d26e101 ">
                <p class="p"> The segment or master port number </p>
              </td>
            </tr>
            <tr class="row">
              <td class="entry" align="left" valign="top" width="11.76470588235294%" headers="d26e83 ">
                <p class="p"> 8 </p>
              </td>
              <td class="entry" align="left" valign="top" width="23.52941176470588%" headers="d26e89 ">
                <p class="p"> session_start_time </p>
              </td>
              <td class="entry" align="left" valign="top" width="23.52941176470588%" headers="d26e95 ">
                <p class="p"> timestamp with time zone </p>
              </td>
              <td class="entry" align="left" valign="top" width="41.17647058823529%" headers="d26e101 ">
                <p class="p"> Time session connection was opened </p>
              </td>
            </tr>
            <tr class="row">
              <td class="entry" align="left" valign="top" width="11.76470588235294%" headers="d26e83 ">
                <p class="p"> 9 </p>
              </td>
              <td class="entry" align="left" valign="top" width="23.52941176470588%" headers="d26e89 ">
                <p class="p"> transaction_id </p>
              </td>
              <td class="entry" align="left" valign="top" width="23.52941176470588%" headers="d26e95 ">
                <p class="p"> int </p>
              </td>
              <td class="entry" align="left" valign="top" width="41.17647058823529%" headers="d26e101 ">
                <p class="p"> Top-level transaction ID on the master. This ID is the parent of any
                  subtransactions. </p>
              </td>
            </tr>
            <tr class="row">
              <td class="entry" align="left" valign="top" width="11.76470588235294%" headers="d26e83 ">
                <p class="p"> 10 </p>
              </td>
              <td class="entry" align="left" valign="top" width="23.52941176470588%" headers="d26e89 ">
                <p class="p"> gp_session_id </p>
              </td>
              <td class="entry" align="left" valign="top" width="23.52941176470588%" headers="d26e95 ">
                <p class="p"> text </p>
              </td>
              <td class="entry" align="left" valign="top" width="41.17647058823529%" headers="d26e101 ">
                <p class="p"> Session identifier number (prefixed with "con") </p>
              </td>
            </tr>
            <tr class="row">
              <td class="entry" align="left" valign="top" width="11.76470588235294%" headers="d26e83 ">
                <p class="p"> 11 </p>
              </td>
              <td class="entry" align="left" valign="top" width="23.52941176470588%" headers="d26e89 ">
                <p class="p"> gp_command_count </p>
              </td>
              <td class="entry" align="left" valign="top" width="23.52941176470588%" headers="d26e95 ">
                <p class="p"> text </p>
              </td>
              <td class="entry" align="left" valign="top" width="41.17647058823529%" headers="d26e101 ">
                <p class="p"> The command number within a session (prefixed with "cmd") </p>
              </td>
            </tr>
            <tr class="row">
              <td class="entry" align="left" valign="top" width="11.76470588235294%" headers="d26e83 ">
                <p class="p"> 12 </p>
              </td>
              <td class="entry" align="left" valign="top" width="23.52941176470588%" headers="d26e89 ">
                <p class="p"> gp_segment </p>
              </td>
              <td class="entry" align="left" valign="top" width="23.52941176470588%" headers="d26e95 ">
                <p class="p"> text </p>
              </td>
              <td class="entry" align="left" valign="top" width="41.17647058823529%" headers="d26e101 ">
                <p class="p"> The segment content identifier (prefixed with "seg" for primaries or
                  "mir" for mirrors). The master always has a content id of -1. </p>
              </td>
            </tr>
            <tr class="row">
              <td class="entry" align="left" valign="top" width="11.76470588235294%" headers="d26e83 ">
                <p class="p"> 13 </p>
              </td>
              <td class="entry" align="left" valign="top" width="23.52941176470588%" headers="d26e89 ">
                <p class="p"> slice_id </p>
              </td>
              <td class="entry" align="left" valign="top" width="23.52941176470588%" headers="d26e95 ">
                <p class="p"> text </p>
              </td>
              <td class="entry" align="left" valign="top" width="41.17647058823529%" headers="d26e101 ">
                <p class="p"> The slice id (portion of the query plan being executed) </p>
              </td>
            </tr>
            <tr class="row">
              <td class="entry" align="left" valign="top" width="11.76470588235294%" headers="d26e83 ">
                <p class="p"> 14 </p>
              </td>
              <td class="entry" align="left" valign="top" width="23.52941176470588%" headers="d26e89 ">
                <p class="p"> distr_tranx_id </p>
              </td>
              <td class="entry" align="left" valign="top" width="23.52941176470588%" headers="d26e95 ">
                <p class="p"> text </p>
              </td>
              <td class="entry" align="left" valign="top" width="41.17647058823529%" headers="d26e101 ">
                <p class="p"> Distributed transaction ID </p>
              </td>
            </tr>
            <tr class="row">
              <td class="entry" align="left" valign="top" width="11.76470588235294%" headers="d26e83 ">
                <p class="p"> 15 </p>
              </td>
              <td class="entry" align="left" valign="top" width="23.52941176470588%" headers="d26e89 ">
                <p class="p"> local_tranx_id </p>
              </td>
              <td class="entry" align="left" valign="top" width="23.52941176470588%" headers="d26e95 ">
                <p class="p"> text </p>
              </td>
              <td class="entry" align="left" valign="top" width="41.17647058823529%" headers="d26e101 ">
                <p class="p"> Local transaction ID </p>
              </td>
            </tr>
            <tr class="row">
              <td class="entry" align="left" valign="top" width="11.76470588235294%" headers="d26e83 ">
                <p class="p"> 16 </p>
              </td>
              <td class="entry" align="left" valign="top" width="23.52941176470588%" headers="d26e89 ">
                <p class="p"> sub_tranx_id </p>
              </td>
              <td class="entry" align="left" valign="top" width="23.52941176470588%" headers="d26e95 ">
                <p class="p"> text </p>
              </td>
              <td class="entry" align="left" valign="top" width="41.17647058823529%" headers="d26e101 ">
                <p class="p"> Subtransaction ID </p>
              </td>
            </tr>
            <tr class="row">
              <td class="entry" align="left" valign="top" width="11.76470588235294%" headers="d26e83 ">
                <p class="p"> 17 </p>
              </td>
              <td class="entry" align="left" valign="top" width="23.52941176470588%" headers="d26e89 ">
                <p class="p"> event_severity </p>
              </td>
              <td class="entry" align="left" valign="top" width="23.52941176470588%" headers="d26e95 ">
                <p class="p"> varchar(10) </p>
              </td>
              <td class="entry" align="left" valign="top" width="41.17647058823529%" headers="d26e101 ">
                <p class="p"> Values include: LOG, ERROR, FATAL, PANIC, DEBUG1, DEBUG2 </p>
              </td>
            </tr>
            <tr class="row">
              <td class="entry" align="left" valign="top" width="11.76470588235294%" headers="d26e83 ">
                <p class="p"> 18 </p>
              </td>
              <td class="entry" align="left" valign="top" width="23.52941176470588%" headers="d26e89 ">
                <p class="p"> sql_state_code </p>
              </td>
              <td class="entry" align="left" valign="top" width="23.52941176470588%" headers="d26e95 ">
                <p class="p"> varchar(10) </p>
              </td>
              <td class="entry" align="left" valign="top" width="41.17647058823529%" headers="d26e101 ">
                <p class="p"> SQL state code associated with the log message </p>
              </td>
            </tr>
            <tr class="row">
              <td class="entry" align="left" valign="top" width="11.76470588235294%" headers="d26e83 ">
                <p class="p"> 19 </p>
              </td>
              <td class="entry" align="left" valign="top" width="23.52941176470588%" headers="d26e89 ">
                <p class="p"> event_message </p>
              </td>
              <td class="entry" align="left" valign="top" width="23.52941176470588%" headers="d26e95 ">
                <p class="p"> text </p>
              </td>
              <td class="entry" align="left" valign="top" width="41.17647058823529%" headers="d26e101 ">
                <p class="p"> Log or error message text </p>
              </td>
            </tr>
            <tr class="row">
              <td class="entry" align="left" valign="top" width="11.76470588235294%" headers="d26e83 ">
                <p class="p"> 20 </p>
              </td>
              <td class="entry" align="left" valign="top" width="23.52941176470588%" headers="d26e89 ">
                <p class="p"> event_detail </p>
              </td>
              <td class="entry" align="left" valign="top" width="23.52941176470588%" headers="d26e95 ">
                <p class="p"> text </p>
              </td>
              <td class="entry" align="left" valign="top" width="41.17647058823529%" headers="d26e101 ">
                <p class="p"> Detail message text associated with an error or warning message </p>
              </td>
            </tr>
            <tr class="row">
              <td class="entry" align="left" valign="top" width="11.76470588235294%" headers="d26e83 ">
                <p class="p"> 21 </p>
              </td>
              <td class="entry" align="left" valign="top" width="23.52941176470588%" headers="d26e89 ">
                <p class="p"> event_hint </p>
              </td>
              <td class="entry" align="left" valign="top" width="23.52941176470588%" headers="d26e95 ">
                <p class="p"> text </p>
              </td>
              <td class="entry" align="left" valign="top" width="41.17647058823529%" headers="d26e101 ">
                <p class="p"> Hint message text associated with an error or warning message </p>
              </td>
            </tr>
            <tr class="row">
              <td class="entry" align="left" valign="top" width="11.76470588235294%" headers="d26e83 ">
                <p class="p"> 22 </p>
              </td>
              <td class="entry" align="left" valign="top" width="23.52941176470588%" headers="d26e89 ">
                <p class="p"> internal_query </p>
              </td>
              <td class="entry" align="left" valign="top" width="23.52941176470588%" headers="d26e95 ">
                <p class="p"> text </p>
              </td>
              <td class="entry" align="left" valign="top" width="41.17647058823529%" headers="d26e101 ">
                <p class="p"> The internally-generated query text </p>
              </td>
            </tr>
            <tr class="row">
              <td class="entry" align="left" valign="top" width="11.76470588235294%" headers="d26e83 ">
                <p class="p"> 23 </p>
              </td>
              <td class="entry" align="left" valign="top" width="23.52941176470588%" headers="d26e89 ">
                <p class="p"> internal_query_pos </p>
              </td>
              <td class="entry" align="left" valign="top" width="23.52941176470588%" headers="d26e95 ">
                <p class="p"> int </p>
              </td>
              <td class="entry" align="left" valign="top" width="41.17647058823529%" headers="d26e101 ">
                <p class="p"> The cursor index into the internally-generated query text </p>
              </td>
            </tr>
            <tr class="row">
              <td class="entry" align="left" valign="top" width="11.76470588235294%" headers="d26e83 ">
                <p class="p"> 24 </p>
              </td>
              <td class="entry" align="left" valign="top" width="23.52941176470588%" headers="d26e89 ">
                <p class="p"> event_context </p>
              </td>
              <td class="entry" align="left" valign="top" width="23.52941176470588%" headers="d26e95 ">
                <p class="p"> text </p>
              </td>
              <td class="entry" align="left" valign="top" width="41.17647058823529%" headers="d26e101 ">
                <p class="p"> The context in which this message gets generated </p>
              </td>
            </tr>
            <tr class="row">
              <td class="entry" align="left" valign="top" width="11.76470588235294%" headers="d26e83 ">
                <p class="p"> 25 </p>
              </td>
              <td class="entry" align="left" valign="top" width="23.52941176470588%" headers="d26e89 ">
                <p class="p"> debug_query_string </p>
              </td>
              <td class="entry" align="left" valign="top" width="23.52941176470588%" headers="d26e95 ">
                <p class="p"> text </p>
              </td>
              <td class="entry" align="left" valign="top" width="41.17647058823529%" headers="d26e101 ">
                <p class="p"> User-supplied query string with full detail for debugging. This string can be
                  modified for internal use. </p>
              </td>
            </tr>
            <tr class="row">
              <td class="entry" align="left" valign="top" width="11.76470588235294%" headers="d26e83 ">
                <p class="p"> 26 </p>
              </td>
              <td class="entry" align="left" valign="top" width="23.52941176470588%" headers="d26e89 ">
                <p class="p"> error_cursor_pos </p>
              </td>
              <td class="entry" align="left" valign="top" width="23.52941176470588%" headers="d26e95 ">
                <p class="p"> int </p>
              </td>
              <td class="entry" align="left" valign="top" width="41.17647058823529%" headers="d26e101 ">
                <p class="p"> The cursor index into the query string </p>
              </td>
            </tr>
            <tr class="row">
              <td class="entry" align="left" valign="top" width="11.76470588235294%" headers="d26e83 ">
                <p class="p"> 27 </p>
              </td>
              <td class="entry" align="left" valign="top" width="23.52941176470588%" headers="d26e89 ">
                <p class="p"> func_name </p>
              </td>
              <td class="entry" align="left" valign="top" width="23.52941176470588%" headers="d26e95 ">
                <p class="p"> text </p>
              </td>
              <td class="entry" align="left" valign="top" width="41.17647058823529%" headers="d26e101 ">
                <p class="p"> The function in which this message is generated </p>
              </td>
            </tr>
            <tr class="row">
              <td class="entry" align="left" valign="top" width="11.76470588235294%" headers="d26e83 ">
                <p class="p"> 28 </p>
              </td>
              <td class="entry" align="left" valign="top" width="23.52941176470588%" headers="d26e89 ">
                <p class="p"> file_name </p>
              </td>
              <td class="entry" align="left" valign="top" width="23.52941176470588%" headers="d26e95 ">
                <p class="p"> text </p>
              </td>
              <td class="entry" align="left" valign="top" width="41.17647058823529%" headers="d26e101 ">
                <p class="p"> The internal code file where the message originated </p>
              </td>
            </tr>
            <tr class="row">
              <td class="entry" align="left" valign="top" width="11.76470588235294%" headers="d26e83 ">
                <p class="p"> 29 </p>
              </td>
              <td class="entry" align="left" valign="top" width="23.52941176470588%" headers="d26e89 ">
                <p class="p"> file_line </p>
              </td>
              <td class="entry" align="left" valign="top" width="23.52941176470588%" headers="d26e95 ">
                <p class="p"> int </p>
              </td>
              <td class="entry" align="left" valign="top" width="41.17647058823529%" headers="d26e101 ">
                <p class="p"> The line of the code file where the message originated </p>
              </td>
            </tr>
            <tr class="row">
              <td class="entry" align="left" valign="top" width="11.76470588235294%" headers="d26e83 ">
                <p class="p"> 30 </p>
              </td>
              <td class="entry" align="left" valign="top" width="23.52941176470588%" headers="d26e89 ">
                <p class="p"> stack_trace </p>
              </td>
              <td class="entry" align="left" valign="top" width="23.52941176470588%" headers="d26e95 ">
                <p class="p"> text </p>
              </td>
              <td class="entry" align="left" valign="top" width="41.17647058823529%" headers="d26e101 ">
                <p class="p"> Stack trace text associated with this message </p>
              </td>
            </tr>
          </tbody>
        </table>
</div>
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

|Field Name<br/>|Value Range<br/>|Default<br/>|Description<br/>|
|:-----------|:------------|:--------|:------------|
|log\_connections<br/>|Boolean<br/>|off<br/>|This outputs a line to the server log detailing each successful connection. Some client programs, like psql, attempt to connect twice while determining if a password is required, so duplicate “connection received” messages do not always indicate a problem.|
|log\_disconnections<br/>|Boolean<br/>|off<br/>|This outputs a line in the server log at termination of a client session, and includes the duration of the session.|
|log\_statement|NONE<br/>DDL<br/>MOD<br/>ALL<br/>|ALL<br/>|Controls which SQL statements are logged. DDL logs all data definition commands like CREATE, ALTER, and DROP commands. MOD logs all DDL statements, plus INSERT, UPDATE, DELETE, TRUNCATE, and COPY FROM. PREPARE and EXPLAIN ANALYZE statements are also logged if their contained command is of an appropriate type.|
|log\_hostname<br/>|Boolean<br/>|off<br/>|By default, connection log messages only show the IP address of the connecting host. Turning on this option causes logging of the host name as well. Note that depending on your host name resolution setup this might impose a non-negligible performance penalty.|
|log\_duration<br/>|Boolean<br/>|off<br/>|Causes the duration of every completed statement which satisfies log\_statement to be logged.|
|log\_error\_verbosity<br/>|TERSE<br/>DEFAULT<br/>VERBOSE<br/>|DEFAULT|Controls the amount of detail written in the server log for each message that is logged.|
|log\_min\_duration\_statement<br/>|number of milliseconds, 0, -1<br/>|-1<br/>|Logs the statement and its duration on a single log line if its duration is greater than or equal to the specified number of milliseconds. Setting this to 0 will print all statements and their durations. -1 disables the feature. For example, if you set it to 250 then all SQL statements that run 250ms or longer will be logged. Enabling this option can be useful in tracking down unoptimized queries in your applications.|
|log\_min\_messages<br/>|DEBUG5<br/>DEBUG4<br/>DEBUG3<br/>DEBUG2<br/>DEBUG1<br/>INFO<br/>NOTICE<br/>WARNING<br/>ERROR<br/>LOG<br/>FATAL<br/>PANIC<br/>|NOTICE<br/>|Controls which message levels are written to the server log. Each level includes all the levels that follow it. The later the level, the fewer messages are sent to the log.|
|log\_rotation\_age<br/>|Any valid time expression \(number and unit\)<br/>|1d|Determines the maximum lifetime of an individual log file. After this time has elapsed, a new log file will be created. Set to zero to disable time-based creation of new log files.|
|log\_statement\_stats<br/>|Boolean<br/>|off|For each query, write total performance statistics of the query parser, planner, and executor to the server log. This is a crude profiling instrument.|
|log\_truncate\_on\_rotation|Boolean<br/>|off|Truncates \(overwrites\), rather than appends to, any existing log file of the same name. Truncation will occur only when a new file is being opened due to time-based rotation. For example, using this setting in combination with a log\_filename such as gpseg\#-%H.log would result in generating twenty-four hourly log files and then cyclically overwriting them. When off, pre-existing files will be appended to in all cases.|

**Parent topic:** [Greenplum Database Security Configuration Guide](../topics/preface.html)

