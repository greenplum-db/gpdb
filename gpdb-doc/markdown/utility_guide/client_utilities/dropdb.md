# dropdb 

Removes a database.

## Synopsis 

``` {#client_util_synopsis}
dropdb [<connection_option> ...] [-e] [-i] <dbname>

dropdb --help 

dropdb --version
```

## Description 

`dropdb` destroys an existing database. The user who executes this command must be a superuser or the owner of the database being dropped.

`dropdb` is a wrapper around the SQL command `DROP DATABASE`. See the *Greenplum Database Reference Guide* for information about `DROP DATABASE.`

## Options 

dbname
:   The name of the database to be removed.

-e \| --echo
:   Echo the commands that `dropdb` generates and sends to the server.

-i \| --interactive
:   Issues a verification prompt before doing anything destructive.

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

To destroy the database named `demo` using default connection parameters:

```
dropdb demo
```

To destroy the database named `demo` using connection options, with verification, and a peek at the underlying command:

```
dropdb -p 54321 -h masterhost -i -e demo
Database "demo" will be permanently deleted.
Are you sure? (y/n) y
DROP DATABASE "demo"
DROP DATABASE
```

## See Also 

[DROP DATABASE](../../ref_guide/sql_commands/DROP_DATABASE.html) in the *Greenplum Database Reference Guide*

