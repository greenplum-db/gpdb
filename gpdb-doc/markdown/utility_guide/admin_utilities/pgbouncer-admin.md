# pgbouncer-admin 

PgBouncer Administration Console.

## Synopsis 

```
psql -p <port> pgbouncer
```

## Description 

The PgBouncer Adminstration Console is available via `psql`. Connect to the PgBouncer `port` and the virtual database named `pgbouncer` to log in to the console.

Users listed in the `pgbouncer.ini` configuration parameters `admin_users` and `stats_users` have privileges to log in to the PgBouncer Administration Console.

You can control connections between PgBouncer and Greenplum Database from the console. You can also set PgBouncer configuration parameters.

## Options 

-p port
:   The PgBouncer port number.

## Command Syntax 

```
pgbouncer=# SHOW help;
NOTICE:  Console usage
DETAIL:  
    SHOW HELP|CONFIG|DATABASES|POOLS|CLIENTS|SERVERS|VERSION
    SHOW FDS|SOCKETS|ACTIVE_SOCKETS|LISTS|MEM
    SHOW DNS_HOSTS|DNS_ZONES
    SHOW STATS|STATS_TOTALS|STATS_AVERAGES
    SET key = arg
    RELOAD
    PAUSE [<db>]
    RESUME [<db>]
    DISABLE <db>
    ENABLE <db>
    KILL <db>
    SUSPEND
    SHUTDOWN
```

## Administration Commands 

The following PgBouncer administration commands control the running `pgbouncer` process.

PAUSE \[db\]
:   If no database is specified, PgBouncer tries to disconnect from all servers, first waiting for all queries to complete. The command will not return before all queries are finished. This command is to be used to prepare to restart the database.

:   If a database name is specified, PgBouncer pauses only that database.

:   If you run a `PAUSE db` command, and then a `PAUSE` command to pause all databases, you must execute two `RESUME` commands, one for all databases, and one for the named database.

SUSPEND
:   All socket buffers are flushed and PgBouncer stops listening for data on them. The command will not return before all buffers are empty. To be used when rebooting PgBouncer online.

RESUME \[ db \]
:   Resume work from a previous `PAUSE` or `SUSPEND` command.

:   If a database was specified for the `PAUSE` command, the database must also be specified with the `RESUME` command.

:   After pausing all databases with the `PAUSE` command, resuming a single database with `RESUME db` is not supported.

DISABLE db
:   Reject all new client connections on the database.

ENABLE db
:   Allow new client connections on the database.

KILL db
:   Immediately drop all client and server connections to the named database.

SHUTDOWN
:   Stop PgBouncer process. To exit from the `psql` command line session, enter `\q`.

RELOAD
:   The PgBouncer process reloads the current configuration file and updates the changeable settings.

SET key = value
:   Override specified configuration setting. See the [`SHOW CONFIG;`](#CONFIG) command.

## SHOW Command 

The `SHOW category` command displays different types of PgBouncer information. You can specify one of the following categories:

-   [ACTIVE\_SOCKETS](#ACTIVE_SOCKETS)
-   [CLIENTS](#CLIENTS)
-   [CONFIG](#CONFIG)
-   [DATABASES](#DATABASES)
-   [DNS\_HOSTS](#DNS_HOSTS)
-   [DNS\_ZONES](#DNS_ZONES)
-   [FDS](#FDS)
-   [POOLS](#POOLS)
-   [SERVERS](#SERVERS)
-   [SOCKETS](#SOCKETS)
-   [STATS](#STATS)
-   [STATS\_TOTALS](#STATS_TOTALS)
-   [STATS\_AVERAGES](#STATS_AVERAGES)
-   [LISTS](#LISTS)
-   [MEM](#MEM)
-   [USERS](#USERS)
-   [VERSION](#VERSION)

### ACTIVE\_SOCKETS 

|Column|Description|
|------|-----------|
|type|S, for server, C for client.|
|user|Username `pgbouncer` uses to connect to server.|
|database|Database name.|
|state|State of the server connection, one of `active`, `used` or `idle`.|
|addr|IP address of PostgreSQL server.|
|port|Port of PostgreSQL server.|
|local\_addr|Connection start address on local machine.|
|local\_port|Connection start port on local machine.|
|connect\_time|When the connection was made.|
|request\_time|When last request was issued.|
|wait|Time waiting.|
|wait\_us|Time waiting \(microseconds\).|
|ptr|Address of internal object for this connection. Used as unique ID.|
|link|Address of client connection the server is paired with.|
|remote\_pid|Process identifier of backend server process.|
|tls|TLS context.|
|recv\_pos|Receive position in the I/O buffer.|
|pkt\_pos|Parse position in the I/O buffer.|
|pkt\_remain|Number of packets remaining on the socket.|
|send\_pos|Send position in the packet.|
|send\_remain|Total packet length remaining to send.|
|pkt\_avail|Amount of I/O buffer left to parse.|
|send\_avail|Amount of I/O buffer left to send.|

### CLIENTS 

|Column|Description|
|------|-----------|
|type|C, for client.|
|user|Client connected user.|
|database|Database name.|
|state|State of the client connection, one of `active`, `used`, `waiting` or `idle`.|
|addr|IP address of client, or `unix` for a socket connection.|
|port|Port client is connected to.|
|local\_addr|Connection end address on local machine.|
|local\_port|Connection end port on local machine.|
|connect\_time|Timestamp of connect time.|
|request\_time|Timestamp of latest client request.|
|wait|Time waiting.|
|wait\_us|Time waiting \(microseconds\).|
|ptr|Address of internal object for this connection. Used as unique ID.|
|link|Address of server connection the client is paired with.|
|remote\_pid|Process ID, if client connects with Unix socket and the OS supports getting it.|
|tls|Client TLS context.|

### CONFIG 

List of current PgBouncer parameter settings

|Column|Description|
|------|-----------|
|key|Configuration variable name|
|value|Configuration value|
|changeable|Either `yes` or `no`. Shows whether the variable can be changed while running. If `no`, the variable can be changed only at boot time.|

### DATABASES 

|Column|Description|
|------|-----------|
|name|Name of configured database entry.|
|host|Host pgbouncer connects to.|
|port|Port pgbouncer connects to.|
|database|Actual database name pgbouncer connects to.|
|force\_user|When user is part of the connection string, the connection between pgbouncer and the database server is forced to the given user, whatever the client user.|
|pool\_size|Maximum number of server connections.|
|reserve\_pool|The number of additional connections that can be created if the pool reaches `pool_size`.|
|pool\_mode|The database's override `pool_mode` or NULL if the default will be used instead.|
|max\_connections|Maximum number of connections for all pools for this database.|
|current\_connections|The total count of connections for all pools for this database.|
|paused|Paused/unpaused state of the database.|
|disabled|Enabled/disabled state of the database.|

### DNS\_HOSTS 

|Column|Description|
|------|-----------|
|hostname|Host name|
|ttl|How many seconds until next lookup.|
|addrs|Comma-separated list of addresses.|

### DNS\_ZONES 

|Column|Description|
|------|-----------|
|zonename|Zone name|
|serial|Current DNS serial number|
|count|Hostnames belonging to this zone|

### FDS 

`SHOW FDS` is an internal command used for an online restart, for example when upgrading to a new PgBouncer version. It displays a list of file descriptors in use with the internal state attached to them. This command blocks the internal event loop, so it should not be used while PgBouncer is in use.

When the connected user has username "pgbouncer", connects through a Unix socket, and has the same UID as the running process, the actual file descriptors are passed over the connection.

|Column|Description|
|------|-----------|
|fd|File descriptor numeric value.|
|task|One of `pooler`, `client`, or `server`.|
|user|User of the connection using the file descriptor.|
|database|Database of the connection using the file descriptor.|
|addr|IP address of the connection using the file descriptor, "unix" if a Unix socket is used.|
|port|Port used by the connection using the file descriptor.|
|cancel|Cancel key for this connection.|
|link|File descriptor for corresponding server/client. NULL if idle.|
|client\_encoding|Character set used for the database.|
|std\_strings|This controls whether ordinary string literals \('...'\) treat backslashes literally, as specified in the SQL standard.|
|datestyle|Display format for date and time values.|
|timezone|The timezone for interpreting and displaying time stamps.|
|password|`auth_user`'s password.|

### LISTS 

Shows the following PgBouncer statistcs in two columns: the item label and value.

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
|dns\_names|Count of DNS names.|
|dns\_zones|Count of DNS zones.|
|dns\_queries|Count of DNS queries.|
|dns\_pending|Count of in-flight DNS queries.|

### MEM 

Shows cache memory information for these PgBouncer caches:

-   user\_cache
-   db\_cache
-   pool\_cache
-   server\_cache
-   client\_cache
-   iobuf\_cache

|Column|Description|
|------|-----------|
|name|Name of cache.|
|size|The size of a single slot in the cache.|
|used|Number of used slots in the cache.|
|free|The number of available slots in the cache.|
|memtotal|Total bytes used by the cache.|

### POOLS 

A new pool entry is made for each pair of \(database, user\).

|Column|Description|
|------|-----------|
|database|Database name.|
|user|User name.|
|cl\_active|Client connections that are linked to server connection and can process queries.|
|cl\_waiting|Client connections have sent queries but have not yet got a server connection.|
|sv\_active|Server connections that linked to client.|
|sv\_idle|Server connections that are unused and immediately usable for client queries.|
|sv\_used|Server connections that have been idle more than `server_check_delay`. The `server_check_query` query must be run on them before they can be used.|
|sv\_tested|Server connections that are currently running either `server_reset_query` or `server_check_query`.|
|sv\_login|Server connections currently in process of logging in.|
|maxwait|How long the first \(oldest\) client in the queue has waited, in seconds. If this begins to increase, the current pool of servers does not handle requests fast enough. The cause may be either an overloaded server or the `pool_size` setting is too small.|
|maxwait\_us|`max_wait` \(microseconds\).|
|pool\_mode|The pooling mode in use.|

### SERVERS 

|Column|Description|
|------|-----------|
|type|S, for server.|
|user|User ID that `pgbouncer` uses to connect to server.|
|database|Database name.|
|state|State of the pgbouncer server connection, one of `active`, `used`, or `idle`.|
|addr|IP address of the Greenplum or PostgreSQL server.|
|port|Port of the Greenplum or PostgreSQL server.|
|local\_addr|Connection start address on local machine.|
|local\_port|Connection start port on local machine.|
|connect\_time|When the connection was made.|
|request\_time|When the last request was issued.|
|wait|Time waiting.|
|wait\_us|Time waiting \(microseconds\).|
|ptr|Address of the internal object for this connection. Used as unique ID.|
|link|Address of gthe client connection the server is paired with.|
|remote\_pid|Pid of backend server process. If the connection is made over Unix socket and the OS supports getting process ID info, it is the OS pid. Otherwise it is extracted from the cancel packet the server sent, which should be PID in case server is PostgreSQL, but it is a random number in case server is another PgBouncer.|
|tls|TLS context.|

### STATS 

Shows statistics.

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
|avg\_xact\_count|Average number of SQL transactions pooled by PgBouncer.|
|avg\_query\_count|Average queries per second in last stats period.|
|avg\_recv|Average received \(from clients\) bytes per second.|
|avg\_sent|Average sent \(to clients\) bytes per second.|
|avg\_xact\_time|Average transaction duration in microseconds.|
|avg\_query\_time|Average query duration in microseconds.|
|avg\_wait\_time|Time spent by clients waiting for a server in microseconds \(average per second\).|

### STATS\_AVERAGES 

Subset of `SHOW STATS` showing the average values for selected statistics.

### STATS\_TOTALS 

Subset of `SHOW STATS` showing the total values for selected statistics.

### USERS 

|Column|Description|
|------|-----------|
|name|The user name|
|pool\_mode|The user's override pool\_mode, or NULL if the default will be used instead.|

### VERSION 

Display PgBouncer version information.

**Note:** This reference documentation is based on the PgBouncer 1.8.1 documentation.

## See Also 

[pgbouncer](pgbouncer.html), [pgbouncer.ini](pgbouncer-ini.html)

