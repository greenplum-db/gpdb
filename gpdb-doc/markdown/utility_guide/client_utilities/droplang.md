# droplang 

Removes a procedural language.

## Synopsis 

``` {#client_util_synopsis}
droplang [<connection-option> ...] [-e] <langname> [[-d] <dbname>]

droplang [<connection-option> ...] [-e] -l <dbname>

droplang --help 

droplang --version
```

## Description 

`droplang` removes an existing programming language from a database. `droplang` can drop any procedural language, even those not supplied by the Greenplum Database distribution.

Although programming languages can be removed directly using several SQL commands, it is recommended to use `droplang` because it performs a number of checks and is much easier to use.

`droplang` is a wrapper for the SQL command `DROP LANGUAGE`.

## Options 

langname
:   Specifies the name of the programming language to be removed.

\[-d\] dbname \| \[--dbname\] dbname
:   Specifies from which database the language should be removed. The default is to use the `PGDATABASE` environment variable setting, or the same name as the current system user.

-e \| --echo
:   Echo the commands that `droplang` generates and sends to the server.

-l \| --list
:   Show a list of already installed languages in the target database.

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

To remove the language `pltcl` from the `mydatabase` database:

```
droplang pltcl mydatabase
```

## See Also 

[DROP LANGUAGE](../../ref_guide/sql_commands/DROP_LANGUAGE.html) in the *Greenplum Database Reference Guide*

