# reindexdb 

Rebuilds indexes in a database.

## Synopsis 

``` {#client_util_synopsis}
reindexdb [<connection-option> ...] [--table | -t <table> ] 
        [--index | -i <index> ] [<dbname>]

reindexdb [<connection-option> ...] [--all | -a]

reindexdb [<connection-option> ...] [--system | -s] [<dbname>]

reindexdb --help 

reindexdb --version
```

## Description 

`reindexdb` is a utility for rebuilding indexes in Greenplum Database, and is a wrapper around the SQL command `REINDEX`.

## Options 

-a \| --all
:   Reindex all databases.

\[-d\] dbname \| \[--dbname\] dbname
:   Specifies the name of the database to be reindexed. If this is not specified and `-all` is not used, the database name is read from the environment variable `PGDATABASE`. If that is not set, the user name specified for the connection is used.

-e \| --echo
:   Echo the commands that `reindexdb` generates and sends to the server.

-i index \| --index index
:   Recreate index only.

-q \| --quiet
:   Do not display a response.

-s \| --system
:   Reindex system catalogs.

-t table \| --table table
:   Reindex table only.

**Connection Options**

-h host \| --host host
:   Specifies the host name of the machine on which the Greenplum master database server is running. If not specified, reads from the environment variable `PGHOST` or defaults to localhost.

-p port \| --port port
:   Specifies the TCP port on which the Greenplum master database server is listening for connections. If not specified, reads from the environment variable `PGPORT` or defaults to 5432.

-U username \| --username username
:   The database role name to connect as. If not specified, reads from the environment variable `PGUSER` or defaults to the current system user name.

-w \| --no-password
:   Never issue a password prompt. If the server requires password authentication and a password is not available by other means such as a `.pgpass` file, the connection attempt will fail. This option can be useful in batch jobs and scripts where no user is present to enter a password.

-W \| --password
:   Force a password prompt.

## Notes 

`reindexdb` might need to connect several times to the master server, asking for a password each time. It is convenient to have a `~/.pgpass` file in such cases.

## Examples 

To reindex the database `mydb`:

```
reindexdb mydb
```

To reindex the table `foo` and the index `bar` in a database named `abcd`:

```
reindexdb --table foo --index bar abcd
```

## See Also 

[REINDEX](../../ref_guide/sql_commands/REINDEX.html) in the *Greenplum Database Reference Guide*

