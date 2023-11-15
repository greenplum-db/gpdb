---
title: pgbouncer 
---

Manages database connection pools.

## <a id="syn"></a>Synopsis 

```
pgbouncer [OPTION ...] <pgbouncer.ini>

  OPTION
   [ -d | --daemon ]
   [ -R | --reboot ]
   [ -q | --quiet ]
   [ -v | --verbose ]
   [ {-u | --user}=username ]

pgbouncer [ -V | --version ] | [ -h | --help ]
```

## <a id="desc"></a>Description 

PgBouncer is a light-weight connection pool manager for Greenplum and PostgreSQL databases. PgBouncer maintains a pool of connections for each database user and database combination. PgBouncer either creates a new database connection for the client or reuses an existing pooled connection for the same user and database. When the client disconnects, PgBouncer returns the connection to the pool for re-use.

PgBouncer supports the standard connection interface shared by PostgreSQL and Greenplum Database. The Greenplum Database client application \(for example, `psql`\) should connect to the host and port on which PgBouncer is running rather than directly to the Greenplum Database master host and port.

You configure PgBouncer and its access to Greenplum Database via a configuration file. You provide the configuration file name, usually `<pgbouncer.ini>`, when you run the `pgbouncer` command. This file provides location information for Greenplum databases. The `pgbouncer.ini` file also specifies process, connection pool, authorized users, and authentication configuration for PgBouncer, among other configuration options.

By default, the `pgbouncer` process runs as a foreground process. You can optionally start `pgbouncer` as a background \(daemon\) process with the `-d` option.

The `pgbouncer` process is owned by the operating system user that starts the process. You can optionally specify a different user name under which to start `pgbouncer`.

PgBouncer includes a `psql`-like administration console. Authorized users can connect to a virtual database to monitor and manage PgBouncer. You can manage a PgBouncer daemon process via the admin console. You can also use the console to update and reload the PgBouncer configuration at runtime without stopping and restarting the process.

For additional information about PgBouncer, refer to the [PgBouncer FAQ](https://pgbouncer.github.io/faq.html).

## <a id="opt"></a>Options 

-d \| --daemon
:   Run PgBouncer as a daemon \(a background process\). The default start-up mode is to run as a foreground process. 

:   In daemon mode, setting `pidfile` as well as `logfile` or `syslog` is required. No log messages will be written to `stderr` after going into the background.

:   To stop a PgBouncer process that was started as a daemon, issue the `SHUTDOWN` command from the PgBouncer administration console.

-R \| --reboot
:   Restart PgBouncer using the specified command line arguments. That means connecting to the running process, loading the open sockets from it, and then using them. If there is no active process, boot normally. Non-TLS connections to databases are maintained during restart; TLS connections are dropped.

:   To restart PgBouncer as a daemon, specify the options `-Rd`.

    > **Note** Restart is available only if the operating system supports Unix sockets and the PgBouncer `unix_socket_dir` configuration is not deactivated.

-q \| --quiet
:   Run quietly. Do not log to `stderr`. This does not affect logging verbosity, only that `stderr` is not to be used. For use in `init.d` scripts.

-v \| --verbose
:   Increase message verbosity. Can be specified multiple times.

\{-u \| --user\}=\<username\>
:   Assume the identity of username on PgBouncer process start-up.

-V \| --version
:   Show the version and exit.

-h \| --help
:   Show the command help message and exit.

## <a id="section7"></a>See Also 

[pgbouncer.ini](pgbouncer-ini.html), [pgbouncer-admin](pgbouncer-admin.html)

