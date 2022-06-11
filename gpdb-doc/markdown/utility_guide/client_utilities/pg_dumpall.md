# pg\_dumpall 

Extracts all databases in a Greenplum Database system to a single script file or other archive file.

## Synopsis 

``` {#client_util_synopsis}
pg_dumpall [<connection_option> ...] [<dump_option> ...]
```

## Description 

`pg_dumpall` is a standard PostgreSQL utility for backing up all databases in a Greenplum Database \(or PostgreSQL\) instance, and is also supported in Greenplum Database. It creates a single \(non-parallel\) dump file. For routine backups of Greenplum Database it is better to use the Greenplum Database backup utility, [gpcrondump](../admin_utilities/gpcrondump.html), for the best performance.

`pg_dumpall` creates a single script file that contains SQL commands that can be used as input to [psql](psql.html) to restore the databases. It does this by calling [pg\_dump](pg_dump.html) for each database. `pg_dumpall` also dumps global objects that are common to all databases. \(`pg_dump` does not save these objects.\) This currently includes information about database users and groups, and access permissions that apply to databases as a whole.

Since `pg_dumpall` reads tables from all databases you will most likely have to connect as a database superuser in order to produce a complete dump. Also you will need superuser privileges to execute the saved script in order to be allowed to add users and groups, and to create databases.

The SQL script will be written to the standard output. Shell operators should be used to redirect it into a file.

`pg_dumpall` needs to connect several times to the Greenplum Database master server \(once per database\). If you use password authentication it is likely to ask for a password each time. It is convenient to have a `~/.pgpass` file in such cases.

**Note:** The `--ignore-version` option is deprecated and will be removed in a future release.

## Options 

**Dump Options**

-a \| --data-only
:   Dump only the data, not the schema \(data definitions\). This option is only meaningful for the plain-text format. For the archive formats, you may specify the option when you call [pg\_restore](pg_restore.html).

-c \| --clean
:   Output commands to clean \(drop\) database objects prior to \(the commands for\) creating them. This option is only meaningful for the plain-text format. For the archive formats, you may specify the option when you call [pg\_restore](pg_restore.html).

-d \| --inserts
:   Dump data as `INSERT` commands \(rather than `COPY`\). This will make restoration very slow; it is mainly useful for making dumps that can be loaded into non-PostgreSQL-based databases. Also, since this option generates a separate command for each row, an error in reloading a row causes only that row to be lost rather than the entire table contents. Note that the restore may fail altogether if you have rearranged column order. The `-D` option is safe against column order changes, though even slower.

-D \| --column-inserts \| --attribute-inserts
:   Dump data as `INSERT` commands with explicit column names `(INSERT INTO table (column, ...) VALUES ...)`. This will make restoration very slow; it is mainly useful for making dumps that can be loaded into non-PostgreSQL-based databases. Also, since this option generates a separate command for each row, an error in reloading a row causes only that row to be lost rather than the entire table contents.

-F \| --filespaces
:   Dump filespace definitions.

-f filename \| --file=filename
:   Send output to the specified file.

-g \| --globals-only
:   Dump only global objects \(roles and tablespaces\), no databases.

-i \| --ignore-version
:   **Note:** This option is deprecated and will be removed in a future release.

    Ignore version mismatch between [pg\_dump](pg_dump.html) and the database server. `pg_dump` can dump from servers running previous releases of Greenplum Database \(or PostgreSQL\), but very old versions may not be supported anymore. Use this option if you need to override the version check.

-o \| --oids
:   Dump object identifiers \(OIDs\) as part of the data for every table. Use of this option is not recommended for files that are intended to be restored into Greenplum Database.

-O \| --no-owner
:   Do not output commands to set ownership of objects to match the original database. By default, [pg\_dump](pg_dump.html) issues `ALTER OWNER` or `SET SESSION AUTHORIZATION` statements to set ownership of created database objects. These statements will fail when the script is run unless it is started by a superuser \(or the same user that owns all of the objects in the script\). To make a script that can be restored by any user, but will give that user ownership of all the objects, specify `-O`. This option is only meaningful for the plain-text format. For the archive formats, you may specify the option when you call [pg\_restore](pg_restore.html).

-s \| --schema-only
:   Dump only the object definitions \(schema\), not data.

-S username \| --superuser=username
:   Specify the superuser user name to use when disabling triggers. This is only relevant if `--disable-triggers` is used. It is better to leave this out, and instead start the resulting script as a superuser.

    **Note:** Greenplum Database does not support user-defined triggers.

-t \| --tablespaces-only
:   Dump only tablespaces, not databases or roles.

-v \| --verbose
:   Specifies verbose mode. This will cause [pg\_dump](pg_dump.html) to output detailed object comments and start/stop times to the dump file, and progress messages to standard error.

-x \| --no-privileges \| --no-acl
:   Prevent dumping of access privileges \(`GRANT/REVOKE` commands\).

--disable-dollar-quoting
:   This option disables the use of dollar quoting for function bodies, and forces them to be quoted using SQL standard string syntax.

--disable-triggers
:   This option is only relevant when creating a data-only dump. It instructs `pg_dumpall` to include commands to temporarily disable triggers on the target tables while the data is reloaded. Use this if you have triggers on the tables that you do not want to invoke during data reload. The commands emitted for `--disable-triggers` must be done as superuser. So, you should also specify a superuser name with `-S`, or preferably be careful to start the resulting script as a superuser.

    **Note:** Greenplum Database does not support user-defined triggers.

--resource-queues
:   Dump resource queue definitions.

--resource-groups
:   Dump resource group definitions.

--roles-only
:   Dump only roles, not databases, tablespaces, or filespaces.

--use-set-session-authorization
:   Output SQL-standard `SET SESSION AUTHORIZATION` commands instead of `ALTER OWNER` commands to determine object ownership. This makes the dump more standards compatible, but depending on the history of the objects in the dump, may not restore properly. A dump using `SET SESSION AUTHORIZATION` will require superuser privileges to restore correctly, whereas `ALTER OWNER` requires lesser privileges.

--gp-syntax
:   Output Greenplum Database syntax in the `CREATE TABLE` statements. This allows the distribution policy \(`DISTRIBUTED BY` or `DISTRIBUTED RANDOMLY` clauses\) of a Greenplum Database table to be dumped, which is useful for restoring into other Greenplum Database systems.

--no-gp-syntax
:   Do not output the table distribution clauses in the `CREATE TABLE` statements.

**Connection Options**

-h host \| --host=host
:   The host name of the machine on which the Greenplum master database server is running. If not specified, reads from the environment variable `PGHOST` or defaults to `localhost`.

-l dbname \| --database=dbname
:   Specifies the name of the database in which to connect to dump global objects. If not specified, the `postgres` database is used. If the `postgres` database does not exist, the `template1` database is used.

-p port \| --port=port
:   The TCP port on which the Greenplum master database server is listening for connections. If not specified, reads from the environment variable `PGPORT` or defaults to 5432.

-U username \| --username= username
:   The database role name to connect as. If not specified, reads from the environment variable `PGUSER` or defaults to the current system role name.

-w
:   Never prompt for a password.

-W \| --password
:   Force a password prompt.

## Notes 

Since `pg_dumpall` calls [pg\_dump](pg_dump.html) internally, some diagnostic messages will refer to `pg_dump`.

Once restored, it is wise to run `ANALYZE` on each database so the query planner has useful statistics. You can also run `vacuumdb -a -z` to analyze all databases.

`pg_dumpall` requires all needed tablespace \(filespace\) directories to exist before the restore or database creation will fail for databases in non-default locations.

## Examples 

To dump all databases:

```
pg_dumpall > db.out
```

To reload this file:

```
psql template1 -f db.out
```

To dump only global objects \(including filespaces and resource queues\):

```
pg_dumpall -g -F --resource-queues
```

## See Also 

[pg\_dump](pg_dump.html)

