# createlang 

Defines a new procedural language for a database.

## Synopsis 

``` {#client_util_synopsis}
createlang [<connection_option> ...] [-e] <langname> [[-d] <dbname>]

createlang [<connection-option> ...] -l <dbname>

createlang --help 

createlang --version
```

## Description 

The `createlang` utility adds a new programming language to a database. `createlang` is a wrapper around the `SQL` command [CREATE EXTENSION](../../ref_guide/sql_commands/CREATE_EXTENSION.html).

**Note:** `createlang` is deprecated and may be removed in a future release. Direct use of the `CREATE EXTENSION` command is recommended instead.

The procedural language packages included in the standard Greenplum Database distribution are:

-   `PL/pgSQL`
-   `PL/Perl`
-   `PL/Python`

The `PL/pgSQL` language is registered in all databases by default.

Greenplum Database also has language handlers for `PL/Java` and `PL/R`, but those languages are not pre-installed with Greenplum Database. See the [Procedural Languages](https://www.postgresql.org/docs/8.3/static/xplang.html) section in the PostgreSQL documentation for more information.

## Options 

langname
:   Specifies the name of the procedural programming language to be defined.

\[-d\] dbname \| \[--dbname\] dbname
:   Specifies to which database the language should be added. The default is to use the `PGDATABASE` environment variable setting, or the same name as the current system user.

-e \| --echo
:   Echo the commands that `createlang` generates and sends to the server.

-l dbname \| --list dbname
:   Show a list of already installed languages in the target database.

**Connection Options**

-h host \| --host host
:   The host name of the machine on which the Greenplum master database server is running. If not specified, reads from the environment variable `PGHOST` or defaults to localhost.

-p port \| --port port
:   The TCP port on which the Greenplum master database server is listening for connections. If not specified, reads from the environment variable `PGPORT` or defaults to `5432`.

-U username \| --username username
:   The database role name to connect as. If not specified, reads from the environment variable `PGUSER` or defaults to the current system role name.

-w \| --no-password
:   Never issue a password prompt. If the server requires password authentication and a password is not available by other means such as a `.pgpass` file, the connection attempt will fail. This option can be useful in batch jobs and scripts where no user is present to enter a password.

-W \| --password
:   Force a password prompt.

## Examples 

To install the language `plperl` into the database `mytestdb`:

```
createlang plperl mytestdb
```

## See Also 

[CREATE LANGUAGE](../../ref_guide/sql_commands/CREATE_LANGUAGE.html), [DROP LANGUAGE](../../ref_guide/sql_commands/DROP_LANGUAGE.html) in the *Greenplum Database Reference Guide*

