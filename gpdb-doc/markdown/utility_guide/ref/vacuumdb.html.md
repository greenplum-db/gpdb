# vacuumdb 

Garbage-collects and analyzes a database.

## <a id="section2"></a>Synopsis 

``` {#client_util_synopsis}
vacuumdb [<connection-option>...] [--full | -f] [--freeze | -F] [--verbose | -v]
    [--analyze | -z] [--analyze-only | -Z] [--disable-page-skipping] [--skip-locked] [--table | -t <table> [( <column> [,...] )] ] [<dbname>]

vacuumdb [<connection-option>...] [--all | -a] [--full | -f] [-F] 
    [--verbose | -v] [--analyze | -z]
    [--analyze-only | -Z]
    [--disable-page-skipping]
    [--skip-locked]

vacuumdb -? | --help

vacuumdb -V | --version
```

## <a id="section3"></a>Description 

`vacuumdb` is a utility for cleaning a Greenplum Database database. `vacuumdb` will also generate internal statistics used by the Greenplum Database query optimizer.

`vacuumdb` is a wrapper around the SQL command `VACUUM`. There is no effective difference between vacuuming databases via this utility and via other methods for accessing the server.

## <a id="section4"></a>Options 

-a \| --all
:   Vacuums all databases.

\[-d\] dbname \| \[--dbname=\]dbname
:   The name of the database to vacuum. If this is not specified and `-a` \(or `--all`\) is not used, the database name is read from the environment variable `PGDATABASE`. If that is not set, the user name specified for the connection is used.

-e \| --echo
:   Echo the commands that `reindexdb` generates and sends to the server.

-f \| --full
:   Selects a full vacuum, which may reclaim more space, but takes much longer and exclusively locks the table.

    > **Caution** A `VACUUM FULL` is not recommended in Greenplum Database.

-F \| --freeze
:   Freeze row transaction information.

-q \| --quiet
:   Do not display a response.

-t table \[\(column\)\] \| --table= table \[\(column\)\]
:   Clean or analyze this table only. Column names may be specified only in conjunction with the `--analyze` or `--analyze-all` options. Multiple tables can be vacuumed by writing multiple `-t` switches. If you specify columns, you probably have to escape the parentheses from the shell.

-v \| --verbose
:   Print detailed information during processing.

-z \| --analyze
:   Collect statistics for use by the query planner.

-Z \| --analyze-only
:   Only calculate statistics for use by the query planner \(no vacuum\).

--disable-page-skipping
:   Disable all page-skipping behavior.

--skip-locked
:   Skip relations that cannot be immediately locked.

-V \| --version
:   Print the `vacuumdb` version and exit.

-? \| --help
:   Show help about `vacuumdb` command line arguments, and exit.

**Connection Options**

-h host \| --host=host
:   Specifies the host name of the machine on which the Greenplum coordinator database server is running. If not specified, reads from the environment variable `PGHOST` or defaults to localhost.

-p port \| --port=port
:   Specifies the TCP port on which the Greenplum coordinator database server is listening for connections. If not specified, reads from the environment variable `PGPORT` or defaults to 5432.

-U username \| --username=username
:   The database role name to connect as. If not specified, reads from the environment variable `PGUSER` or defaults to the current system user name.

-w \| --no-password
:   Never issue a password prompt. If the server requires password authentication and a password is not available by other means such as a `.pgpass` file, the connection attempt will fail. This option can be useful in batch jobs and scripts where no user is present to enter a password.

-W \| --password
:   Force a password prompt.

--maintenance-db=dbname
:   Specifies the name of the database to connect to discover what other databases should be vacuumed. If not specified, the `postgres` database will be used, and if that does not exist, `template1` will be used.

## <a id="section6"></a>Notes 

`vacuumdb` might need to connect several times to the coordinator server, asking for a password each time. It is convenient to have a `~/.pgpass` file in such cases.

## <a id="section7"></a>Examples 

To clean the database `test`:

```
vacuumdb test
```

To clean and analyze a database named `bigdb`:

```
vacuumdb --analyze bigdb
```

To clean a single table `foo` in a database named `mydb`, and analyze a single column `bar` of the table. Note the quotes around the table and column names to escape the parentheses from the shell:

```
vacuumdb --analyze --verbose --table 'foo(bar)' mydb
```

## <a id="section8"></a>See Also 

[VACUUM](../../ref_guide/sql_commands/VACUUM.html), [ANALYZE](../../ref_guide/sql_commands/ANALYZE.html)

