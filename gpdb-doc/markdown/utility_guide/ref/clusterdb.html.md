# clusterdb 

Reclusters tables that were previously clustered with `CLUSTER`.

## <a id="section2"></a>Synopsis 

``` {#client_util_synopsis}
clusterdb [<connection-option> ...] [--verbose | -v] [--table | -t <table>] [[--dbname | -d] <dbname]

clusterdb [<connection-option> ...] [--verbose | -v] --all | -a

clusterdb -? | --help

clusterdb -V | --version
```

## <a id="section3"></a>Description 

To cluster a table means to physically reorder a table on disk according to an index. Clustering helps improving index seek performance for queries that use that index. Clustering is a one-time operation: when the table is subsequently updated, the changes are not clustered. That is, no attempt is made to store new or updated rows according to their index order.

The `clusterdb` utility will find any tables in a database that have previously been clustered with the `CLUSTER` SQL command, and clusters them again on the same index that was last used. Tables that have never been clustered are not affected.

`clusterdb` is a wrapper around the SQL command [CLUSTER](../../ref_guide/sql_commands/CLUSTER.html). There is no effective difference between clustering databases via this utility and via other methods for accessing the server.

## <a id="section4"></a>Options 

`clusterdb` accepts the following command-line arguments:

-a
--all
:   Cluster all databases.

\[-d\] dbname
\[--dbname=\]dbname
:   Specifies the name of the database to be clustered, when `-a/--all` is not used. If this is not specified, the database name is read from the environment variable `PGDATABASE`. If that is not set, the user name specified for the connection is used. The dbname can be a [connection string](https://www.postgresql.org/docs/12/libpq-connect.html#LIBPQ-CONNSTRING). If so, connection string parameters will override any conflicting command line options.

-e
--echo
:   Echo the commands that `clusterdb` generates and sends to the server.

-q
--quiet
:   Do not display progress messages.

-t table
--table=table
:   Cluster the named table only. You can cluster multiple tables by specifying multiple `-t` switches.

-v
--verbose
:   Print detailed information during processing.

-V
--version
:   Print the `clusterdb` version, and exit.

-?
--help
:   Show help about `clusterdb` command line arguments, and exit.

**Connection Options**

`clusterdb` also accepts the following command-line arguments for connection parameters:

-h host
--host=host
:   Specifies the host name of the machine on which the Greenplum coordinator database server is running. If not specified, reads from the environment variable `PGHOST` or defaults to `localhost`.

-p port
--port=port
:   Specifies the TCP port on which the Greenplum coordinator database server is listening for connections. If not specified, reads from the environment variable `PGPORT` or defaults to 5432.

-U username
--username=username
:   The database role name to connect as. If not specified, reads from the environment variable `PGUSER` or defaults to the current system role name.

-w
--no-password
:   Never issue a password prompt. If the server requires password authentication and a password is not available by other means such as a `.pgpass` file, the connection attempt will fail. This option can be useful in batch jobs and scripts where no user is present to enter a password.

-W
--password
:   Force `clusterdb` to prompt for a password before connecting to a database.
:   This option is never essential, since `clusterdb` will automatically prompt for a password if the server demands password authentication. However, `clusterdb` will waste a connection attempt finding out that the server wants a password. In some cases it is worth typing `-W` to avoid the extra connection attempt.

--maintenance-db=dbname
:   Specifies the name of the database to connect to discover what other databases should be clustered. If not specified, the `postgres` database will be used, and if that does not exist, `template1` will be used. This can be a [connection](https://www.postgresql.org/docs/12/libpq-connect.html#LIBPQ-CONNSTRING) string. If so, connection string parameters will override any conflicting command line options. Also, connection string parameters other than the database name itself will be re-used when connecting to other databases.

## <a id="section6e"></a>Environment

PGDATABASE
PGHOST
PGPORT
PGUSER
:   Default connection parameters.

PG_COLOR
:   Specifies whether to use color in diagnostic messages. Possible values are `always`, `auto`, and `never`.

This utility, like most other Greenplum Database utilities, also uses the environment variables supported by `libpq`.

## <a id="section6d"></a>Diagnostics

In case of difficulty, see [CLUSTER](../../ref_guide/sql_commands/CLUSTER.html) and [psql](psql.html) for discussions of potential problems and error messages. The database server must be running at the targeted host. Also, any default connection settings and environment variables used by the `libpq` front-end library will apply.

## <a id="section6"></a>Examples 

To cluster the database named `test`:

```
clusterdb test
```

To cluster a single table `foo` in a database named `xyzzy`:

```
clusterdb --table=foo xyzzy
```

## <a id="section7"></a>See Also 

[CLUSTER](../../ref_guide/sql_commands/CLUSTER.html)

