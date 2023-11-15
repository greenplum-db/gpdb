---
title: pgbouncer-admin 
---

PgBouncer Administration Console.

## <a id="syn"></a>Synopsis 

```
psql -p <port> pgbouncer
```

## <a id="desc"></a>Description 

The PgBouncer Administration Console is available via `psql`. Connect to the PgBouncer `<port>` and the virtual database named `pgbouncer` to log in to the console.

Users listed in the `pgbouncer.ini` configuration parameters `admin_users` and `stats_users` have privileges to log in to the PgBouncer Administration Console. When `auth_type=any`, then any user is allowed in as a stats_user.

Additionally, the user name `pgbouncer` is allowed to log in without password when the login comes via the Unix socket and the client has same Unix user UID as the running process.

You can control connections between PgBouncer and Greenplum Database from the console. You can also set PgBouncer configuration parameters.

## <a id="opt"></a>Options 

-p \<port\>
:   The PgBouncer port number.

## <a id="topic_bk3_3jc_dt"></a>Command Syntax 

```
pgbouncer=# SHOW help;
NOTICE:  Console usage
DETAIL:  
    SHOW HELP|CONFIG|USERS|DATABASES|POOLS|CLIENTS|SERVERS|VERSION
    SHOW FDS|SOCKETS|ACTIVE_SOCKETS|LISTS|MEM
    SHOW DNS_HOSTS|DNS_ZONES
    SHOW STATS|STATS_TOTALS|STATS_AVERAGES
    SHOW TOTALS
    SET key = arg
    RELOAD
    PAUSE [<db>]
    RESUME [<db>]
    DISABLE <db>
    ENABLE <db>
    RECONNECT [<db>]
    KILL <db>
    SUSPEND
    SHUTDOWN
```

## <a id="topic_bf5_jcl_gs"></a>Administration Commands 

The following PgBouncer administration commands control the running `pgbouncer` process.

PAUSE \[\<db\>\]
:   If no database is specified, PgBouncer tries to disconnect from all servers, first waiting for all queries to complete. The command will not return before all queries are finished. This command is to be used to prepare to restart the database.

:   If a database name is specified, PgBouncer pauses only that database.

:   New client connections to a paused database will wait until a `RESUME` command is invoked.

DISABLE \<db\>
:   Reject all new client connections on the database.

ENABLE \<db\>
:   Allow new client connections after a previous `DISABLE` command.

RECONNECT
:   Close each open server connection for the given database, or all databases, after it is released \(according to the pooling mode\), even if its lifetime is not up yet. New server connections can be made immediately and will connect as necessary according to the pool size settings.

:   This command is useful when the server connection setup has changed, for example to perform a gradual switchover to a new server. It is not necessary to run this command when the connection string in `pgbouncer.ini` has been changed and reloaded (see `RELOAD`) or when DNS resolution has changed, because then the equivalent of this command will be run automatically. This command is only necessary if something downstream of PgBouncer routes the connections.

:   After this command is run, there could be an extended period where some server connections go to an old destination and some server connections go to a new destination. This is likely only sensible when switching read-only traffic between read-only replicas, or when switching between nodes of a multimaster replication setup. If all connections need to be switched at the same time, `PAUSE` is recommended instead. To close server connections without waiting (for example, in emergency failover rather than gradual switchover scenarios), also consider `KILL`.

KILL \<db\>
:   Immediately drop all client and server connections to the named database.

:   New client connections to a killed database will wait until `RESUME` is called.

SUSPEND
:   All socket buffers are flushed and PgBouncer stops listening for data on them. The command will not return before all buffers are empty. To be used when rebooting PgBouncer online.

:   New client connections to a suspended database will wait until `RESUME` is called.

RESUME \[\<db\>\]
:   Resume work from a previous `KILL`, `PAUSE`, or `SUSPEND` command.

SHUTDOWN
:   The PgBouncer process will exit. To exit from the `psql` command line session, enter `\q`.

RELOAD
:   The PgBouncer process reloads the current configuration file and updates the changeable settings.

:   PgBouncer notices when a configuration file reload changes the connection parameters of a database definition. An existing server connection to the old destination will be closed when the server connection is next released (according to the pooling mode), and new server connections will immediately use the updated connection parameters

WAIT\_CLOSE \[\<db\>\]
:   Wait until all server connections, either of the specified database or of all databases, have cleared the “close\_needed” state \(see `SHOW SERVERS`\). This can be called after a `RECONNECT` or `RELOAD` to wait until the respective configuration change has been fully activated, for example in switchover scripts.

SET key = value
:   Changes the specified configuration setting. See the [`SHOW CONFIG;`](#CONFIG) command.

:   (Note that this command is run on the PgBouncer admin console and sets PgBouncer settings. A `SET` command run on another database will be passed to the PostgreSQL backend like any other SQL command.)

## <a id="topic_zfh_2dl_gs"></a>SHOW Command 

The `SHOW <category>` command displays different types of PgBouncer information. You can specify one of the following categories:

-   [CLIENTS](#CLIENTS)
-   [CONFIG](#CONFIG)
-   [DATABASES](#DATABASES)
-   [DNS\_HOSTS](#DNS_HOSTS)
-   [DNS\_ZONES](#DNS_ZONES)
-   [FDS](#FDS)
-   [LISTS](#LISTS)
-   [MEM](#MEM)
-   [POOLS](#POOLS)
-   [SERVERS](#SERVERS)
-   [SOCKETS, ACTIVE\_SOCKETS](#SOCKETS), 
-   [STATS](#STATS)
-   [STATS\_TOTALS](#STATS_TOTALS)
-   [STATS\_AVERAGES](#STATS_AVERAGES)
-   [TOTALS](#TOTALS)
-   [USERS](#USERS)
-   [VERSION](#VERSION)

### <a id="CLIENTS"></a>CLIENTS 

|Column|Description|
|------|-----------|
|type|C, for client.|
|user|Client connected user.|
|database|Database name.|
|state|State of the client connection, one of `active` or `waiting`.|
|addr|IP address of client.|
|port|Port client is connected to.|
|local\_addr|Connection end address on local machine.|
|local\_port|Connection end port on local machine.|
|connect\_time|Timestamp of connect time.|
|request\_time|Timestamp of latest client request.|
|wait|Current Time waiting in seconds.|
|wait\_us|Microsecond part of the current waiting time.|
|ptr|Address of internal object for this connection. Used as unique ID.|
|link|Address of server connection the client is paired with.|
|remote\_pid|Process ID, if client connects with Unix socket and the OS supports getting it.|
|tls|A string with TLS connection information, or empty if not using TLS.|

### <a id="CONFIG"></a>CONFIG 

Show the current PgBouncer configuration settings, one per row, with the following columns:

|Column|Description|
|------|-----------|
|key|Configuration variable name|
|value|Configuration value|
|default|Configuration default value|
|changeable|Either `yes` or `no`. Shows whether the variable can be changed while running. If `no`, the variable can be changed only at boot time. Use `SET` to change a variable at run time.|

### <a id="DATABASES"></a>DATABASES 

|Column|Description|
|------|-----------|
|name|Name of configured database entry.|
|host|Host pgbouncer connects to.|
|port|Port pgbouncer connects to.|
|database|Actual database name pgbouncer connects to.|
|force\_user|When user is part of the connection string, the connection between pgbouncer and the database server is forced to the given user, whatever the client user.|
|pool\_size|Maximum number of server connections.|
|min\_pool\_size|Minimum number of server connections.|
|reserve\_pool|The maximum number of additional connections for this database.|
|pool\_mode|The database's override `pool_mode` or NULL if the default will be used instead.|
|max\_connections|Maximum number of allowed connections for this database, as set by `max_db_connections`, either globally or per-database.|
|current\_connections|The current number of connections for this database.|
|paused|Paused/unpaused state of the database. 1 if this database is currently paused, else 0.|
|disabled|Enabled/disabled state of the database. 1 if this database is currently disabled, else 0.|

### <a id="DNS_HOSTS"></a>DNS\_HOSTS 

Show host names in DNS cache.

|Column|Description|
|------|-----------|
|hostname|Host name|
|ttl|How many seconds until next lookup.|
|addrs|Comma-separated list of addresses.|

### <a id="DNS_ZONES"></a>DNS\_ZONES 

Show DNS zones in cache.

|Column|Description|
|------|-----------|
|zonename|Zone name|
|serial|Current DNS serial number|
|count|Hostnames belonging to this zone|

### <a id="FDS"></a>FDS 

`SHOW FDS` is an internal command used for an online restart, for example when upgrading to a new PgBouncer version. It displays a list of file descriptors in use with the internal state attached to them. This command blocks the internal event loop, so it should not be used while PgBouncer is in use.

When the connected user has username "pgbouncer", connects through a Unix socket, and has the same UID as the running process, the actual file descriptors are passed over the connection. This mechanism is used to do an online restart.

|Column|Description|
|------|-----------|
|fd|File descriptor numeric value.|
|task|One of `pooler`, `client`, or `server`.|
|user|User of the connection using the file descriptor.|
|database|Database of the connection using the file descriptor.|
|addr|IP address of the connection using the file descriptor, `unix` if a Unix socket is used.|
|port|Port used by the connection using the file descriptor.|
|cancel|Cancel key for this connection.|
|link|File descriptor for corresponding server/client. NULL if idle.|

### <a id="LISTS"></a>LISTS 

Shows the following PgBouncer internal information, in columns (not rows):

|Item|Description|
|----|-----------|
|databases|Count of databases.|
|users|Count of users.|
|pools|Count of pools.|
|free\_clients|Count of free clients.|
|used\_clients|Count of used clients.|
|login\_clients|Count of clients in `login` state.|
|free\_servers|Count of free servers.|
|used\_servers|Count of used servers.|
|dns\_names|Count of DNS names in the cache.|
|dns\_zones|Count of DNS zones in the cache.|
|dns\_queries|Count of in-flight DNS queries.|
|dns\_pending| not used |

### <a id="MEM"></a>MEM 

Shows low-level information about the current sizes of various internal memory allocations. The information presented is subject to change.

### <a id="POOLS"></a>POOLS 

A new pool entry is made for each pair of \(database, user\).

|Column|Description|
|------|-----------|
|database|Database name.|
|user|User name.|
|cl\_active|Client connections that are linked to server connection and can process queries.|
|cl\_waiting|Client connections that have sent queries but have not yet got a server connection.|
|cl\_cancel_req|Client connections that have not yet forwarded query cancellations to the server.|
|sv\_active|Server connections that are linked to client.|
|sv\_idle|Server connections that are unused and immediately usable for client queries.|
|sv\_used|Server connections that have been idle more than `server_check_delay`. The `server_check_query` query must be run on them before they can be used again.|
|sv\_tested|Server connections that are currently running either `server_reset_query` or `server_check_query`.|
|sv\_login|Server connections currently in the process of logging in.|
|maxwait|How long the first \(oldest\) client in the queue has waited, in seconds. If this begins to increase, the current pool of servers does not handle requests quickly enough. The cause may be either an overloaded server or the `pool_size` setting is too small.|
|maxwait\_us|Microsecond part of the maximum waiting time.|
|pool\_mode|The pooling mode in use.|

### <a id="SERVERS"></a>SERVERS 

|Column|Description|
|------|-----------|
|type|S, for server.|
|user|User name that `pgbouncer` uses to connect to server.|
|database|Database name.|
|state|State of the `pgbouncer` server connection, one of `active`, `idle`, `used`, `tested`, or `new`.|
|addr|IP address of the Greenplum or PostgreSQL server.|
|port|Port of the Greenplum or PostgreSQL server.|
|local\_addr|Connection start address on local machine.|
|local\_port|Connection start port on local machine.|
|connect\_time|When the connection was made.|
|request\_time|When the last request was issued.|
|wait|Current waiting time in seconds.|
|wait\_us|Microsecond part of the current waiting time.|
|close\_needed|1 if the connection will be closed as soon as possible, because a configuration file reload or DNS update changed the connection information or `RECONNECT` was issued.|
|ptr|Address of the internal object for this connection. Used as unique ID.|
|link|Address of the client connection the server is paired with.|
|remote\_pid|Pid of backend server process. If the connection is made over Unix socket and the OS supports getting process ID info, it is the OS pid. Otherwise it is extracted from the cancel packet the server sent, which should be PID in case server is PostgreSQL, but it is a random number in case server is another PgBouncer.|
|tls|A string with TLS connection information, or empty if not using TLS.|

### <a id="SOCKETS"></a>SOCKETS, ACTIVE\_SOCKETS

Shows low-level information about sockets or only active sockets. This includes the information shown under `SHOW CLIENTS` and `SHOW SERVERS` as well as other more low-level information.

### <a id="STATS"></a>STATS 

Shows statistics. In this and related commands, the total figures are since process start, the averages are updated every `stats_period`.

|Column|Description|
|------|-----------|
|database|Statistics are presented per database.|
|total\_xact\_count|Total number of SQL transactions pooled by PgBouncer.|
|total\_query\_count|Total number of SQL queries pooled by PgBouncer.|
|total\_received|Total volume in bytes of network traffic received by `pgbouncer`.|
|total\_sent|Total volume in bytes of network traffic sent by `pgbouncer`.|
|total\_xact\_time|Total number of microseconds spent by PgBouncer when connected to Greenplum Database in a transaction, either idle in transaction or executing queries.|
|total\_query\_time|Total number of microseconds spent by `pgbouncer` when actively connected to the database server.|
|total\_wait\_time|Time spent \(in microseconds\) by clients waiting for a server.|
|avg\_xact\_count|Average number of transactions per second in the last stat period.|
|avg\_query\_count|Average queries per second in the last stats period.|
|avg\_recv|Average received \(from clients\) bytes per second.|
|avg\_sent|Average sent \(to clients\) bytes per second.|
|avg\_xact\_time|Average transaction duration in microseconds.|
|avg\_query\_time|Average query duration in microseconds.|
|avg\_wait\_time|Time spent by clients waiting for a server in microseconds \(average per second\).|

### <a id="STATS_AVERAGES"></a>STATS\_AVERAGES 

Subset of `SHOW STATS` showing the average values for selected statistics (`avg_`)

### <a id="STATS_TOTALS"></a>STATS\_TOTALS 

Subset of `SHOW STATS` showing the total values for selected statistics (`total_`)

### <a id="TOTALS"></a>TOTALS

Like `SHOW STATS` but aggregated across all databases.

### <a id="USERS"></a>USERS 

|Column|Description|
|------|-----------|
|name|The user name|
|pool\_mode|The user's override pool\_mode, or NULL if the default will be used instead.|

### <a id="VERSION"></a>VERSION 

Display PgBouncer version information.

> **Note** This reference documentation is based on the PgBouncer 1.16 documentation.

## <a id="signals"></a>Signals

SIGHUP : Reload config. Same as issuing the command `RELOAD` on the console.

SIGINT : Safe shutdown. Same as issuing `PAUSE` and `SHUTDOWN` on the console.

SIGTERM : Immediate shutdown. Same as issuing `SHUTDOWN` on the console.

SIGUSR1 : Same as issuing `PAUSE` on the console.

SIGUSR2 : Same as issuing `RESUME` on the console.

## <a id="libevent"></a>Libevent Settings

From the Libevent documentation:

```
It is possible to disable support for epoll, kqueue, devpoll, poll or select by
setting the environment variable EVENT_NOEPOLL, EVENT_NOKQUEUE, EVENT_NODEVPOLL,
EVENT_NOPOLL or EVENT_NOSELECT, respectively.

By setting the environment variable EVENT_SHOW_METHOD, libevent displays the
kernel notification method that it uses.
```

## <a id="seealso"></a>See Also 

[pgbouncer](pgbouncer.html), [pgbouncer.ini](pgbouncer-ini.html)

