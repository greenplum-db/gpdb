# clusterdb 

Reclusters tables that were previously clustered with `CLUSTER`.

## <a id="section2"></a>Synopsis 

``` {#client_util_synopsis}
clusterdb [<connection-option> ...] [--verbose | -v] [--table | -t <table>] [[--dbname | -d] <dbname]

clusterdb [<connection-option> ...] [--all | -a] [--verbose | -v]

clusterdb -? | --help

clusterdb -V | --version
```

## <a id="section3"></a>Description 

To cluster a table means to physically reorder a table on disk according to an index so that index scan operations can access data on disk in a somewhat sequential order, thereby improving index seek performance for queries that use that index.

The `clusterdb` utility will find any tables in a database that have previously been clustered with the `CLUSTER` SQL command, and clusters them again on the same index that was last used. Tables that have never been clustered are not affected.

`clusterdb` is a wrapper around the SQL command `CLUSTER`. Although clustering a table in this way is supported in Greenplum Database, it is not recommended because the `CLUSTER` operation itself is extremely slow.

If you do need to order a table in this way to improve your query performance, use a `CREATE TABLE AS` statement to reorder the table on disk rather than using `CLUSTER`. If you do 'cluster' a table in this way, then `clusterdb` would not be relevant.

## <a id="section4"></a>Options 

-a \| --all
:   Cluster all databases.

\[-d\] dbname \| \[--dbname=\]dbname
:   Specifies the name of the database to be clustered. If this is not specified, the database name is read from the environment variable `PGDATABASE`. If that is not set, the user name specified for the connection is used.

-e \| --echo
:   Echo the commands that `clusterdb` generates and sends to the server.

-q \| --quiet
:   Do not display a response.

-t table \| --table=table
:   Cluster the named table only. Multiple tables can be clustered by writing multiple `-t` switches.

-v \| --verbose
:   Print detailed information during processing.

-V \| --version
:   Print the `clusterdb` version and exit.

-? \| --help
:   Show help about `clusterdb` command line arguments, and exit.

**Connection Options**

-h host \| --host=host
:   The host name of the machine on which the Greenplum coordinator database server is running. If not specified, reads from the environment variable `PGHOST` or defaults to localhost.

-p port \| --port=port
:   The TCP port on which the Greenplum coordinator database server is listening for connections. If not specified, reads from the environment variable `PGPORT` or defaults to 5432.

-U username \| --username=username
:   The database role name to connect as. If not specified, reads from the environment variable `PGUSER` or defaults to the current system role name.

-w \| --no-password
:   Never issue a password prompt. If the server requires password authentication and a password is not available by other means such as a `.pgpass` file, the connection attempt will fail. This option can be useful in batch jobs and scripts where no user is present to enter a password.

-W \| --password
:   Force a password prompt.

--maintenance-db=dbname
:   Specifies the name of the database to connect to discover what other databases should be clustered. If not specified, the `postgres` database will be used, and if that does not exist, `template1` will be used.

## <a id="section6"></a>Examples 

To cluster the database `test`:

```
clusterdb test
```

To cluster a single table `foo` in a database named `xyzzy`:

```
clusterdb --table foo xyzzyb
```

## <a id="section7"></a>See Also 

[CLUSTER](../../ref_guide/sql_commands/CLUSTER.html)

