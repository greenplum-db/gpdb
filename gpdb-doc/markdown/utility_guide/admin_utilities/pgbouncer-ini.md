# pgbouncer.ini 

PgBouncer configuration file.

## Synopsys 

```
[databases]
db = ...

[pgbouncer]
...

[users]
...
```

## Description 

You specify PgBouncer configuration parameters and identify user-specific configuration parameters in the `pgbouncer.ini` configuration file.

The PgBouncer configuration file \(typically named `pgbouncer.ini`\) is specified in `.ini` format. Files in `.ini` format are composed of sections, parameters, and values. Section names are enclosed in square brackets, for example, `[section\_name]`. Parameters and values are specified in `key=value` format. Lines beginning with a semicolon \(`;`\) or pound sign \(`#`\) are considered comment lines and are ignored.

The PgBouncer configuration file can contain `%include` directives, which specify another file to read and process. This enables you to split the configuration file into separate parts. For example:

```
%include filename
```

If the filename provided is not an absolute path, the file system location is taken as relative to the current working directory.

The PgBouncer configuration file includes the following sections, described in detail below:

-   [\[databases\] Section](#topic_fmd_ckd_gs)
-   [\[pgbouncer\] Section](#topic_orc_gkd_gs)
-   [\[users\] Section](#topic_lzk_zjd_gs)

## \[databases\] Section 

The `[databases]` section contains `key=value` pairs, where the `key` is a database name and the `value` is a `libpq` connect-string list of `key`=`value` pairs.

A database name can contain characters `[0-9A-Za-z_.-]` without quoting. Names that contain other characters must be quoted with standard SQL identifier quoting:

-   Enclose names in double quotes \(`" "`\).
-   Represent a double-quote within an identifier with two consecutive double quote characters.

The database name `*` is the fallback database. PgBouncer uses the value for this key as a connect string for the requested database. Automatically-created database entries such as these are cleaned up if they remain idle longer than the time specified in `autodb_idle_timeout` parameter.

### Database Connection Parameters 

The following parameters may be included in the `value` to specify the location of the database.

dbname
:   The destination database name.

    Default: the client-specified database name

host
:   The name or IP address of the Greenplum master host. Host names are resolved at connect time. If DNS returns several results, they are used in a round-robin manner. The DNS result is cached and the `dns_max_ttl` parameter determines when the cache entry expires.

:   Default: not set; the connection is made through a Unix socket

port
:   The Greenplum Database master port.

:   Default: 5432

user, password
:   If `user=` is set, all connections to the destination database are initiated as the specified user, resulting in a single connection pool for the database.

    If the `user=` parameter is not set, PgBouncer attempts to log in to the destination database with the user name passed by the client. In this situation, there will be one pool for each user who connects to the database.

auth\_user
:   If `auth_user` is set, any user who is not specified in `auth_file` is authenticated by querying the `pg_shadow` database view. PgBouncer performs this query as the `auth_user` Greenplum Database user. `auth_user`'s password must be set in the `auth_file`.

client\_encoding
:   Ask for specific `client_encoding` from server.

datestyle
:   Ask for specific `datestyle` from server.

timezone
:   Ask for specific `timezone` from server.

### Pool Configuration 

You can use the following parameters for database-specific pool configuration.

pool\_size
:   Set the maximum size of pools for this database. If not set, the `default_pool_size` is used.

connect\_query
:   Query to be executed after a connection is established, but before allowing the connection to be used by any clients. If the query raises errors, they are logged but ignored otherwise.

pool\_mode
:   Set the pool mode for this database. If not set, the default `pool_mode` is used.

max\_db\_connections
:   Set a database-wide maximum number of PgBouncer connections for this database. The total number of connections for all pools for this database will not exceed this value.

## \[pgbouncer\] Section 

### Generic Settings 

logfile
:   The location of the log file. The log file is kept open. After log rotation, execute `kill -HUP pgbouncer` or run the `RELOAD;` command in the PgBouncer Administration Console.

    Default: not set

pidfile
:   The name of the pid file. Without a pidfile, you cannot run PgBouncer as a background \(daemon\) process.

    Default: not set

listen\_addr
:   A list of interface addresses where PgBouncer listens for TCP connections. You may also use `*`, which means to listen on all interfaces. If not set, only Unix socket connections are allowed.

    Specify addresses numerically \(IPv4/IPv6\) or by name.

    Default: not set

listen\_port
:   The port PgBouncer listens on. Applies to both TCP and Unix sockets.

    Default: 6432

unix\_socket\_dir
:   Specifies the location for the Unix sockets. Applies to both listening socket and server connections. If set to an empty string, Unix sockets are disabled. Required for online restart \(`-R` option\) to work.

    Default: `/tmp`

unix\_socket\_mode
:   Filesystem mode for the Unix socket.

    Default: 0777

unix\_socket\_group
:   Group name to use for Unix socket.

    Default: not set

user
:   If set, specifies the Unix user to change to after startup. This works only if PgBouncer is started as root or if `user` is the same as the current user.

    Default: not set

auth\_file
:   The name of the file containing the user names and passwords to load. The file format is the same as the Greenplum Database pg\_auth file.

    Default: not set

auth\_hba\_file
:   HBA configuration file to use when `auth_type` is `hba`. Refer to the [HBA file format](https://pgbouncer.github.io/config.html#hba-file-format) discussion in the PgBouncer documentation for information about PgBouncer support of the HBA authentication file format.

    Default: not set

auth\_type
:   How to authenticate users.

    pam
    :   Use PAM to authenticate users. `auth_file` is ignored. This method is not compatible with databases using the `auth_user` option. The service name reported to PAM is “pgbouncer”. PAM is not supported in the HBA configuration file.

    hba
    :   The actual authentication type is loaded from the `auth_hba_file`. This setting allows different authentication methods different access paths.

    cert
    :   Clients must connect with TLS using a valid client certificate. The client's username is taken from CommonName field in the certificate.

    md5
    :   Use MD5-based password check. `auth_file` may contain both MD5-encrypted or plain-text passwords. This is the default authentication method.

    plain
    :   Clear-text password is sent over wire. *Deprecated*.

    trust
    :   No authentication is performed. The username must still exist in the `auth_file`.

    any
    :   Like the `trust` method, but the username supplied is ignored. Requires that all databases are configured to log in with a specific user. Additionally, the console database allows any user to log in as admin.

auth\_query
:   Query to load a user's password from the database. If a user does not exist in the `auth_file` and the database entry includes an `auth_user`, this query is run in the database as `auth_user` to lookup up the user.

    Note that the query is run inside target database, so if a function is used it needs to be installed into each database.

    Default: `SELECT usename, passwd FROM pg_shadow WHERE usename=$1`

auth\_user
:   If `auth_user` is set, any user who is not specified in `auth_file` is authenticated through the `auth_query` query from the `pg_shadow` database view. PgBouncer performs this query as the `auth_user` Greenplum Database user. `auth_user`'s password must be set in the `auth_file`.

    Direct access to `pg_shadow` requires Greenplum Database administrative privileges. It is preferable to use a non-admin user that calls `SECURITY DEFINER` function instead.

pool\_mode
:   Specifies when a server connection can be reused by other clients.

    session
    :   Connection is returned to the pool when the client disconnects. Default.

    transaction
    :   Connection is returned to the pool when the transaction finishes.

    statement
    :   Connection is returned to the pool when the current query finishes. Long transactions with multiple statements are disallowed in this mode.

max\_client\_conn
:   Maximum number of client connections allowed. When increased, you should also increase the file descriptor limits. The actual number of file descriptors used is more than `max_client_conn`. The theoretical maximum used, when each user connects with its own username to the server is:

    ```
    max_client_conn + (max_pool_size * total_databases * total_users)
    ```

    If a database user is specified in the connect string, all users connect using the same username. Then the theoretical maximum connections is:

    ```
    max_client_conn + (max_pool_size * total_databases)
    ```

    \(The theoretical maximum should be never reached, unless someone deliberately crafts a load for it. Still, it means you should set the number of file descriptors to a safely high number. Search for `ulimit` in your operating system documentation.\)

    Default: 100

default\_pool\_size
:   The number of server connections to allow per user/database pair. This can be overridden in the per-database configuration.

    Default: 20

min\_pool\_size
:   Add more server connections to the pool when it is lower than this number. This improves behavior when the usual load drops and then returns suddenly after a period of total inactivity.

    Default: 0 \(disabled\)

reserve\_pool\_size
:   The number of additional connections to allow for a pool. 0 disables.

    Default: 0 \(disabled\)

reserve\_pool\_timeout
:   If a client has not been serviced in this many seconds, PgBouncer enables use of additional connections from the reserve pool. 0 disables.

    Default: 5.0

max\_db\_connections
:   The maximum number of connections per database. If you hit the limit, closing a client connection to one pool does not immediately allow a server connection to be established for another pool, because the server connection for the first pool is still open. Once the server connection closes \(due to idle timeout\), a new server connection will be opened for the waiting pool.

    Default: unlimited

max\_user\_connections
:   The maximum number of connections per-user. When you hit the limit, closing a client connection to one pool does not immediately allow a connection to be established for another pool, because the connection for the first pool is still open. After the connection for the first pool has closed \(due to idle timeout\), a new server connection is opened for the waiting pool.

server\_round\_robin
:   By default, PgBouncer reuses server connections in LIFO \(last-in, first-out\) order, so that a few connections get the most load. This provides the best performance when a single server serves a database. But if there is TCP round-robin behind a database IP, then it is better if PgBouncer also uses connections in that manner to achieve uniform load.

    Default: 0

ignore\_startup\_parameters
:   By default, PgBouncer allows only parameters it can keep track of in startup packets: `client_encoding`, `datestyle`, `timezone`, and `standard_conforming_strings`.

    All others parameters raise an error. To allow other parameters, specify them here so that PgBouncer can ignore them.

    Default: empty

disable\_pqexec
:   Disable Simple Query protocol \(PQexec\). Unlike Extended Query protocol, Simple Query protocol allows multiple queries in one packet, which allows some classes of SQL-injection attacks. Disabling it can improve security. This means that only clients that exclusively use Extended Query protocol will work.

    Default: 0

application\_name\_add\_host
:   Add the client host address and port to the application name setting set on connection start. This helps in identifying the source of bad queries. The setting is overwritten without detection if the application executes `SET application_name` after connecting.

    Default: 0

conffile
:   Show location of the current configuration file. Changing this parameter will result in PgBouncer using another config file for next RELOAD / SIGHUP.

    Default: file from command line

service\_name
:   Used during win32 service registration.

    Default: pgbouncer

job\_name
:   Alias for `service_name`.

### Log Settings 

syslog
:   Toggles syslog on and off.

    Default: 0

syslog\_ident
:   Under what name to send logs to syslog.

    Default: `pgbouncer`

syslog\_facility
:   Under what facility to send logs to syslog. Some possibilities are: `auth`, `authpriv`, `daemon`, `user`, `local0-7`

    Default: `daemon`

log\_connections
:   Log successful logins.

    Default: 1

log\_disconnections
:   Log disconnections, with reasons.

    Default: 1

log\_pooler\_errors
:   Log error messages that the pooler sends to clients.

    Default: 1

stats\_period
:   How often to write aggregated statistics to the log.

    Default: 60

### Console Access Control 

admin\_users
:   Comma-separated list of database users that are allowed to connect and run all commands on the PgBouncer Administration Console. Ignored when `auth_type=any`, in which case any username is allowed in as admin.

    Default: empty

stats\_users
:   Comma-separated list of database users that are allowed to connect and run read-only queries on the console. This includes all `SHOW` commands except `SHOW FDS`.

    Default: empty

### Connection Sanity Checks, Timeouts 

server\_reset\_query
:   Query sent to server on connection release, before making it available to other clients. At that moment no transaction is in progress so it should not include `ABORT` or `ROLLBACK`.

    The query should clean any changes made to a database session so that the next client gets a connection in a well-defined state. Default is `DISCARD ALL` which cleans everything, but that leaves the next client no pre-cached state.

    **Note:** Greenplum Database does not support `DISCARD ALL`.

    You can use other commands to clean up the session state. For example, `DEALLOCATE ALL` drops prepared statements, and `DISCARD TEMP` drops temporary tables.

    When transaction pooling is used, the `server_reset_query` should be empty, as clients should not use any session features. If clients do use session features, they will be broken because transaction pooling does not guarantee that the next query will run on the same connection.

    Default: `DISCARD ALL;` \(Not supported by Greenplum Database.\)

server\_reset\_query\_always
:   Whether `server_reset_query` should be run in all pooling modes. When this setting is off \(default\), the `server_reset_query` will be run only in pools that are in sessions pooling mode. Connections in transaction pooling mode should not have any need for reset query.

    Default: 0

server\_check\_delay
:   How long to keep released connections available for re-use without running sanity-check queries on it. If 0, then the query is run always.

    Default: 30.0

server\_check\_query
:   A simple do-nothing query to test the server connection.

    If an empty string, then sanity checking is disabled.

    Default: SELECT 1;

server\_lifetime
:   The pooler tries to close server connections that have been connected longer than this number of seconds. Setting it to 0 means the connection is to be used only once, then closed.

    Default: 3600.0

server\_idle\_timeout
:   If a server connection has been idle more than this many seconds it is dropped. If this parameter is set to 0, timeout is disabled. \[seconds\]

    Default: 600.0

server\_connect\_timeout
:   If connection and login will not finish in this number of seconds, the connection will be closed.

    Default: 15.0

server\_login\_retry
:   If a login fails due to failure from `connect()` or authentication, the pooler waits this many seconds before retrying to connect.

    Default: 15.0

client\_login\_timeout
:   If a client connects but does not manage to login in this number of seconds, it is disconnected. This is needed to avoid dead connections stalling `SUSPEND` and thus online restart.

    Default: 60.0

autodb\_idle\_timeout
:   If database pools created automatically \(via `*`\) have been unused this many seconds, they are freed. Their statistics are also forgotten.

    Default: 3600.0

dns\_max\_ttl
:   How long to cache DNS lookups, in seconds. If a DNS lookup returns several answers, PgBouncer round-robins between them in the meantime. The actual DNS TTL is ignored.

    Default: 15.0

dns\_nxdomain\_ttl
:   How long error and NXDOMAIN DNS lookups can be cached, in seconds.

    Default: 15.0

dns\_zone\_check\_period
:   Period to check if zone serial numbers have changed.

    PgBouncer can collect DNS zones from hostnames \(everything after first dot\) and then periodically check if the zone serial numbers change. If changes are detected, all hostnames in that zone are looked up again. If any host IP changes, its connections are invalidated.

    Works only with UDNS and c-ares backend \(`--with-udns` or `--with-cares` to configure\).

    Default: 0.0 \(disabled\)

### TLS settings 

client\_tls\_sslmode
:   TLS mode to use for connections from clients. TLS connections are disabled by default. When enabled, `client_tls_key_file` and `client_tls_cert_file` must be also configured to set up the key and certificate PgBouncer uses to accept client connections.

    -   `disable`: Plain TCP. If client requests TLS, it’s ignored. Default.
    -   `allow`: If client requests TLS, it is used. If not, plain TCP is used. If client uses client-certificate, it is not validated.
    -   `prefer`: Same as `allow`.
    -   `require`: Client must use TLS. If not, client connection is rejected. If client uses client-certificate, it is not validated.
    -   `verify-ca`: Client must use TLS with valid client certificate.
    -   `verify-full`: Same as `verify-ca`.

client\_tls\_key\_file
:   Private key for PgBouncer to accept client connections.

:   Default: not set

client\_tls\_cert\_file
:   Root certificate file to validate client certificates.

:   Default: unset

client\_tls\_ca\_file
:   Root certificate to validate client certificates.

:   Default: unset

client\_tls\_protocols
:   Which TLS protocol versions are allowed.

:   Valid values: are `tlsv1.0`, `tlsv1.1`, `tlsv1.2`.

:   Shortcuts: `all` \(`tlsv1.0`,`tlsv1.1`,`tlsv1.2`\), `secure` \(`tlsv1.2`\), `legacy` \(`all`\).

:   Default: `all`

client\_tls\_ciphers
:   Default: `fast`

client\_tls\_ecdhcurve
:   Elliptic Curve name to use for ECDH key exchanges.

:   Allowed values: `none` \(DH is disabled\), `auto` \(256-bit ECDH\), curve name.

:   Default: `auto`

client\_tls\_dheparams
:   DHE key exchange type.

:   Allowed values: `none` \(DH is disabled\), `auto` \(2048-bit DH\), `legacy` \(1024-bit DH\).

:   Default: `auto`

server\_tls\_sslmode
:   TLS mode to use for connections to Greenplum Database and PostgreSQL servers. TLS connections are disabled by default.

    -   `disabled`: Plain TCP. TLS is not requested from the server. Default.
    -   `allow`: If server rejects plain, try TLS. \(*PgBouncer Documentation is speculative on this..*\)
    -   `prefer`: TLS connection is always requested first. When connection is refused, plain TPC is used. Server certificate is not validated.
    -   `require`: Connection must use TLS. If server rejects it, plain TCP is not attempted. Server certificate is not validated.
    -   `verify-ca`: Connection must use TLS and server certificate must be valid according to `server_tls_ca_file`. The server hostname is not verfied against the certificate.
    -   `verify-full`: Connection must use TLS and the server certificate must be valid according to `server_tls_ca_file`. The server hostname must match the hostname in the certificate.

server\_tls\_ca\_file
:   Path to the root certificate file used to validate Greenplum Database and PostgreSQL server certificates.

:   Default: unset

server\_tls\_key\_file
:   The private key for PgBouncer to authenticate against Greenplum Database or PostgreSQL server.

:   Default: not set

server\_tls\_cert\_file
:   Certificate for private key. Greenplum Database or PostgreSQL servers can validate it.

:   Default: not set

server\_tls\_protocols
:   Which TLS protocol versions are allowed.

:   Valid values are: `tlsv1.0`, `tlsv1.1`, `tlsv1.2`.

:   Shortcuts: `all` \(`tlsv1.0`, `tlsv1.1`, `tlsv1.2`\); `secure` \(`tlsv1.2`\); `legacy` \(`all`\).

:   Default: `all`

server\_tls\_ciphers
:   Default: `fast`

### Dangerous Timeouts 

Setting the following timeouts can cause unexpected errors.

query\_timeout
:   Queries running longer than this \(seconds\) are canceled. This parameter should be used only with a slightly smaller server-side `statement_timeout`, to trap queries with network problems. \[seconds\]

    Default: 0.0 \(disabled\)

query\_wait\_timeout
:   The maximum time, in seconds, queries are allowed to wait for execution. If the query is not assigned a connection during that time, the client is disconnected. This is used to prevent unresponsive servers from grabbing up connections.

    Default: 120

client\_idle\_timeout
:   Client connections idling longer than this many seconds are closed. This should be larger than the client-side connection lifetime settings, and only used for network problems.

    Default: 0.0 \(disabled\)

idle\_transaction\_timeout
:   If client has been in "idle in transaction" state longer than this \(seconds\), it is disconnected.

    Default: 0.0 \(disabled\)

### Low-level Network Settings 

pkt\_buf
:   Internal buffer size for packets. Affects the size of TCP packets sent and general memory usage. Actual `libpq` packets can be larger than this so there is no need to set it large.

    Default: 4096

max\_packet\_size
:   Maximum size for packets that PgBouncer accepts. One packet is either one query or one result set row. A full result set can be larger.

    Default: 2147483647

listen\_backlog
:   Backlog argument for the `listen(2)` system call. It determines how many new unanswered connection attempts are kept in queue. When the queue is full, further new connection attemps are dropped.

    Default: 128

sbuf\_loopcnt
:   How many times to process data on one connection, before proceeding. Without this limit, one connection with a big result set can stall PgBouncer for a long time. One loop processes one `pkt_buf` amount of data. 0 means no limit.

    Default: 5

suspend\_timeout
:   How many seconds to wait for buffer flush during `SUSPEND` or restart \(`-R`\). Connection is dropped if flush does not succeed.

    Default: 10

tcp\_defer\_accept
:   For details on this and other TCP options, please see the tcp\(7\) man page.

    Default: 45 on Linux, otherwise 0

tcp\_socket\_buffer
:   Default: not set

tcp\_keepalive
:   Turns on basic keepalive with OS defaults.

    On Linux, the system defaults are `tcp_keepidle=7200`, `tcp_keepintvl=75`, `tcp_keepcnt=9`.

    Default: 1

tcp\_keepcnt
:   Default: not set

tcp\_keepidle
:   Default: not set

tcp\_keepintvl
:   Default: not set

## \[users\] Section 

This section contains `key`=`value` pairs, where the `key` is a user name and the `value` is a `libpq` connect-string list of `key`=`value` pairs.

**Pool configuration**

pool\_mode
:   Set the pool mode for all connections from this user. If not set, the database or default `pool_mode` is used.

## Example Configuration Files 

**Minimal Configuration**

```
[databases]
postgres = host=127.0.0.1 dbname=postgres auth_user=gpadmin

[pgbouncer]
pool_mode = session
listen_port = 6543
listen_addr = 127.0.0.1
auth_type = md5
auth_file = users.txt
logfile = pgbouncer.log
pidfile = pgbouncer.pid
admin_users = someuser
stats_users = stat_collector

```

Use connection parameters passed by the client:

```
[databases]
* =

[pgbouncer]
listen_port = 6543
listen_addr = 0.0.0.0
auth_type = trust
auth_file = bouncer/users.txt
logfile = pgbouncer.log
pidfile = pgbouncer.pid
ignore_startup_parameters=options
```

**Database Defaults**

```
[databases]

; foodb over unix socket
foodb =

; redirect bardb to bazdb on localhost
bardb = host=127.0.0.1 dbname=bazdb

; access to destination database will go with single user
forcedb = host=127.0.0.1 port=300 user=baz password=foo client_encoding=UNICODE datestyle=ISO

```

## See Also 

[pgbouncer](pgbouncer.html), [pgbouncer-admin](pgbouncer-admin.html), [PgBouncer Configuration Page](https://pgbouncer.github.io/config.html)

