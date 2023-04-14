# reindexdb 

Rebuilds indexes in a database.

## <a id="section2"></a>Synopsis 

``` {#client_util_synopsis}
reindexdb [<connection-option> ...] [<option> ...] [-S | --schema <schema>] ...
        [-t | --table <table>] 
        [-i | --index <index>] ... [<dbname>]

reindexdb [<connection-option> ...] [<option> ...] -a | --all

reindexdb [<connection-option> ...] [<option> ...] -s | --system [<dbname>]

reindexdb -? | --help

reindexdb -V | --version
```

## <a id="section3"></a>Description 

`reindexdb` is a utility for rebuilding indexes in Greenplum Database.

`reindexdb` is a wrapper around the SQL command [REINDEX](../../ref_guide/sql_commands/REINDEX.html). There is no effective difference between reindexing databases via this utility and via other methods for accessing the server.

## <a id="section4"></a>Options 

-a
--all
:   Reindex all databases.

\[-d\] dbname
\[--dbname=\]dbname
:   Specifies the name of the database to be reindexed, when `-a` or `--all` is not used. If this is not specified, Greenplum Database reads the database name from the environment variable `PGDATABASE`. If that is not set, the user name specified for the connection is used. The dbname can be a connection string. If so, connection string parameters will override any conflicting command line options.

-e
--echo
:   Echo the commands that `reindexdb` generates and sends to the server.

-i index
--index=index
:   Recreate index only. Multiple indexes can be recreated by writing multiple `-i` switches.

-q
--quiet
:   Do not display progress messages.

-s
--system
:   Reindex the database's system catalogs.

-t table
--table=table
:   Reindex table only. Multiple tables can be reindexed by writing multiple `-t` switches.

-v
--verbose
:   Print detailed information during processing.

-V
--version
:   Print the `reindexdb` version and exit.

-?
--help
:   Show help about `reindexdb` command line arguments, and exit.

**Connection Options**

`reindexdb` also accepts the following command-line arguments for connection parameters:

-h host
--host=host
:   Specifies the host name of the machine on which the Greenplum coordinator database server is running. If not specified, reads from the environment variable `PGHOST` or defaults to localhost.

-p port
--port=port
:   Specifies the TCP port on which the Greenplum coordinator database server is listening for connections. If not specified, reads from the environment variable `PGPORT` or defaults to 5432.

-U username
--username=username
:   The database user name to connect as. If not specified, reads from the environment variable `PGUSER` or defaults to the current system user name.

-w
--no-password
:   Never issue a password prompt. If the server requires password authentication and a password is not available by other means such as a `.pgpass` file, the connection attempt will fail. This option can be useful in batch jobs and scripts where no user is present to enter a password.

-W
--password
:   Force `reindexdb` to prompt for a password before connecting to a database.
:   This option is never essential, since `reindexdb` automatically prompts for a password if the server demands password authentication. However, `reindexdb` will waste a connection attempt finding out that the server wants a password. In some cases it is worth typing `-W` to avoid the extra connection attempt.

--maintenance-db=dbname
:   Specifies the name of the database to connect to discover what other databases should be reindexed, when `-a` or `--all` is used. If not specified, the `postgres` database will be used, and if that does not exist, `template1` will be used. This can be a connection string. If so, connection string parameters will override any conflicting command line options. Also, connection string parameters other than the database name itself will be re-used when connecting to other databases.

## <a id="section6"></a>Notes 

`reindexdb` causes locking of system catalog tables, which could affect currently running queries. To avoid disrupting ongoing business operations, schedule the `reindexb` operation during a period of low activity.

`reindexdb` might need to connect several times to the coordinator server, asking for a password each time. It is convenient to have a `~/.pgpass` file in such cases.

## <a id="section7"></a>Examples 

To reindex the database `mydb`:

```
reindexdb mydb
```

To reindex the table `foo` and the index `bar` in a database named `abcd`:

```
reindexdb --table=foo --index=bar abcd
```

## <a id="section8"></a>See Also 

[REINDEX](../../ref_guide/sql_commands/REINDEX.html)

