# pg\_restore 

Restores a database from an archive file created by `pg_dump`.

## Synopsis 

``` {#client_util_synopsis}
pg_restore [<connection_option> ...] [<restore_option> ...] <filename>
```

## Description 

`pg_restore` is a utility for restoring a database from an archive created by [pg\_dump](pg_dump.html) in one of the non-plain-text formats. It will issue the commands necessary to reconstruct the database to the state it was in at the time it was saved. The archive files also allow `pg_restore` to be selective about what is restored, or even to reorder the items prior to being restored.

`pg_restore` can operate in two modes. If a database name is specified, the archive is restored directly into the database. Otherwise, a script containing the SQL commands necessary to rebuild the database is created and written to a file or standard output. The script output is equivalent to the plain text output format of `pg_dump`. Some of the options controlling the output are therefore analogous to `pg_dump` options.

`pg_restore` cannot restore information that is not present in the archive file. For instance, if the archive was made using the "dump data as `INSERT` commands" option, `pg_restore` will not be able to load the data using `COPY` statements.

**Note:** The `--ignore-version` option is deprecated and will be removed in a future release.

## Options 

filename
:   Specifies the location of the archive file to be restored. If not specified, the standard input is used.

**Restore Options**

-a \| --data-only
:   Restore only the data, not the schema \(data definitions\).

-c \| --clean
:   Clean \(drop\) database objects before recreating them.

-C \| --create
:   Create the database before restoring into it. \(When this option is used, the database named with `-d` is used only to issue the initial `CREATE DATABASE` command. All data is restored into the database name that appears in the archive.\)

-d dbname \| --dbname=dbname
:   Connect to this database and restore directly into this database. The default is to use the `PGDATABASE` environment variable setting, or the same name as the current system user.

-e \| --exit-on-error
:   Exit if an error is encountered while sending SQL commands to the database. The default is to continue and to display a count of errors at the end of the restoration.

-f outfilename \| --file=outfilename
:   Specify output file for generated script, or for the listing when used with `-l`. Default is the standard output.

-F t \|c \| --format=tar \| custom
:   The format of the archive produced by [pg\_dump](pg_dump.html). It is not necessary to specify the format, since `pg_restore` will determine the format automatically. Format can be either `tar` or `custom`.

-i \| --ignore-version
:   **Note:** This option is deprecated and will be removed in a future release.

    Ignore database version checks.

-I index \| --index=index
:   Restore definition of named index only.

-l \| --list
:   List the contents of the archive. The output of this operation can be used with the `-L` option to restrict and reorder the items that are restored.

-L list-file \| --use-list=list-file
:   Restore elements in the list-file only, and in the order they appear in the file. Lines can be moved and may also be commented out by placing a `;` at the start of the line.

-n schema \| --schema=schema
:   Restore only objects that are in the named schema. This can be combined with the `-t` option to restore just a specific table.

-O \| --no-owner
:   Do not output commands to set ownership of objects to match the original database. By default, `pg_restore` issues `ALTER OWNER` or `SET SESSION AUTHORIZATION` statements to set ownership of created schema elements. These statements will fail unless the initial connection to the database is made by a superuser \(or the same user that owns all of the objects in the script\). With `-O`, any user name can be used for the initial connection, and this user will own all the created objects.

-P 'function-name\(argtype \[, ...\]\)' \| --function='function-name\(argtype \[, ...\]\)'
:   Restore the named function only. The function name must be enclosed in quotes. Be careful to spell the function name and arguments exactly as they appear in the dump file's table of contents \(as shown by the `--list` option\).

-s \| --schema-only
:   Restore only the schema \(data definitions\), not the data \(table contents\). Sequence current values will not be restored, either. \(Do not confuse this with the `--schema` option, which uses the word schema in a different meaning.\)

-S username \| --superuser=username
:   Specify the superuser user name to use when disabling triggers. This is only relevant if `--disable-triggers` is used.

    **Note:** Greenplum Database does not support user-defined triggers.

-t table \| --table=table
:   Restore definition and/or data of named table only.

-T trigger \| --trigger=trigger
:   Restore named trigger only.

    **Note:** Greenplum Database does not support user-defined triggers.

-v \| --verbose
:   Specifies verbose mode.

-x \| --no-privileges \| --no-acl
:   Prevent restoration of access privileges \(`GRANT/REVOKE` commands\).

--disable-triggers
:   This option is only relevant when performing a data-only restore. It instructs `pg_restore` to execute commands to temporarily disable triggers on the target tables while the data is reloaded. Use this if you have triggers on the tables that you do not want to invoke during data reload. The commands emitted for `--disable-triggers` must be done as superuser. So, you should also specify a superuser name with `-S`, or preferably run `pg_restore` as a superuser.

    **Note:** Greenplum Database does not support user-defined triggers.

--no-data-for-failed-tables
:   By default, table data is restored even if the creation command for the table failed \(e.g., because it already exists\). With this option, data for such a table is skipped. This behavior is useful when the target database may already contain the desired table contents. Specifying this option prevents duplicate or obsolete data from being loaded. This option is effective only when restoring directly into a database, not when producing SQL script output.

**Connection Options**

-h host \| --host host
:   The host name of the machine on which the Greenplum master database server is running. If not specified, reads from the environment variable `PGHOST` or defaults to localhost.

-p port \| --port port
:   The TCP port on which the Greenplum Database master database server is listening for connections. If not specified, reads from the environment variable `PGPORT` or defaults to 5432.

-U username \| --username username
:   The database role name to connect as. If not specified, reads from the environment variable `PGUSER` or defaults to the current system role name.

-W \| --password
:   Force a password prompt.

-1 \| --single-transaction
:   Execute the restore as a single transaction. This ensures that either all the commands complete successfully, or no changes are applied.

## Notes 

If your installation has any local additions to the `template1` database, be careful to load the output of `pg_restore` into a truly empty database; otherwise you are likely to get errors due to duplicate definitions of the added objects. To make an empty database without any local additions, copy from `template0` not `template1`, for example:

```
CREATE DATABASE foo WITH TEMPLATE template0;
```

When restoring data to a pre-existing table and the option `--disable-triggers` is used, `pg_restore` emits commands to disable triggers on user tables before inserting the data then emits commands to re-enable them after the data has been inserted. If the restore is stopped in the middle, the system catalogs may be left in the wrong state.

`pg_restore` will not restore large objects for a single table. If an archive contains large objects, then all large objects will be restored.

See also the `pg_dump` documentation for details on limitations of `pg_dump`.

Once restored, it is wise to run `ANALYZE` on each restored table so the query planner has useful statistics.

## Examples 

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
; Archive created at Fri Jul 28 22:28:36 2006
;     dbname: mydb
;     TOC Entries: 74
;     Compression: 0
;     Dump Version: 1.4-0
;     Format: CUSTOM
;
; Selected TOC Entries:
;
2; 145344 TABLE species postgres
3; 145344 ACL species
4; 145359 TABLE nt_header postgres
5; 145359 ACL nt_header
6; 145402 TABLE species_records postgres
7; 145402 ACL species_records
8; 145416 TABLE ss_old postgres
9; 145416 ACL ss_old
10; 145433 TABLE map_resolutions postgres
11; 145433 ACL map_resolutions
12; 145443 TABLE hs_old postgres
13; 145443 ACL hs_old
```

Semicolons start a comment, and the numbers at the start of lines refer to the internal archive ID assigned to each item. Lines in the file can be commented out, deleted, and reordered. For example,

```
10; 145433 TABLE map_resolutions postgres
;2; 145344 TABLE species postgres
;4; 145359 TABLE nt_header postgres
6; 145402 TABLE species_records postgres
;8; 145416 TABLE ss_old postgres
```

Could be used as input to `pg_restore` and would only restore items 10 and 6, in that order:

pg\_restore -L db.list db.dump

## See Also 

[pg\_dump](pg_dump.html)

