# pg_dump 

Extracts a database into a single script file or other archive file.

## <a id="section2"></a>Synopsis 

``` {#client_util_synopsis}
pg_dump [<connection-option> ...] [<dump_option> ...] [<dbname>]

pg_dump -? | --help

pg_dump -V | --version
```

## <a id="section3"></a>Description 

`pg_dump` is a standard PostgreSQL utility for backing up a database, and is also supported in Greenplum Database. It creates a single \(non-parallel\) dump file. For routine backups of Greenplum Database, it is better to use the Greenplum Database backup utility, [gpbackup](https://docs.vmware.com/en/VMware-Tanzu-Greenplum-Backup-and-Restore/index.html), for the best performance.

Use `pg_dump` if you are migrating your data to another database vendor's system, or to another Greenplum Database system with a different segment configuration \(for example, if the system you are migrating to has greater or fewer segment instances\). To restore, you must use the corresponding [pg\_restore](pg_restore.html) utility \(if the dump file is in archive format\), or you can use a client program such as [psql](psql.html) \(if the dump file is in plain text format\).

Since `pg_dump` is compatible with regular PostgreSQL, it can be used to migrate data into Greenplum Database. The `pg_dump` utility in Greenplum Database is very similar to the PostgreSQL `pg_dump` utility, with the following exceptions and limitations:

-   If using `pg_dump` to backup a Greenplum Database database, keep in mind that the dump operation can take a long time \(several hours\) for very large databases. Also, you must make sure you have sufficient disk space to create the dump file.
-   If you are migrating data from one Greenplum Database system to another, use the `--gp-syntax` command-line option to include the `DISTRIBUTED BY` clause in `CREATE TABLE` statements. This ensures that Greenplum Database table data is distributed with the correct distribution key columns upon restore.

`pg_dump` makes consistent backups even if the database is being used concurrently. `pg_dump` does not block other users accessing the database \(readers or writers\).

When used with one of the archive file formats and combined with `pg_restore`, `pg_dump` provides a flexible archival and transfer mechanism. `pg_dump` can be used to backup an entire database, then `pg_restore`can be used to examine the archive and/or select which parts of the database are to be restored. The most flexible output file formats are the custom format \(`-Fc`\) and the directory format \(`-Fd`\). They allow for selection and reordering of all archived items, support parallel restoration, and are compressed by default. The directory format is the only format that supports parallel dumps.

## <a id="section4"></a>Options 

dbname
:   Specifies the name of the database to be dumped. If this is not specified, the environment variable `PGDATABASE` is used. If that is not set, the user name specified for the connection is used.

**Dump Options**

-a \| --data-only
:   Dump only the data, not the schema \(data definitions\). Table data and sequence values are dumped.

:   This option is similar to, but for historical reasons not identical to, specifying `--section=data`.

-b \| --blobs
:   Include large objects in the dump. This is the default behavior except when `--schema`, `--table`, or `--schema-only` is specified. The `-b` switch is only useful add large objects to dumps where a specific schema or table has been requested. Note that blobs are considered data and therefore will be included when `--data-only` is used, but not when `--schema-only` is.

    > **Note** Greenplum Database does not support the PostgreSQL [large object facility](https://www.postgresql.org/docs/12/largeobjects.html) for streaming user data that is stored in large-object structures.

-c \| --clean
:   Adds commands to the text output file to clean \(drop\) database objects prior to outputting the commands for creating them. \(Restore might generate some harmless error messages, if any objects were not present in the destination database.\) Note that objects are not dropped before the dump operation begins, but `DROP` commands are added to the DDL dump output files so that when you use those files to do a restore, the `DROP` commands are run prior to the `CREATE` commands. This option is only meaningful for the plain-text format. For the archive formats, you may specify the option when you call [pg\_restore](pg_restore.html).

-C \| --create
:   Begin the output with a command to create the database itself and reconnect to the created database. \(With a script of this form, it doesn't matter which database in the destination installation you connect to before running the script.\) If `--clean` is also specified, the script drops and recreates the target database before reconnecting to it. This option is only meaningful for the plain-text format. For the archive formats, you may specify the option when you call [pg\_restore](pg_restore.html).

-E encoding \| --encoding=encoding
:   Create the dump in the specified character set encoding. By default, the dump is created in the database encoding. \(Another way to get the same result is to set the `PGCLIENTENCODING` environment variable to the desired dump encoding.\)

-f file \| --file=file
:   Send output to the specified file. This parameter can be omitted for file-based output formats, in which case the standard output is used. It must be given for the directory output format however, where it specifies the target directory instead of a file. In this case the directory is created by `pg_dump` and must not exist before.

-F p\|c\|d\|t \| --format=plain\|custom\|directory\|tar
:   Selects the format of the output. format can be one of the following:

:   p \| plain — Output a plain-text SQL script file \(the default\).

:   c \| custom — Output a custom archive suitable for input into [pg\_restore](pg_restore.html). Together with the directory output format, this is the most flexible output format in that it allows manual selection and reordering of archived items during restore. This format is compressed by default and also supports parallel dumps.

:   d \| directory — Output a directory-format archive suitable for input into `pg_restore`. This will create a directory with one file for each table and blob being dumped, plus a so-called Table of Contents file describing the dumped objects in a machine-readable format that `pg_restore` can read. A directory format archive can be manipulated with standard Unix tools; for example, files in an uncompressed archive can be compressed with the `gzip` tool. This format is compressed by default.

:   t \| tar — Output a tar-format archive suitable for input into [pg\_restore](pg_restore.html). The tar format is compatible with the directory format; extracting a tar-format archive produces a valid directory-format archive. However, the tar format does not support compression. Also, when using tar format the relative order of table data items cannot be changed during restore.

-j njobs \| --jobs=njobs
:   Run the dump in parallel by dumping njobs tables simultaneously. This option reduces the time of the dump but it also increases the load on the database server. You can only use this option with the directory output format because this is the only output format where multiple processes can write their data at the same time.

:   > **Note** Parallel dumps using `pg_dump` are parallelized only on the query dispatcher \(coordinator\) node, not across the query executor \(segment\) nodes as is the case when you use `gpbackup`.

:   `pg_dump` will open njobs + 1 connections to the database, so make sure your [max\_connections](../../ref_guide/config_params/guc-list.html) setting is high enough to accommodate all connections.

:   Requesting exclusive locks on database objects while running a parallel dump could cause the dump to fail. The reason is that the `pg_dump` coordinator process requests shared locks on the objects that the worker processes are going to dump later in order to make sure that nobody deletes them and makes them go away while the dump is running. If another client then requests an exclusive lock on a table, that lock will not be granted but will be queued waiting for the shared lock of the coordinator process to be released. Consequently, any other access to the table will not be granted either and will queue after the exclusive lock request. This includes the worker process trying to dump the table. Without any precautions this would be a classic deadlock situation. To detect this conflict, the `pg_dump` worker process requests another shared lock using the `NOWAIT` option. If the worker process is not granted this shared lock, somebody else must have requested an exclusive lock in the meantime and there is no way to continue with the dump, so `pg_dump` has no choice but to cancel the dump.

:   For a consistent backup, the database server needs to support synchronized snapshots, a feature that was introduced in Greenplum Database 6.0. With this feature, database clients can ensure they see the same data set even though they use different connections. `pg_dump -j` uses multiple database connections; it connects to the database once with the coordinator process and once again for each worker job. Without the synchronized snapshot feature, the different worker jobs wouldn't be guaranteed to see the same data in each connection, which could lead to an inconsistent backup.

:   If you want to run a parallel dump of a pre-6.0 server, you need to make sure that the database content doesn't change from between the time the coordinator connects to the database until the last worker job has connected to the database. The easiest way to do this is to halt any data modifying processes \(DDL and DML\) accessing the database before starting the backup. You also need to specify the `--no-synchronized-snapshots` parameter when running `pg_dump -j` against a pre-6.0 Greenplum Database server.

-n schema \| --schema=schema
:   Dump only schemas matching the schema pattern; this selects both the schema itself, and all its contained objects. When this option is not specified, all non-system schemas in the target database will be dumped. Multiple schemas can be selected by writing multiple `-n` switches. Also, the schema parameter is interpreted as a pattern according to the same rules used by `psql`'s`\d` commands, so multiple schemas can also be selected by writing wildcard characters in the pattern. When using wildcards, be careful to quote the pattern if needed to prevent the shell from expanding the wildcards.

:   Note: When -n is specified, `pg_dump` makes no attempt to dump any other database objects that the selected schema\(s\) may depend upon. Therefore, there is no guarantee that the results of a specific-schema dump can be successfully restored by themselves into a clean database.

    > **Note** Non-schema objects such as blobs are not dumped when `-n` is specified. You can add blobs back to the dump with the `--blobs` switch.

-N schema \| --exclude-schema=schema
:   Do not dump any schemas matching the schema pattern. The pattern is interpreted according to the same rules as for `-n`. `-N` can be given more than once to exclude schemas matching any of several patterns. When both `-n` and `-N` are given, the behavior is to dump just the schemas that match at least one `-n` switch but no `-N` switches. If `-N` appears without `-n`, then schemas matching `-N` are excluded from what is otherwise a normal dump.

-o \| --oids
:   Dump object identifiers \(OIDs\) as part of the data for every table. Use of this option is not recommended for files that are intended to be restored into Greenplum Database.

-O \| --no-owner
:   Do not output commands to set ownership of objects to match the original database. By default, `pg_dump` issues `ALTER OWNER` or `SET SESSION AUTHORIZATION` statements to set ownership of created database objects. These statements will fail when the script is run unless it is started by a superuser \(or the same user that owns all of the objects in the script\). To make a script that can be restored by any user, but will give that user ownership of all the objects, specify `-O`. This option is only meaningful for the plain-text format. For the archive formats, you may specify the option when you call [pg\_restore](pg_restore.html).

-s \| --schema-only
:   Dump only the object definitions \(schema\), not data.

:   This option is the inverse of `--data-only`. It is similar to, but for historical reasons not identical to, specifying `--section=pre-data --section=post-data`.

:   \(Do not confuse this with the `--schema` option, which uses the word "schema" in a different meaning.\)

:   To exclude table data for only a subset of tables in the database, see `--exclude-table-data`.

-S username \| --superuser=username
:   Specify the superuser user name to use when deactivating triggers. This is relevant only if `--disable-triggers` is used. It is better to leave this out, and instead start the resulting script as a superuser.

    > **Note** Greenplum Database does not support user-defined triggers.

-t table \| --table=table
:   Dump only tables \(or views or sequences or foreign tables\) matching the table pattern. Specify the table in the format `schema.table`.

:   Multiple tables can be selected by writing multiple `-t` switches. Also, the table parameter is interpreted as a pattern according to the same rules used by `psql`'s `\d` commands, so multiple tables can also be selected by writing wildcard characters in the pattern. When using wildcards, be careful to quote the pattern if needed to prevent the shell from expanding the wildcards. The `-n` and `-N` switches have no effect when `-t` is used, because tables selected by `-t` will be dumped regardless of those switches, and non-table objects will not be dumped.

    > **Note** When `-t` is specified, `pg_dump` makes no attempt to dump any other database objects that the selected table\(s\) may depend upon. Therefore, there is no guarantee that the results of a specific-table dump can be successfully restored by themselves into a clean database.

    Also, `-t` cannot be used to specify a child table partition. To dump a partitioned table, you must specify the parent table name.

-T table \| --exclude-table=table
:   Do not dump any tables matching the table pattern. The pattern is interpreted according to the same rules as for `-t`. `-T` can be given more than once to exclude tables matching any of several patterns. When both `-t` and `-T` are given, the behavior is to dump just the tables that match at least one `-t` switch but no `-T` switches. If `-T` appears without `-t`, then tables matching `-T` are excluded from what is otherwise a normal dump.

-v \| --verbose
:   Specifies verbose mode. This will cause `pg_dump` to output detailed object comments and start/stop times to the dump file, and progress messages to standard error.

-V \| --version
:   Print the `pg_dump` version and exit.

-x \| --no-privileges \| --no-acl
:   Prevent dumping of access privileges \(`GRANT/REVOKE` commands\).

-Z 0..9 \| --compress=0..9
:   Specify the compression level to use. Zero means no compression. For the custom archive format, this specifies compression of individual table-data segments, and the default is to compress at a moderate level.

:   For plain text output, setting a non-zero compression level causes the entire output file to be compressed, as though it had been fed through `gzip`; but the default is not to compress. The tar archive format currently does not support compression at all.

--binary-upgrade
:   This option is for use by in-place upgrade utilities. Its use for other purposes is not recommended or supported. The behavior of the option may change in future releases without notice.

--column-inserts \| --attribute-inserts
:   Dump data as `INSERT` commands with explicit column names `(INSERT INTO`table`(`column`, ...) VALUES ...)`. This will make restoration very slow; it is mainly useful for making dumps that can be loaded into non-PostgreSQL-based databases. However, since this option generates a separate command for each row, an error in reloading a row causes only that row to be lost rather than the entire table contents.

--disable-dollar-quoting
:   This option deactivates the use of dollar quoting for function bodies, and forces them to be quoted using SQL standard string syntax.

--disable-triggers
:   This option is relevant only when creating a data-only dump. It instructs `pg_dump` to include commands to temporarily deactivate triggers on the target tables while the data is reloaded. Use this if you have triggers on the tables that you do not want to invoke during data reload. The commands emitted for `--disable-triggers` must be done as superuser. So, you should also specify a superuser name with `-S`, or preferably be careful to start the resulting script as a superuser. This option is only meaningful for the plain-text format. For the archive formats, you may specify the option when you call [pg\_restore](pg_restore.html).

    > **Note** Greenplum Database does not support user-defined triggers.

`--exclude-table-data=table`
:   Do not dump data for any tables matching the table pattern. The pattern is interpreted according to the same rules as for `-t`. `--exclude-table-data` can be given more than once to exclude tables matching any of several patterns. This option is useful when you need the definition of a particular table even though you do not need the data in it.

:   To exclude data for all tables in the database, see `--schema-only`.

`--if-exists`
:   Use conditional commands \(i.e. add an `IF EXISTS` clause\) when cleaning database objects. This option is not valid unless `--clean` is also specified.

--inserts
:   Dump data as `INSERT` commands \(rather than `COPY`\). This will make restoration very slow; it is mainly useful for making dumps that can be loaded into non-PostgreSQL-based databases. However, since this option generates a separate command for each row, an error in reloading a row causes only that row to be lost rather than the entire table contents. Note that the restore may fail altogether if you have rearranged column order. The `--column-inserts` option is safe against column order changes, though even slower.

--lock-wait-timeout=timeout
:   Do not wait forever to acquire shared table locks at the beginning of the dump. Instead, fail if unable to lock a table within the specified timeout. Specify timeout as a number of milliseconds.

--no-security-labels
:   Do not dump security labels.

--no-synchronized-snapshots
:   This option allows running `pg_dump -j` against a pre-6.0 Greenplum Database server; see the documentation of the `-j` parameter for more details.

--no-tablespaces
:   Do not output commands to select tablespaces. With this option, all objects will be created in whichever tablespace is the default during restore.

:   This option is only meaningful for the plain-text format. For the archive formats, you can specify the option when you call `pg_restore`.

--no-unlogged-table-data
:   Do not dump the contents of unlogged tables. This option has no effect on whether or not the table definitions \(schema\) are dumped; it only suppresses dumping the table data. Data in unlogged tables is always excluded when dumping from a standby server.

--quote-all-identifiers
:   Force quoting of all identifiers. This option is recommended when dumping a database from a server whose Greenplum Database major version is different from `pg_dump`'s, or when the output is intended to be loaded into a server of a different major version. By default, `pg_dump` quotes only identifiers that are reserved words in its own major version. This sometimes results in compatibility issues when dealing with servers of other versions that may have slightly different sets of reserved words. Using `--quote-all-identifiers` prevents such issues, at the price of a harder-to-read dump script.

--section=sectionname
:   Only dump the named section. The section name can be `pre-data`, `data`, or `post-data`. This option can be specified more than once to select multiple sections. The default is to dump all sections.

:   The `data` section contains actual table data and sequence values. `post-data` items include definitions of indexes, triggers, rules, and constraints other than validated check constraints. `pre-data` items include all other data definition items.

--serializable-deferrable
:   Use a serializable transaction for the dump, to ensure that the snapshot used is consistent with later database states; but do this by waiting for a point in the transaction stream at which no anomalies can be present, so that there isn't a risk of the dump failing or causing other transactions to roll back with a serialization\_failure.

:   This option is not beneficial for a dump which is intended only for disaster recovery. It could be useful for a dump used to load a copy of the database for reporting or other read-only load sharing while the original database continues to be updated. Without it the dump may reflect a state which is not consistent with any serial execution of the transactions eventually committed. For example, if batch processing techniques are used, a batch may show as closed in the dump without all of the items which are in the batch appearing.

:   This option will make no difference if there are no read-write transactions active when `pg_dump` is started. If read-write transactions are active, the start of the dump may be delayed for an indeterminate length of time. Once running, performance with or without the switch is the same.

:   > **Note** Because Greenplum Database does not support serializable transactions, the `--serializable-deferrable` option has no effect in Greenplum Database.

--use-set-session-authorization
:   Output SQL-standard `SET SESSION AUTHORIZATION` commands instead of `ALTER OWNER` commands to determine object ownership. This makes the dump more standards-compatible, but depending on the history of the objects in the dump, may not restore properly. A dump using `SET SESSION AUTHORIZATION` will require superuser privileges to restore correctly, whereas `ALTER OWNER` requires lesser privileges.

--gp-syntax \| --no-gp-syntax
:   Use `--gp-syntax` to dump Greenplum Database syntax in the `CREATE TABLE` statements. This allows the distribution policy \(`DISTRIBUTED BY` or `DISTRIBUTED RANDOMLY` clauses\) of a Greenplum Database table to be dumped, which is useful for restoring into other Greenplum Database systems. The default is to include Greenplum Database syntax when connected to a Greenplum Database system, and to exclude it when connected to a regular PostgreSQL system.

--function-oids oids
:   Dump the function\(s\) specified in the oids list of object identifiers.

    > **Note** This option is provided solely for use by other administration utilities; its use for any other purpose is not recommended or supported. The behavior of the option may change in future releases without notice.

--relation-oids oids
:   Dump the relation\(s\) specified in the oids list of object identifiers.

    > **Note** This option is provided solely for use by other administration utilities; its use for any other purpose is not recommended or supported. The behavior of the option may change in future releases without notice.

-? \| --help
:   Show help about `pg_dump` command line arguments, and exit.

**Connection Options**

-d dbname \| --dbname=dbname
:   Specifies the name of the database to connect to. This is equivalent to specifying dbname as the first non-option argument on the command line.

:   If this parameter contains an `=` sign or starts with a valid URI prefix \(`postgresql://` or `postgres://`\), it is treated as a conninfo string. See [Connection Strings](https://www.postgresql.org/docs/12/libpq-connect.html#LIBPQ-CONNSTRING) in the PostgreSQL documentation for more information.

-h host \| --host=host
:   The host name of the machine on which the Greenplum Database coordinator database server is running. If not specified, reads from the environment variable `PGHOST` or defaults to localhost.

-p port \| --port=port
:   The TCP port on which the Greenplum Database coordinator database server is listening for connections. If not specified, reads from the environment variable `PGPORT` or defaults to 5432.

-U username \| --username=username
:   The database role name to connect as. If not specified, reads from the environment variable `PGUSER` or defaults to the current system role name.

-W \| --password
:   Force a password prompt.

-w \| --no-password
:   Never issue a password prompt. If the server requires password authentication and a password is not available by other means such as a `.pgpass` file the connection attempt will fail. This option can be useful in batch jobs and scripts where no user is present to enter a password.

--role=rolename
:   Specifies a role name to be used to create the dump. This option causes `pg_dump` to issue a `SET ROLE rolename` command after connecting to the database. It is useful when the authenticated user \(specified by `-U`\) lacks privileges needed by `pg_dump`, but can switch to a role with the required rights. Some installations have a policy against logging in directly as a superuser, and use of this option allows dumps to be made without violating the policy.

## <a id="section7"></a>Notes 

When a data-only dump is chosen and the option `--disable-triggers` is used, `pg_dump` emits commands to deactivate triggers on user tables before inserting the data and commands to re-enable them after the data has been inserted. If the restore is stopped in the middle, the system catalogs may be left in the wrong state.

The dump file produced by `pg_dump` does not contain the statistics used by the optimizer to make query planning decisions. Therefore, it is wise to run `ANALYZE` after restoring from a dump file to ensure optimal performance.

The database activity of `pg_dump` is normally collected by the statistics collector. If this is undesirable, you can set parameter `track_counts` to false via `PGOPTIONS` or the `ALTER USER` command.

Because `pg_dump` may be used to transfer data to newer versions of Greenplum Database, the output of `pg_dump` can be expected to load into Greenplum Database versions newer than `pg_dump`'s version. `pg_dump` can also dump from Greenplum Database versions older than its own version. However, `pg_dump` cannot dump from Greenplum Database versions newer than its own major version; it will refuse to even try, rather than risk making an invalid dump. Also, it is not guaranteed that `pg_dump`'s output can be loaded into a server of an older major version — not even if the dump was taken from a server of that version. Loading a dump file into an older server may require manual editing of the dump file to remove syntax not understood by the older server. Use of the `--quote-all-identifiers` option is recommended in cross-version cases, as it can prevent problems arising from varying reserved-word lists in different Greenplum Database versions.

## <a id="section8"></a>Examples 

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

To dump a database into a directory-format archive:

```
pg_dump -Fd mydb -f dumpdir
```

To dump a database into a directory-format archive in parallel with 5 worker jobs:

```
pg_dump -Fd mydb -j 5 -f dumpdir
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

## <a id="section9"></a>See Also 

[pg\_dumpall](pg_dumpall.html), [pg\_restore](pg_restore.html), [psql](psql.html)

