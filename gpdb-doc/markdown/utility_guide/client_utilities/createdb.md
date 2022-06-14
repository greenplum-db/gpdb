# createdb 

Creates a new database.

## Synopsis 

``` {#client_util_synopsis}
createdb [<connection_option> ...] [-D <tablespace>] [-E <encoding>] 
        [-O <owner>] [-T <template>] [-e] [<dbname> ['<description>']]

createdb --help 

createdb --version
```

## Description 

`createdb` creates a new database in a Greenplum Database system.

Normally, the database user who executes this command becomes the owner of the new database. However a different owner can be specified via the `-O` option, if the executing user has appropriate privileges.

`createdb` is a wrapper around the SQL command `CREATE DATABASE`.

## Options 

dbname
:   The name of the database to be created. The name must be unique among all other databases in the Greenplum system. If not specified, reads from the environment variable `PGDATABASE`, then `PGUSER` or defaults to the current system user.

description
:   A comment to be associated with the newly created database. Descriptions containing white space must be enclosed in quotes.

-D tablespace \| --tablespace tablespace
:   The default tablespace for the database.

-e echo
:   Echo the commands that createdb generates and sends to the server.

-E encoding \| --encoding encoding
:   Character set encoding to use in the new database. Specify a string constant \(such as `'UTF8'`\), an integer encoding number, or `DEFAULT` to use the default encoding. See the Greenplum Database Reference Guide for information about supported character sets.

-O owner \| --owner owner
:   The name of the database user who will own the new database. Defaults to the user executing this command.

-T template \| --template template
:   The name of the template from which to create the new database. Defaults to `template1`.

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

To create the database `test` using the default options:

```
createdb test
```

To create the database `demo` using the Greenplum master on host `gpmaster`, port `54321`, using the `LATIN1` encoding scheme:

```
createdb -p 54321 -h gpmaster -E LATIN1 demo
```

## See Also 

[CREATE DATABASE](../../ref_guide/sql_commands/CREATE_DATABASE.html) in the *Greenplum Database Reference Guide*

