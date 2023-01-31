# About the Greenplum Database Utilities 

General information about using the Greenplum Database utility programs.

**Parent topic:** [Greenplum Database Utility Guide](utility_guide.html)

## <a id="ipv6"></a>Referencing IP Addresses 

When you reference IPv6 addresses in Greenplum Database utility programs, or when you use numeric IP addresses instead of hostnames in any management utility, always enclose the IP address in brackets. When specifying an IP address at the command line, the best practice is to escape any brackets or enclose them in single quotes. For example, use either:

```
\[2620:0:170:610::11\]
```

Or:

```
'[2620:0:170:610::11]'
```

## <a id="topic_zqp_5xm_cp"></a>Running Backend Server Programs 

Greenplum Database has modified certain PostgreSQL backend server programs to handle the parallelism and distribution of a Greenplum Database system. You access these programs only through the Greenplum Database management tools and utilities. *Do not run these programs directly.*

The following table identifies certain PostgreSQL backend server programs and the Greenplum Database utility command to run instead.

|PostgreSQL Program Name|Description|Use Instead|
|-----------------------|-----------|-----------|
|`initdb`|This program is called by `gpinitsystem` when initializing a Greenplum Database array. It is used internally to create the individual segment instances and the master instance.|[gpinitsystem](ref/gpinitsystem.html)|
|`ipcclean`|Not used in Greenplum Database|N/A|
|`pg_basebackup`|This program makes a binary copy of a single database instance. Greenplum Database uses it for tasks such as creating a standby master instance, or recovering a mirror segment when a full copy is needed. Do not use this utility to back up Greenplum Database segment instances because it does not produce MPP-consistent backups.|[gpinitstandby](ref/gpinitstandby.html), `[gprecoverseg](ref/gprecoverseg.html)`|
|`pg_controldata`|Not used in Greenplum Database|[gpstate](ref/gpstate.html)|
|`pg_ctl`|This program is called by `gpstart` and `gpstop` when starting or stopping a Greenplum Database array. It is used internally to stop and start the individual segment instances and the master instance in parallel and with the correct options.|[gpstart](ref/gpstart.html), [gpstop](ref/gpstop.html)|
|`pg_resetxlog`|DO NOT USE<br/><br/>**Warning:** This program might cause data loss or cause data to become unavailable. If this program is used, the VMware Greenplum cluster is not supported. Thecluster must be reinitialized and restoredby the customer.|N/A|
|`postgres`|The `postgres` executable is the actual PostgreSQL server process that processes queries.|The main `postgres` process \(postmaster\) creates other `postgres` subprocesses and `postgres` session as needed to handle client connections.|
|`postmaster`|`postmaster` starts the `postgres` database server listener process that accepts client connections. In Greenplum Database, a `postgres` database listener process runs on the Greenplum master Instance and on each Segment Instance.|In Greenplum Database, you use [gpstart](ref/gpstart.html) and [gpstop](ref/gpstop.html) to start all postmasters \(`postgres` processes\) in the system at once in the correct order and with the correct options.|

