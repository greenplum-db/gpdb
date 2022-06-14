# clusterdb 

Reclusters tables that were previously clustered with `CLUSTER`.

## Synopsis 

``` {#client_util_synopsis}
clusterdb [<connection-option> ...] [-v] [-t <table>] [[-d] <dbname>]

clusterdb [<connection-option> ...] [-a] [-v]

clusterdb --help

clusterdb --version
```

## Description 

To cluster a table means to physically reorder a table on disk according to an index so that index scan operations can access data on disk in a somewhat sequential order, thereby improving index seek performance for queries that use that index.

The `clusterdb` utility will find any tables in a database that have previously been clustered with the `CLUSTER` SQL command, and clusters them again on the same index that was last used. Tables that have never been clustered are not affected.

`clusterdb` is a wrapper around the SQL command `CLUSTER`. Although clustering a table in this way is supported in Greenplum Database, it is not recommended because the `CLUSTER` operation itself is extremely slow.

If you do need to order a table in this way to improve your query performance, Greenplum recommends using a `CREATE TABLE AS` statement to reorder the table on disk rather than using `CLUSTER`. If you do 'cluster' a table in this way, then `clusterdb` would not be relevant.

## Options 

-a \| --all
:   Cluster all databases.

\[-d\] dbname \| \[--dbname\] dbname
:   Specifies the name of the database to be clustered. If this is not specified, the database name is read from the environment variable `PGDATABASE`. If that is not set, the user name specified for the connection is used.

-e \| --echo
:   Echo the commands that `clusterdb` generates and sends to the server.

-q \| --quiet
:   Do not display a response.

-t table \| --table table
:   Cluster the named table only.

-v \| --verbose
:   Print detailed information during processing.

**Connection Options**

-h host \| --host host
:   The host name of the machine on which the Greenplum master database server is running. If not specified, reads from the environment variable `PGHOST` or defaults to localhost.

-p port \| --port port
:   The TCP port on which the Greenplum master database server is listening for connections. If not specified, reads from the environment variable `PGPORT` or defaults to 5432.

-U username \| --username username
:   The database role name to connect as. If not specified, reads from the environment variable `PGUSER` or defaults to the current system role name.

-w \| --no-password
:   Never issue a password prompt. If the server requires password authentication and a password is not available by other means such as a `.pgpass` file, the connection attempt will fail. This option can be useful in batch jobs and scripts where no user is present to enter a password.

-W \| --password
:   Force a password prompt.

## Examples 

To cluster the database `test`:

```
clusterdb test
```

To cluster a single table `foo` in a database named `xyzzy`:

```
clusterdb --table foo xyzzyb
```

## See Also 

[CLUSTER](../../ref_guide/sql_commands/CLUSTER.html) in the *Greenplum Database Reference Guide*

