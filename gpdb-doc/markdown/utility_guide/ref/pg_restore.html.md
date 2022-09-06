# pg_restore 

Restores a database from an archive file created by `pg_dump`.

## <a id="section2"></a>Synopsis 

``` {#client_util_synopsis}
pg_restore [<connection-option> ...] [<restore_option> ...] <filename>

pg_restore -? | --help

pg_restore -V | --version
```

## <a id="section3"></a>Description 

`pg_restore` is a utility for restoring a database from an archive created by [pg\_dump](pg_dump.html) in one of the non-plain-text formats. It will issue the commands necessary to reconstruct the database to the state it was in at the time it was saved. The archive files also allow `pg_restore` to be selective about what is restored, or even to reorder the items prior to being restored.

`pg_restore` can operate in two modes. If a database name is specified, the archive is restored directly into the database. Otherwise, a script containing the SQL commands necessary to rebuild the database is created and written to a file or standard output. The script output is equivalent to the plain text output format of `pg_dump`. Some of the options controlling the output are therefore analogous to `pg_dump` options.

`pg_restore` cannot restore information that is not present in the archive file. For instance, if the archive was made using the "dump data as `INSERT` commands" option, `pg_restore` will not be able to load the data using `COPY` statements.

## <a id="section4"></a>Options 

filename
:   Specifies the location of the archive file \(or directory, for a directory-format archive\) to be restored. If not specified, the standard input is used.

**Restore Options**

-a \| --data-only
:   Restore only the data, not the schema \(data definitions\). Table data and sequence values are restored, if present in the archive.

    This option is similar to, but for historical reasons not identical to, specifying `--section=data`.

-c \| --clean
:   Clean \(drop\) database objects before recreating them. \(This might generate some harmless error messages, if any objects were not present in the destination database.\)

-C \| --create
:   Create the database before restoring into it. If `--clean` is also specified, drop and recreate the target database before connecting to it.

:   When this option is used, the database named with `-d` is used only to issue the initial `DROP DATABASE` and `CREATE DATABASE` commands. All data is restored into the database name that appears in the archive.

-d dbname \| --dbname=dbname
:   Connect to this database and restore directly into this database. This utility, like most other Greenplum Database utilities, also uses the environment variables supported by `libpq`. However it does not read `PGDATABASE` when a database name is not supplied.

-e \| --exit-on-error
:   Exit if an error is encountered while sending SQL commands to the database. The default is to continue and to display a count of errors at the end of the restoration.

-f outfilename \| --file=outfilename
:   Specify output file for generated script, or for the listing when used with `-l`. Default is the standard output.

-F c\|d\|t \| --format=\{custom \| directory \| tar\}
:   The format of the archive produced by [pg\_dump](pg_dump.html). It is not necessary to specify the format, since `pg_restore` will determine the format automatically. Format can be `custom`, `directory`, or `tar`.

-I index \| --index=index
:   Restore definition of named index only.

-j \| --number-of-jobs \| --jobs=number-of-jobs
:   Run the most time-consuming parts of `pg_restore` — those which load data, create indexes, or create constraints — using multiple concurrent jobs. This option can dramatically reduce the time to restore a large database to a server running on a multiprocessor machine.

:   Each job is one process or one thread, depending on the operating system, and uses a separate connection to the server.

:   The optimal value for this option depends on the hardware setup of the server, of the client, and of the network. Factors include the number of CPU cores and the disk setup. A good place to start is the number of CPU cores on the server, but values larger than that can also lead to faster restore times in many cases. Of course, values that are too high will lead to decreased performance because of thrashing.

:   Only the custom archive format is supported with this option. The input file must be a regular file \(not, for example, a pipe\). This option is ignored when emitting a script rather than connecting directly to a database server. Also, multiple jobs cannot be used together with the option `--single-transaction`.

-l \| --list
:   List the contents of the archive. The output of this operation can be used with the `-L` option to restrict and reorder the items that are restored.

-L list-file \| --use-list=list-file
:   Restore elements in the list-file only, and in the order they appear in the file. Note that if filtering switches such as `-n` or `-t` are used with `-L`, they will further restrict the items restored.

    list-file is normally created by editing the output of a previous `-l` operation. Lines can be moved or removed, and can also be commented out by placing a semicolon \(;\) at the start of the line. See below for examples.

-n schema \| --schema=schema
:   Restore only objects that are in the named schema. This can be combined with the `-t` option to restore just a specific table.

-O \| --no-owner
:   Do not output commands to set ownership of objects to match the original database. By default, `pg_restore` issues `ALTER OWNER` or `SET SESSION AUTHORIZATION` statements to set ownership of created schema elements. These statements will fail unless the initial connection to the database is made by a superuser \(or the same user that owns all of the objects in the script\). With `-O`, any user name can be used for the initial connection, and this user will own all the created objects.

-P 'function-name\(argtype \[, ...\]\)' \| --function='function-name\(argtype \[, ...\]\)'
:   Restore the named function only. The function name must be enclosed in quotes. Be careful to spell the function name and arguments exactly as they appear in the dump file's table of contents \(as shown by the `--list` option\).

-s \| --schema-only
:   Restore only the schema \(data definitions\), not data, to the extent that schema entries are present in the archive.

    This option is the inverse of `--data-only`. It is similar to, but for historical reasons not identical to, specifying `--section=pre-data --section=post-data`.

    \(Do not confuse this with the `--schema` option, which uses the word "schema" in a different meaning.\)

-S username \| --superuser=username
:   Specify the superuser user name to use when deactivating triggers. This is only relevant if `--disable-triggers` is used.

    **Note:** Greenplum Database does not support user-defined triggers.

-t table \| --table=table
:   Restore definition and/or data of named table only. Multiple tables may be specified with multiple `-t` switches. This can be combined with the `-n` option to specify a schema.

-T trigger \| --trigger=trigger
:   Restore named trigger only.

    **Note:** Greenplum Database does not support user-defined triggers.

-v \| --verbose
:   Specifies verbose mode.

-V \| --version
:   Print the `pg_restore` version and exit.

-x \| --no-privileges \| --no-acl
:   Prevent restoration of access privileges \(`GRANT/REVOKE` commands\).

-1 \| --single-transaction
:   Run the restore as a single transaction. This ensures that either all the commands complete successfully, or no changes are applied.

--disable-triggers
:   This option is relevant only when performing a data-only restore. It instructs `pg_restore` to run commands to temporarily deactivate triggers on the target tables while the data is reloaded. Use this if you have triggers on the tables that you do not want to invoke during data reload. The commands emitted for `--disable-triggers` must be done as superuser. So you should also specify a superuser name with `-S` or, preferably, run `pg_restore` as a superuser.

    **Note:** Greenplum Database does not support user-defined triggers.

--no-data-for-failed-tables
:   By default, table data is restored even if the creation command for the table failed \(e.g., because it already exists\). With this option, data for such a table is skipped. This behavior is useful when the target database may already contain the desired table contents. Specifying this option prevents duplicate or obsolete data from being loaded. This option is effective only when restoring directly into a database, not when producing SQL script output.

--no-security-labels
:   Do not output commands to restore security labels, even if the archive contains them.

--no-tablespaces
:   Do not output commands to select tablespaces. With this option, all objects will be created in whichever tablespace is the default during restore.

--section=sectionname
:   Only restore the named section. The section name can be `pre-data`, `data`, or `post-data`. This option can be specified more than once to select multiple sections.

:   The default is to restore all sections.

--use-set-session-authorization
:   Output SQL-standard `SET SESSION AUTHORIZATION` commands instead of `ALTER OWNER` commands to determine object ownership. This makes the dump more standards-compatible, but depending on the history of the objects in the dump, it might not restore properly.

-? \| --help
:   Show help about `pg_restore` command line arguments, and exit.

**Connection Options**

-h host \| --host host
:   The host name of the machine on which the Greenplum master database server is running. If not specified, reads from the environment variable `PGHOST` or defaults to localhost.

-p port \| --port port
:   The TCP port on which the Greenplum Database master database server is listening for connections. If not specified, reads from the environment variable `PGPORT` or defaults to 5432.

-U username \| --username username
:   The database role name to connect as. If not specified, reads from the environment variable `PGUSER` or defaults to the current system role name.

-w \| --no-password
:   Never issue a password prompt. If the server requires password authentication and a password is not available by other means such as a `.pgpass` file the connection attempt will fail. This option can be useful in batch jobs and scripts where no user is present to enter a password.

-W \| --password
:   Force a password prompt.

--role=rolename
:   Specifies a role name to be used to perform the restore. This option causes `pg_restore` to issue a `SET ROLE rolename` command after connecting to the database. It is useful when the authenticated user \(specified by `-U`\) lacks privileges needed by `pg_restore`, but can switch to a role with the required rights. Some installations have a policy against logging in directly as a superuser, and use of this option allows restores to be performed without violating the policy.

## <a id="section6"></a>Notes 

If your installation has any local additions to the `template1` database, be careful to load the output of `pg_restore` into a truly empty database; otherwise you are likely to get errors due to duplicate definitions of the added objects. To make an empty database without any local additions, copy from `template0` not `template1`, for example:

```
CREATE DATABASE foo WITH TEMPLATE template0;
```

When restoring data to a pre-existing table and the option `--disable-triggers` is used, `pg_restore` emits commands to deactivate triggers on user tables before inserting the data, then emits commands to re-enable them after the data has been inserted. If the restore is stopped in the middle, the system catalogs may be left in the wrong state.

See also the `pg_dump` documentation for details on limitations of `pg_dump`.

Once restored, it is wise to run `ANALYZE` on each restored table so the query planner has useful statistics.

## <a id="section7"></a>Examples 

Assume we have dumped a database called `mydb` into a custom-format dump file:

```
pg_dump -Fc mydb > db.dump
```

To drop the database and recreate it from the dump:

```
dropdb mydb
pg_restore -C -d template1 db.dump
```

To reload the dump into a new database called `newdb`. Notice there is no `-C`, we instead connect directly to the database to be restored into. Also note that we clone the new database from `template0` not `template1`, to ensure it is initially empty:

```
createdb -T template0 newdb
pg_restore -d newdb db.dump
```

To reorder database items, it is first necessary to dump the table of contents of the archive:

```
pg_restore -l db.dump > db.list
```

The listing file consists of a header and one line for each item, for example,

```
; Archive created at Mon Sep 14 13:55:39 2009
;     dbname: DBDEMOS
;     TOC Entries: 81
;     Compression: 9
;     Dump Version: 1.10-0
;     Format: CUSTOM
;     Integer: 4 bytes
;     Offset: 8 bytes
;     Dumped from database version: 9.4.24
;     Dumped by pg_dump version: 9.4.24
;
; Selected TOC Entries:
;
3; 2615 2200 SCHEMA - public pasha
1861; 0 0 COMMENT - SCHEMA public pasha
1862; 0 0 ACL - public pasha
317; 1247 17715 TYPE public composite pasha
319; 1247 25899 DOMAIN public domain0 pasha2
```

Semicolons start a comment, and the numbers at the start of lines refer to the internal archive ID assigned to each item. Lines in the file can be commented out, deleted, and reordered. For example:

```
10; 145433 TABLE map_resolutions postgres
;2; 145344 TABLE species postgres
;4; 145359 TABLE nt_header postgres
6; 145402 TABLE species_records postgres
;8; 145416 TABLE ss_old postgres
```

Could be used as input to `pg_restore` and would only restore items 10 and 6, in that order:

```
pg_restore -L db.list db.dump
```

## <a id="section8"></a>See Also 

[pg\_dump](pg_dump.html)

