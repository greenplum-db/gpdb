---
title: Starting and Stopping Greenplum Database 
---

In a Greenplum Database DBMS, the database server instances \(the coordinator and all segments\) are started or stopped across all of the hosts in the system in such a way that they can work together as a unified DBMS.

Because a Greenplum Database system is distributed across many machines, the process for starting and stopping a Greenplum Database system is different than the process for starting and stopping a regular PostgreSQL DBMS.

Use the `gpstart` and `gpstop` utilities to start and stop Greenplum Database, respectively. These utilities are located in the $GPHOME/bin directory on your Greenplum Database coordinator host.

> **Important** Do not issue a `kill` command to end any Postgres process. Instead, use the database command `pg_cancel_backend()`.

Issuing a `kill -9` or `kill -11` can introduce database corruption and prevent root cause analysis from being performed.

For information about `gpstart` and `gpstop`, see the *Greenplum Database Utility Guide*.

**Parent topic:** [Managing a Greenplum System](../managing/partII.html)

## <a id="task_hkd_gzv_fp"></a>Starting Greenplum Database 

Start an initialized Greenplum Database system by running the `gpstart` utility on the coordinator instance.

Use the `gpstart` utility to start a Greenplum Database system that has already been initialized by the `gpinitsystem` utility, but has been stopped by the `gpstop` utility. The `gpstart` utility starts Greenplum Database by starting all the Postgres database instances on the Greenplum Database cluster. `gpstart` orchestrates this process and performs the process in parallel.

Run `gpstart` on the coordinator host to start Greenplum Database:
```
$ gpstart
```


## <a id="task_gpdb_restart"></a>Restarting Greenplum Database 

Stop the Greenplum Database system and then restart it.

The `gpstop` utility with the `-r` option can stop and then restart Greenplum Database after the shutdown completes.

To restart Greenplum Database, enter the following command on the coordinator host:
```
$ gpstop -r
```


## <a id="task_upload_config"></a>Reloading Configuration File Changes Only 

Reload changes to Greenplum Database configuration files without interrupting the system.

The `gpstop` utility can reload changes to the pg\_hba.conf configuration file and to *runtime* parameters in the coordinator postgresql.conf file without service interruption. Active sessions pick up changes when they reconnect to the database. Many server configuration parameters require a full system restart \(`gpstop -r`\) to activate. For information about server configuration parameters, see the *Greenplum Database Reference Guide*.

Reload configuration file changes without shutting down the Greenplum Database system using the `gpstop` utility:
```
$ gpstop -u
```


## <a id="task_maint_mode"></a>Starting the Coordinator in Maintenance Mode 

Start only the coordinator to perform maintenance or administrative tasks without affecting data on the segments.

Maintenance mode should only be used with direction from VMware Technical Support. For example, you could connect to a database only on the coordinator instance in maintenance mode and edit system catalog settings. For more information about system catalog tables, see the *Greenplum Database Reference Guide*.

1.  Run `gpstart` using the -m option:

    ```
    $ gpstart -m
    ```

2.  Connect to the coordinator in maintenance mode to do catalog maintenance. For example:

     <a id="kg155401"></a>
     ``` 
     $ PGOPTIONS='-c gp_role=utility' psql postgres
     ```

3.  After completing your administrative tasks, stop the coordinator in maintenance mode. Then, restart it in production mode.

    ```
    $ gpstop -m
    $ gpstart
    ```

    > **Caution** Incorrect use of maintenance mode connections can result in an inconsistent system state. Only Technical Support should perform this operation.


## <a id="task_gpdb_stop"></a>Stopping Greenplum Database 

The `gpstop` utility stops or restarts your Greenplum Database system and always runs on the coordinator host. When activated, `gpstop` stops all `postgres` processes in the system, including the coordinator and all segment instances. The `gpstop` utility uses a default of up to 64 parallel worker threads to bring down the Postgres instances that make up the Greenplum Database cluster. The system waits for any active transactions to finish before shutting down. If after two minutes there are still active connections, `gpstop` will prompt you to either continue waiting in smart mode, stop in fast mode, or stop in immediate mode. To stop Greenplum Database immediately, use fast mode.

> **Important** Immediate shut down mode is not recommended. This mode stops all database processes without allowing the database server to complete transaction processing or clean up any temporary or in-process work files.

-   To stop Greenplum Database:

    ```
    $ gpstop
    ```

-   To stop Greenplum Database in fast mode:

    ```
    $ gpstop -M fast
    ```

    By default, you are not allowed to shut down Greenplum Database if there are any client connections to the database. Use the `-M fast` option to roll back all in progress transactions and terminate any connections before shutting down.


## <a id="topic13"></a>Stopping Client Processes 

Greenplum Database launches a new backend process for each client connection. A Greenplum Database user with `SUPERUSER` privileges can cancel and terminate these client backend processes.

Canceling a backend process with the `pg_cancel_backend()` function ends a specific queued or active client query. Terminating a backend process with the `pg_terminate_backend()` function terminates a client connection to a database.

The `pg_cancel_backend()` function has two signatures:

-   `pg_cancel_backend( pid int4 )`
-   `pg_cancel_backend( pid int4, msg text )`

The `pg_terminate_backend()` function has two similar signatures:

-   `pg_terminate_backend( pid int4 )`
-   `pg_terminate_backend( pid int4, msg text )`

If you provide a `msg`, Greenplum Database includes the text in the cancel message returned to the client. `msg` is limited to 128 bytes; Greenplum Database truncates anything longer.

The `pg_cancel_backend()` and `pg_terminate_backend()` functions return `true` if successful, and `false` otherwise.

To cancel or terminate a backend process, you must first identify the process ID of the backend. You can obtain the process ID from the `pid` column of the `pg_stat_activity` view. For example, to view the process information associated with all running and queued queries:

```
=# SELECT usename, pid, waiting, state, query, datname
     FROM pg_stat_activity;
```

Sample partial query output:

```
 usename |  pid     | waiting | state  |         query          | datname
---------+----------+---------+--------+------------------------+---------
  sammy  |   31861  |    f    | idle   | SELECT * FROM testtbl; | testdb
  billy  |   31905  |    t    | active | SELECT * FROM topten;  | testdb
```

Use the output to identify the process id \(`pid`\) of the query or client connection.

For example, to cancel the waiting query identified in the sample output above and include `'Admin canceled long-running query.'` as the message returned to the client:

```
=# SELECT pg_cancel_backend(31905 ,'Admin canceled long-running query.');
ERROR:  canceling statement due to user request: "Admin canceled long-running query."
```

