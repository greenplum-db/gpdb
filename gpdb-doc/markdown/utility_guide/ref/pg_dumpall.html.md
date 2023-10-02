# pg_dumpall 

Extracts all databases in a Greenplum Database system to a single script file or other archive file.

## <a id="section2"></a>Synopsis 

``` {#client_util_synopsis}
pg_dumpall [<connection-option> ...] [<dump_option> ...]

pg_dumpall -? | --help

pg_dumpall -V | --version
```

## <a id="section3"></a>Description 

`pg_dumpall` is a standard PostgreSQL utility for backing up all databases in a Greenplum Database \(or PostgreSQL\) instance, and is also supported in Greenplum Database. It creates a single \(non-parallel\) dump file. For routine backups of Greenplum Database it is better to use the Greenplum Database backup utility, [gpbackup](https://docs.vmware.com/en/VMware-Greenplum-Backup-and-Restore/index.html), for the best performance.

`pg_dumpall` creates a single script file that contains SQL commands that can be used as input to [psql](psql.html) to restore the databases. It does this by calling [pg\_dump](pg_dump.html) for each database. `pg_dumpall` also dumps global objects that are common to all databases. \(`pg_dump` does not save these objects.\) This currently includes information about database users and groups, and access permissions that apply to databases as a whole.

Since `pg_dumpall` reads tables from all databases you will most likely have to connect as a database superuser in order to produce a complete dump. Also you will need superuser privileges to run the saved script in order to be allowed to add users and groups, and to create databases.

The SQL script will be written to the standard output. Use the `[-f | --file]` option or shell operators to redirect it into a file.

`pg_dumpall` needs to connect several times to the Greenplum Database coordinator server \(once per database\). If you use password authentication it is likely to ask for a password each time. It is convenient to have a `~/.pgpass` file in such cases.

## <a id="section4"></a>Options 

**Dump Options**

-a \| --data-only
:   Dump only the data, not the schema \(data definitions\). This option is only meaningful for the plain-text format. For the archive formats, you may specify the option when you call [pg\_restore](pg_restore.html).

-c \| --clean
:   Output commands to clean \(drop\) database objects prior to \(the commands for\) creating them. This option is only meaningful for the plain-text format. For the archive formats, you may specify the option when you call [pg\_restore](pg_restore.html).

-E encoding \| --encoding=encoding
:   Create the dump in the specified character set encoding. By default, the dump is created in the database encoding. (An alternate method to achieve the same result is to set the `PGCLIENTENCODING` environment variable to the desired dump encoding.)

-f filename \| --file=filename
:   Send output to the specified file. If this is omitted, `pg_dump` sends the output to standard out.

-g \| --globals-only
:   Dump only global objects \(roles and tablespaces\), no databases.

-O \| --no-owner
:   Do not output commands to set ownership of objects to match the original database. By default, [pg\_dump](pg_dump.html) issues `ALTER OWNER` or `SET SESSION AUTHORIZATION` statements to set ownership of created database objects. These statements will fail when the script is run unless it is started by a superuser \(or the same user that owns all of the objects in the script\). To make a script that can be restored by any user, but will give that user ownership of all the objects, specify `-O`. This option is only meaningful for the plain-text format. For the archive formats, you may specify the option when you call [pg\_restore](pg_restore.html).

-r \| --roles-only
:   Dump only roles, not databases or tablespaces.

-s \| --schema-only
:   Dump only the object definitions \(schema\), not data.

-S username \| --superuser=username
:   Specify the superuser user name to use when deactivating triggers. This is relevant only if `--disable-triggers` is used. It is better to leave this out, and instead start the resulting script as a superuser.

    > **Note** Greenplum Database does not support user-defined triggers.

-t \| --tablespaces-only
:   Dump only tablespaces, not databases or roles.

-v \| --verbose
:   Specifies verbose mode. This will cause `[pg\_dump](pg_dump.html)` to output detailed object comments and start/stop times to the dump file, and progress messages to standard error.

-V \| --version
:   Print the `pg_dumpall` version and exit.

-x \| --no-privileges \| --no-acl
:   Prevent dumping of access privileges \(`GRANT/REVOKE` commands\).

--binary-upgrade
:   This option is for use by in-place upgrade utilities. Its use for other purposes is not recommended or supported. The behavior of the option may change in future releases without notice.

--column-inserts \| --attribute-inserts
:   Dump data as `INSERT` commands with explicit column names \(`INSERT INTO <table> (<column>, ...) VALUES ...`\). This will make restoration very slow; it is mainly useful for making dumps that can be loaded into non-PostgreSQL-based databases. Also, since this option generates a separate command for each row, an error in reloading a row causes only that row to be lost rather than the entire table contents.

--disable-dollar-quoting
:   This option deactivates the use of dollar quoting for function bodies, and forces them to be quoted using SQL standard string syntax.

--disable-triggers
:   This option is relevant only when creating a data-only dump. It instructs `pg_dumpall` to include commands to temporarily deactivate triggers on the target tables while the data is reloaded. Use this if you have triggers on the tables that you do not want to invoke during data reload. The commands emitted for `--disable-triggers` must be done as superuser. So, you should also specify a superuser name with `-S`, or preferably be careful to start the resulting script as a superuser.

    > **Note** Greenplum Database does not support user-defined triggers.

--extra-float-digits=ndigits
:   Use the specified value of `extra_float_digits` when dumping floating-point data, instead of the maximum available precision. Routine dumps made for backup purposes should not use this option.

--exclude-database=pattern
:   Do not dump databases whose name matches pattern. Multiple patterns can be excluded by writing multiple `--exclude-database` switches. The pattern parameter is interpreted as a pattern according to the same rules used by `psql`'s `\d` commands, so multiple databases can also be excluded by writing wildcard characters in the pattern. When using wildcards, be careful to quote the pattern if needed to prevent shell wildcard expansion.

--if-exists
:   Use conditional commands (for example, add an `IF EXISTS` clause) to drop databases and other objects. This option is not valid unless `--clean` is also specified.

--inserts
:   Dump data as `INSERT` commands \(rather than `COPY`\). This will make restoration very slow; it is mainly useful for making dumps that can be loaded into non-PostgreSQL-based databases. Also, since this option generates a separate command for each row, an error in reloading a row causes only that row to be lost rather than the entire table contents. Note that the restore may fail altogether if you have rearranged column order. The `--column-inserts` option is safe against column order changes, though even slower.

--load-via-partition-root
:   When dumping data for a table partition, make the `COPY` or `INSERT` statements target the root of the partitioning hierarchy that contains it, rather than the partition itself. This causes the appropriate partition to be re-determined for each row when the data is loaded. This may be useful when restoring data on a server where rows do not always fall into the same partitions as they did on the original server. That could happen, for example, if the partitioning column is of type `text` and the two systems have different definitions of the collation used to sort the partitioning column.

--lock-wait-timeout=timeout
:   Do not wait forever to acquire shared table locks at the beginning of the dump. Instead, fail if unable to lock a table within the specified timeout. The timeout may be specified in any of the formats accepted by `SET statement_timeout`. Allowed values vary depending on the server version you are dumping from, but an integer number of milliseconds is accepted by all Greenplum Database versions.

--no-comments
:   Do not dump comments.

--no-role-passwords
:   Do not dump passwords for roles. When restored, roles will have a null password, and password authentication will always fail until the password is set. Since password values are not required when this option is specified, the role information is read from the catalog view `pg_roles` instead of `pg_authid`. Consequently, this option also helps if access to `pg_authid` is restricted by some security policy.

--no-security-labels
:   Do not dump security labels.

--no-sync
:   By default, `pg_dumpall` waits for all files to be written safely to disk. Specifying this option causes `pg_dumpall` to return without waiting, which is faster, but implies that a subsequent operating system crash can leave the dump corrupt. This option is generally useful for testing, and should not be used when dumping data from a production installation.

--no-tablespaces
:   Do not output commands to create tablespaces nor select tablespaces for objects. With this option, all objects will be created in whichever tablespace is the default during restore.

--no-unlogged-table-data
:   Do not dump the contents of unlogged tables. This option has no effect on whether or not the table definitions \(schema\) are dumped; it only suppresses dumping the table data.

--on-conflict-do-nothing
:   Add `ON CONFLICT DO NOTHING` to `INSERT` commands. This option is not valid unless `--inserts` or `--column-inserts` is also specified.

--quote-all-identifiers
:   Force quoting of all identifiers. This option is recommended when dumping a database from a server whose Greenplum Database major version is different from `pg_dumpall`'s, or when the output is intended to be loaded into a server of a different major version. By default, `pg_dumpall` quotes only identifiers that are reserved words in its own major version. This sometimes results in compatibility issues when dealing with servers of other versions that may have slightly different sets of reserved words. Using `--quote-all-identifiers` prevents such issues, at the price of a harder-to-read dump script.

--resource-queues
:   Dump resource queue definitions.

--resource-groups
:   Dump resource group definitions.

--rows-per-insert=nrows
:   Dump data as `INSERT` commands (rather than `COPY)`. Controls the maximum number of rows per `INSERT` command. The value specified must be a number greater than zero. Any error during restoring will cause only rows that are part of the problematic `INSERT` to be lost, rather than the entire table contents.

--use-set-session-authorization
:   Output SQL-standard `SET SESSION AUTHORIZATION` commands instead of `ALTER OWNER` commands to determine object ownership. This makes the dump more standards compatible, but depending on the history of the objects in the dump, may not restore properly. A dump using `SET SESSION AUTHORIZATION` requires superuser privileges to restore correctly, whereas `ALTER OWNER` requires lesser privileges.

--gp-syntax
:   Output Greenplum Database syntax in the `CREATE TABLE` statements. This allows the distribution policy \(`DISTRIBUTED BY` or `DISTRIBUTED RANDOMLY` clauses\) of a Greenplum Database table to be dumped, which is useful for restoring into other Greenplum Database systems.

--no-gp-syntax
:   Do not output the table distribution clauses in the `CREATE TABLE` statements.

-? \| --help
:   Show help about `pg_dumpall` command line arguments, and exit.

**Connection Options**

-d connstr \| --dbname=connstr
:   Specifies parameters used to connect to the server, as a connection string. See [Connection Strings](https://www.postgresql.org/docs/12/libpq-connect.html#LIBPQ-CONNSTRING) in the PostgreSQL documentation for more information.

:   The option is called `--dbname` for consistency with other client applications, but because `pg_dumpall` needs to connect to many databases, the database name in the connection string will be ignored. Use the `-l` option to specify the name of the database used to dump global objects and to discover what other databases should be dumped.

-h host \| --host=host
:   The host name of the machine on which the Greenplum coordinator database server is running. If not specified, reads from the environment variable `PGHOST` or defaults to `localhost`.

-l dbname \| --database=dbname
:   Specifies the name of the database in which to connect to dump global objects. If not specified, the `postgres` database is used. If the `postgres` database does not exist, the `template1` database is used.

-p port \| --port=port
:   The TCP port on which the Greenplum coordinator database server is listening for connections. If not specified, reads from the environment variable `PGPORT` or defaults to `5432`.

-U username \| --username= username
:   The database role name to connect as. If not specified, reads from the environment variable `PGUSER` or defaults to the current system role name.

-w \| --no-password
:   Never issue a password prompt. If the server requires password authentication and a password is not available by other means such as a `.pgpass` file the connection attempt will fail. This option can be useful in batch jobs and scripts where no user is present to enter a password.

-W \| --password
:   Force a password prompt.
:   This option is never essential, since `pg_dumpall` will automatically prompt for a password if the server demands password authentication. However, `pg_dumpall` will waste a connection attempt finding out that the server wants a password. In some cases it is worth typing `-W` to avoid the extra connection attempt.
:   Note that the password prompt will occur again for each database to be dumped. It may be preferable to set up a `~/.pgpass` file rather than to rely on manual password entry.

--role=rolename
:   Specifies a role name to be used to create the dump. This option causes `pg_dumpall` to issue a `SET ROLE <rolename>` command after connecting to the database. It is useful when the authenticated user \(specified by `-U`\) lacks privileges needed by `pg_dumpall`, but can switch to a role with the required rights. Some installations have a policy against logging in directly as a superuser, and use of this option allows dumps to be made without violating the policy.

## <a id="section5"></a>Environment

PGHOST
PGOPTIONS
PGPORT
PGUSER
:   Default connection parameters.

PG_COLOR
:   Specifies whether to use color in diagnostic messages. Possible values are `always`, `auto`, and `never`.

This utility also uses the [environment variables supported by libpq](https://www.postgresql.org/docs/12/libpq-envars.html).

## <a id="section7"></a>Notes 

Since `pg_dumpall` calls [pg\_dump](pg_dump.html) internally, some diagnostic messages will refer to `pg_dump`.

The `--clean` option can be useful even when your intention is to restore the dump script into a fresh cluster. Use of `--clean` authorizes the script to drop and re-create the built-in `postgres` and `template1` databases, ensuring that those databases will retain the same properties (for instance, locale and encoding) that they had in the source cluster. Without the option, those databases will retain their existing database-level properties, as well as any pre-existing contents.

Once restored, it is wise to run `ANALYZE` on each database so the query planner has useful statistics. You can also run `vacuumdb -a -z` to vacuum and analyze all databases.

Do no expect the dump script to run completely without errors. In particular, because the script issues `CREATE ROLE` for every role existing in the source cluster, it is certain to generate a "role already exists" error for the bootstrap superuser, unless the destination cluster was initialized with a different bootstrap superuser name. This error is harmless and can be ignored. Use of the `--clean` option is likely to produce additional harmless error messages about non-existent objects; minimize those by adding `--if-exists`.

`pg_dumpall` requires all needed tablespace directories to exist before the restore; otherwise, database creation will fail for databases in non-default locations.

## <a id="section8"></a>Examples 

To dump all databases:

```
pg_dumpall > db.out
```

To reload database\(s\) from this file, you can use:

```
psql template1 -f db.out
```

To dump only global objects \(including resource queues\):

```
pg_dumpall -g --resource-queues
```

## <a id="section9"></a>See Also 

Check [pg\_dump](pg_dump.html) for details on possible error conditions.

