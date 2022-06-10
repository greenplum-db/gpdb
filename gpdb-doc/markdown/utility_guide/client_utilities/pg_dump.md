# pg\_dump 

Extracts a database into a single script file or other archive file.

## Synopsis 

``` {#client_util_synopsis}
pg_dump [<connection_option> ...] [<dump_option> ...] <dbname>
```

## Description 

`pg_dump` is a standard PostgreSQL utility for backing up a database, and is also supported in Greenplum Database. It creates a single \(non-parallel\) dump file. For routine backups of Greenplum Database, it is better to use the Greenplum Database backup utility, [gpcrondump](../admin_utilities/gpcrondump.html), for the best performance.

Use `pg_dump` if you are migrating your data to another database vendor's system, or to another Greenplum Database system with a different segment configuration \(for example, if the system you are migrating to has greater or fewer segment instances\). To restore, you must use the corresponding [pg\_restore](pg_restore.html) utility \(if the dump file is in archive format\), or you can use a client program such as [psql](psql.html) \(if the dump file is in plain text format\).

Since `pg_dump` is compatible with regular PostgreSQL, it can be used to migrate data into Greenplum Database. The `pg_dump` utility in Greenplum Database is very similar to the PostgreSQL `pg_dump` utility, with the following exceptions and limitations:

-   If using `pg_dump` to backup a Greenplum Database database, keep in mind that the dump operation can take a long time \(several hours\) for very large databases. Also, you must make sure you have sufficient disk space to create the dump file.
-   If you are migrating data from one Greenplum Database system to another, use the `--gp-syntax` command-line option to include the `DISTRIBUTED BY` clause in `CREATE TABLE` statements. This ensures that Greenplum Database table data is distributed with the correct distribution key columns upon restore.

`pg_dump` makes consistent backups even if the database is being used concurrently. `pg_dump` does not block other users accessing the database \(readers or writers\).

When used with one of the archive file formats and combined with `pg_restore`, `pg_dump` provides a flexible archival and transfer mechanism. `pg_dump` can be used to backup an entire database, then `pg_restore`can be used to examine the archive and/or select which parts of the database are to be restored. The most flexible output file format is the custom format \(`-Fc`\). It allows for selection and reordering of all archived items, and is compressed by default. The tar format \(`-Ft`\) is not compressed and it is not possible to reorder data when loading, but it is otherwise quite flexible. It can be manipulated with standard UNIX tools such as `tar`.

**Note:** The `--ignore-version` option is deprecated and will be removed in a future release.

## Options 

dbname
:   Specifies the name of the database to be dumped. If this is not specified, the environment variable `PGDATABASE` is used. If that is not set, the user name specified for the connection is used.

**Dump Options**

-a \| --data-only
:   Dump only the data, not the schema \(data definitions\). This option is only meaningful for the plain-text format. For the archive formats, you may specify the option when you call [pg\_restore](pg_restore.html).

-b \| --blobs
:   Include large objects in the dump. This is the default behavior except when `--schema`, `--table`, or `--schema-only` is specified, so the `-b` switch is only useful to add large objects to selective dumps.

-c \| --clean
:   Adds commands to the text output file to clean \(drop\) database objects prior to \(the commands for\) creating them. Note that objects are not dropped before the dump operation begins, but `DROP` commands are added to the DDL dump output files so that when you use those files to do a restore, the `DROP` commands are run prior to the `CREATE` commands. This option is only meaningful for the plain-text format. For the archive formats, you may specify the option when you call [pg\_restore](pg_restore.html).

-C \| --create
:   Begin the output with a command to create the database itself and reconnect to the created database. \(With a script of this form, it doesn't matter which database you connect to before running the script.\) This option is only meaningful for the plain-text format. For the archive formats, you may specify the option when you call [pg\_restore](pg_restore.html).

-d \| --inserts
:   Dump data as `INSERT` commands \(rather than `COPY`\). This will make restoration very slow; it is mainly useful for making dumps that can be loaded into non-PostgreSQL-based databases. Also, since this option generates a separate command for each row, an error in reloading a row causes only that row to be lost rather than the entire table contents. Note that the restore may fail altogether if you have rearranged column order. The `-D` option is safe against column order changes, though even slower.

-D \| --column-inserts \| --attribute-inserts
:   Dump data as `INSERT` commands with explicit column names `(INSERT INTO`table`(`column`, ...) VALUES ...)`. This will make restoration very slow; it is mainly useful for making dumps that can be loaded into non-PostgreSQL-based databases. Also, since this option generates a separate command for each row, an error in reloading a row causes only that row to be lost rather than the entire table contents.

-E encoding \| --encoding=encoding
:   Create the dump in the specified character set encoding. By default, the dump is created in the database encoding. \(Another way to get the same result is to set the `PGCLIENTENCODING` environment variable to the desired dump encoding.\)

-f file \| --file=file
:   Send output to the specified file. If this is omitted, the standard output is used.

-F p\|c\|t \| --format=plain\|custom\|tar
:   Selects the format of the output. format can be one of the following:

:   p \| plain — Output a plain-text SQL script file \(the default\).

:   c \| custom — Output a custom archive suitable for input into [pg\_restore](pg_restore.html). This is the most flexible format in that it allows reordering of loading data as well as object definitions. This format is also compressed by default.

:   t \| tar — Output a tar archive suitable for input into [pg\_restore](pg_restore.html). Using this archive format allows reordering and/or exclusion of database objects at the time the database is restored. It is also possible to limit which data is reloaded at restore time.

-i \| --ignore-version
:   **Note:** This option is deprecated and will be removed in a future release.

    Ignore version mismatch between `pg_dump` and the database server. `pg_dump` can dump from servers running previous releases of Greenplum Database \(or PostgreSQL\), but very old versions may not be supported anymore. Use this option if you need to override the version check.

-n schema \| --schema=schema
:   Dump only schemas matching the schema pattern; this selects both the schema itself, and all its contained objects. When this option is not specified, all non-system schemas in the target database will be dumped. Multiple schemas can be selected by writing multiple `-n` switches. Also, the schema parameter is interpreted as a pattern according to the same rules used by `psql`'s`\d` commands, so multiple schemas can also be selected by writing wildcard characters in the pattern. When using wildcards, be careful to quote the pattern if needed to prevent the shell from expanding the wildcards.

:   Note: When -n is specified, `pg_dump` makes no attempt to dump any other database objects that the selected schema\(s\) may depend upon. Therefore, there is no guarantee that the results of a specific-schema dump can be successfully restored by themselves into a clean database.

    **Note:** Non-schema objects such as blobs are not dumped when `-n` is specified. You can add blobs back to the dump with the `--blobs` switch.

-N schema \| --exclude-schema=schema
:   Do not dump any schemas matching the schema pattern. The pattern is interpreted according to the same rules as for `-n`. `-N` can be given more than once to exclude schemas matching any of several patterns. When both `-n` and `-N` are given, the behavior is to dump just the schemas that match at least one `-n` switch but no `-N` switches. If `-N` appears without `-n`, then schemas matching `-N` are excluded from what is otherwise a normal dump.

-o \| --oids
:   Dump object identifiers \(OIDs\) as part of the data for every table. Use of this option is not recommended for files that are intended to be restored into Greenplum Database.

-O \| --no-owner
:   Do not output commands to set ownership of objects to match the original database. By default, `pg_dump` issues `ALTER OWNER` or `SET SESSION AUTHORIZATION` statements to set ownership of created database objects. These statements will fail when the script is run unless it is started by a superuser \(or the same user that owns all of the objects in the script\). To make a script that can be restored by any user, but will give that user ownership of all the objects, specify `-O`. This option is only meaningful for the plain-text format. For the archive formats, you may specify the option when you call [pg\_restore](pg_restore.html).

-s \| --schema-only
:   Dump only the object definitions \(schema\), not data.

-S username \| --superuser=username
:   Specify the superuser user name to use when disabling triggers. This is only relevant if `--disable-triggers` is used. It is better to leave this out, and instead start the resulting script as a superuser.

    **Note:** Greenplum Database does not support user-defined triggers.

-t table \| --table=table
:   Dump only tables \(or views or sequences\) matching the table pattern. Specify the table in the format `schema.table`.

:   Multiple tables can be selected by writing multiple `-t` switches. Also, the table parameter is interpreted as a pattern according to the same rules used by `psql`'s `\d` commands, so multiple tables can also be selected by writing wildcard characters in the pattern. When using wildcards, be careful to quote the pattern if needed to prevent the shell from expanding the wildcards. The `-n` and `-N` switches have no effect when `-t` is used, because tables selected by `-t` will be dumped regardless of those switches, and non-table objects will not be dumped.

    **Note:** When `-t` is specified, `pg_dump` makes no attempt to dump any other database objects that the selected table\(s\) may depend upon. Therefore, there is no guarantee that the results of a specific-table dump can be successfully restored by themselves into a clean database.

    Also, `-t` cannot be used to specify a child table partition. To dump a partitioned table, you must specify the parent table name.

-T table \| --exclude-table=table
:   Do not dump any tables matching the table pattern. The pattern is interpreted according to the same rules as for `-t`. `-T` can be given more than once to exclude tables matching any of several patterns. When both `-t` and `-T` are given, the behavior is to dump just the tables that match at least one `-t` switch but no `-T` switches. If `-T` appears without `-t`, then tables matching `-T` are excluded from what is otherwise a normal dump.

-v \| --verbose
:   Specifies verbose mode. This will cause `pg_dump` to output detailed object comments and start/stop times to the dump file, and progress messages to standard error.

-x \| --no-privileges \| --no-acl
:   Prevent dumping of access privileges \(`GRANT/REVOKE` commands\).

--disable-dollar-quoting
:   This option disables the use of dollar quoting for function bodies, and forces them to be quoted using SQL standard string syntax.

--disable-triggers
:   This option is only relevant when creating a data-only dump. It instructs `pg_dump` to include commands to temporarily disable triggers on the target tables while the data is reloaded. Use this if you have triggers on the tables that you do not want to invoke during data reload. The commands emitted for `--disable-triggers` must be done as superuser. So, you should also specify a superuser name with `-S`, or preferably be careful to start the resulting script as a superuser. This option is only meaningful for the plain-text format. For the archive formats, you may specify the option when you call [pg\_restore](pg_restore.html).

    **Note:** Greenplum Database does not support user-defined triggers.

--use-set-session-authorization
:   Output SQL-standard `SET SESSION AUTHORIZATION` commands instead of `ALTER OWNER` commands to determine object ownership. This makes the dump more standards compatible, but depending on the history of the objects in the dump, may not restore properly. A dump using `SET SESSION AUTHORIZATION` will require superuser privileges to restore correctly, whereas `ALTER OWNER` requires lesser privileges.

--gp-syntax \| --no-gp-syntax
:   Use `--gp-syntax` to dump Greenplum Database syntax in the `CREATE TABLE` statements. This allows the distribution policy \(`DISTRIBUTED BY` or `DISTRIBUTED RANDOMLY` clauses\) of a Greenplum Database table to be dumped, which is useful for restoring into other Greenplum Database systems. The default is to include Greenplum Database syntax when connected to a Greenplum Database system, and to exclude it when connected to a regular PostgreSQL system.

--function-oids oids
:   Dump the function\(s\) specified in the oids list of object identifiers.

    **Note:** This option is provided solely for use by other administration utilities; its use for any other purpose is not recommended or supported. The behaviour of the option may change in future releases without notice.

--relation-oids oids
:   Dump the relation\(s\) specified in the oids list of object identifiers.

    **Note:** This option is provided solely for use by other administration utilities; its use for any other purpose is not recommended or supported. The behaviour of the option may change in future releases without notice.

-Z 0..9 \| --compress=0..9
:   Specify the compression level to use in archive formats that support compression. Currently only the custom archive format supports compression.

**Connection Options**

-h host\| --host host
:   The host name of the machine on which the Greenplum Database master database server is running. If not specified, reads from the environment variable `PGHOST` or defaults to localhost.

-p port\| --port port
:   The TCP port on which the Greenplum Database master database server is listening for connections. If not specified, reads from the environment variable `PGPORT` or defaults to 5432.

-U username\| --username username
:   The database role name to connect as. If not specified, reads from the environment variable `PGUSER` or defaults to the current system role name.

-W \| --password
:   Force a password prompt.

## Notes 

When a data-only dump is chosen and the option `--disable-triggers` is used, `pg_dump` emits commands to disable triggers on user tables before inserting the data and commands to re-enable them after the data has been inserted. If the restore is stopped in the middle, the system catalogs may be left in the wrong state.

Members of `tar` archives are limited to a size less than 8 GB. \(This is an inherent limitation of the `tar` file format.\) Therefore this format cannot be used if the textual representation of any one table exceeds that size. The total size of a tar archive and any of the other output formats is not limited, except possibly by the operating system.

The dump file produced by `pg_dump` does not contain the statistics used by the optimizer to make query planning decisions. Therefore, it is wise to run `ANALYZE` after restoring from a dump file to ensure good performance.

## Examples 

Dump a database called `mydb` into a SQL-script file:

```
pg_dump mydb > db.sql
```

To reload such a script into a \(freshly created\) database named `newdb`:

```
psql -d newdb -f db.sql
```

Dump a Greenplum Database in tar file format and include distribution policy information:

```
pg_dump -Ft --gp-syntax mydb > db.tar
```

To dump a database into a custom-format archive file:

```
pg_dump -Fc mydb > db.dump
```

To reload an archive file into a \(freshly created\) database named `newdb`:

```
pg_restore -d newdb db.dump
```

To dump a single table named `mytab`:

```
pg_dump -t mytab mydb > db.sql
```

To specify an upper-case or mixed-case name in `-t` and related switches, you need to double-quote the name; else it will be folded to lower case. But double quotes are special to the shell, so in turn they must be quoted. Thus, to dump a single table with a mixed-case name, you need something like:

```
pg_dump -t '"MixedCaseName"' mydb > mytab.sql
```

## See Also 

[pg\_dumpall](pg_dumpall.html), [pg\_restore](pg_restore.html), [psql](psql.html)

