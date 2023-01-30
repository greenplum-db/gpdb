# gpload 

Runs a load job as defined in a YAML formatted control file.

## <a id="section2"></a>Synopsis 

```
gpload -f <control_file> [-l <log_file>] [-h <hostname>] [-p <port>] 
   [-U <username>] [-d <database>] [-W] [--gpfdist_timeout <seconds>] 
   [--no_auto_trans] [--max_retries <retry_times>] [[-v | -V] [-q]] [-D]

gpload -? 

gpload --version
```

## <a id="section3"></a>Requirements 

The client machine where `gpload` is run must have the following:

-   The [gpfdist](gpfdist.html) parallel file distribution program installed and in your `$PATH`. This program is located in `$GPHOME/bin` of your Greenplum Database server installation.
-   Network access to and from all hosts in your Greenplum Database array \(coordinator and segments\).
-   Network access to and from the hosts where the data to be loaded resides \(ETL servers\).

## <a id="section4"></a>Description 

`gpload` is a data loading utility that acts as an interface to the Greenplum Database external table parallel loading feature. Using a load specification defined in a YAML formatted control file, `gpload` runs a load by invoking the Greenplum Database parallel file server \([gpfdist](gpfdist.html)\), creating an external table definition based on the source data defined, and running an `INSERT`, `UPDATE` or `MERGE` operation to load the source data into the target table in the database.

> **Note** `gpfdist` is compatible only with the Greenplum Database major version in which it is shipped. For example, a `gpfdist` utility that is installed with Greenplum Database 4.x cannot be used with Greenplum Database 5.x or 6.x.

> **Note** The Greenplum Database 5.22 and later `gpload` for Linux is compatible with Greenplum Database 6.x. The Greenplum Database 6.x `gpload` for both Linux and Windows is compatible with Greenplum 5.x.

> **Note** `MERGE` and `UPDATE` operations are not supported if the target table column name is a reserved keyword, has capital letters, or includes any character that requires quotes \(" "\) to identify the column.

The operation, including any SQL commands specified in the `SQL` collection of the YAML control file \(see [Control File Format](#section7)\), are performed as a single transaction to prevent inconsistent data when performing multiple, simultaneous load operations on a target table.

## <a id="section5"></a>Options 

-f control\_file
:   Required. A YAML file that contains the load specification details. See [Control File Format](#section7).

--gpfdist\_timeout seconds
:   Sets the timeout for the `gpfdist` parallel file distribution program to send a response. Enter a value from `0` to `30` seconds \(entering "`0`" to deactivates timeouts\). Note that you might need to increase this value when operating on high-traffic networks.

-l log\_file
:   Specifies where to write the log file. Defaults to `~/gpAdminLogs/gpload_YYYYMMDD`. For more information about the log file, see [Log File Format](#section9).

--no\_auto\_trans
:   Specify `--no_auto_trans` to deactivate processing the load operation as a single transaction if you are performing a single load operation on the target table.

:   By default, `gpload` processes each load operation as a single transaction to prevent inconsistent data when performing multiple, simultaneous operations on a target table.

-q \(no screen output\)
:   Run in quiet mode. Command output is not displayed on the screen, but is still written to the log file.

-D \(debug mode\)
:   Check for error conditions, but do not run the load.

-v \(verbose mode\)
:   Show verbose output of the load steps as they are run.

-V \(very verbose mode\)
:   Shows very verbose output.

-? \(show help\)
:   Show help, then exit.

--version
:   Show the version of this utility, then exit.

**Connection Options**

-d database
:   The database to load into. If not specified, reads from the load control file, the environment variable `$PGDATABASE` or defaults to the current system user name.

-h hostname
:   Specifies the host name of the machine on which the Greenplum Database coordinator database server is running. If not specified, reads from the load control file, the environment variable `$PGHOST` or defaults to `localhost`.

-p port
:   Specifies the TCP port on which the Greenplum Database coordinator database server is listening for connections. If not specified, reads from the load control file, the environment variable `$PGPORT` or defaults to 5432.

--max\_retries retry\_times
:   Specifies the maximum number of times `gpload` attempts to connect to Greenplum Database after a connection timeout. The default value is `0`, do not attempt to connect after a connection timeout. A negative integer, such as `-1`, specifies an unlimited number of attempts.

-U username
:   The database role name to connect as. If not specified, reads from the load control file, the environment variable `$PGUSER` or defaults to the current system user name.

-W \(force password prompt\)
:   Force a password prompt. If not specified, reads the password from the environment variable `$PGPASSWORD` or from a password file specified by `$PGPASSFILE` or in `~/.pgpass`. If these are not set, then `gpload` will prompt for a password even if `-W` is not supplied.

## <a id="section7"></a>Control File Format 

The `gpload` control file uses the [YAML 1.1](http://yaml.org/spec/1.1/) document format and then implements its own schema for defining the various steps of a Greenplum Database load operation. The control file must be a valid YAML document.

The `gpload` program processes the control file document in order and uses indentation \(spaces\) to determine the document hierarchy and the relationships of the sections to one another. The use of white space is significant. White space should not be used simply for formatting purposes, and tabs should not be used at all.

The basic structure of a load control file is:

```
---
[VERSION](#cfversion): 1.0.0.1
[DATABASE](#cfdatabase): <db_name>
[USER](#cfuser): <db_username>
[HOST](#cfhost): <coordinator_hostname>
[PORT](#cfport): <coordinator_port>
[GPLOAD](#cfgpload):
   [INPUT](#cfinput):
    - [SOURCE](#cfsource):
         [LOCAL\_HOSTNAME](#cfsourcelocalname):
           - <hostname_or_ip>
         [PORT](#cfsourceport): <http_port>
       | [PORT\_RANGE](#cfversion): [<start_port_range>, <end_port_range>]
         [FILE](#cfsourcefile): 
           - </path/to/input_file>
         [SSL](#cfsourcessl): true | false
         [CERTIFICATES\_PATH](#cfsourcecertificatespath): </path/to/certificates>
    - [FULLY\_QUALIFIED\_DOMAIN\_NAME](#fqdn): true | false
    - [COLUMNS](#cfcolumns):
           - <field_name>: <data_type>
    - [TRANSFORM](#cftransform): '<transformation>'
    - [TRANSFORM\_CONFIG](#cftransformconfig): '<configuration-file-path>' 
    - [MAX\_LINE\_LENGTH](#cfmaxlinelength): <integer> 
    - [FORMAT](#cfformat): text | csv
    - [DELIMITER](#cfdelimiter): '<delimiter_character>'
    - [ESCAPE](#cfescape): '<escape_character>' | 'OFF'
    - [NEWLINE](#newline): 'LF' | 'CR' | 'CRLF'
    - [NULL\_AS](#cfnullas): '<null_string>'
    - [FILL\_MISSING\_FIELDS](#cfillfields): true | false
    - [FORCE\_NOT\_NULL](#cfforcenotnull): true | false
    - [QUOTE](#cfquote): '<csv_quote_character>'
    - [HEADER](#cfheader): true | false
    - [ENCODING](#cfencoding): <database_encoding>
    - [ERROR\_LIMIT](#cferrorlimit): <integer>
    - [LOG\_ERRORS](#cferrorlog): true | false
   [EXTERNAL](#cfexternal):
      - [SCHEMA](#cfschema): <schema> | '%'
   [OUTPUT](#cfoutput):
    - [TABLE](#cftable): <schema.table_name>
    - [MODE](#cfmode): insert | update | merge
    - [MATCH\_COLUMNS](#cfmatchcolumns):
           - <target_column_name>
    - [UPDATE\_COLUMNS](#cfupdatecolumns):
           - <target_column_name>
    - [UPDATE\_CONDITION](#cfupdatecondition): '<boolean_condition>'
    - [MAPPING](#cfmapping):
              <target_column_name>: <source_column_name> | '<expression>'
   [PRELOAD](#cfpreload):
    - [TRUNCATE](#cftruncate): true | false
    - [REUSE\_TABLES](#cfreusetables): true | false
    - [STAGING\_TABLE](#cfstagetbl): <external_table_name>
    - [FAST\_MATCH](#cffastmatch): true | false
   [SQL](#cfsql):
    - [BEFORE](#cfbefore): "<sql_command>"
    - [AFTER](#cfafter): "<sql_command>"
```

VERSION
:   Optional. The version of the `gpload` control file schema. The current version is 1.0.0.1.

DATABASE
:   Optional. Specifies which database in the Greenplum Database system to connect to. If not specified, defaults to `$PGDATABASE` if set or the current system user name. You can also specify the database on the command line using the `-d` option.

USER
:   Optional. Specifies which database role to use to connect. If not specified, defaults to the current user or `$PGUSER` if set. You can also specify the database role on the command line using the `-U` option.

:   If the user running `gpload` is not a Greenplum Database superuser, then the appropriate rights must be granted to the user for the load to be processed. See the *Greenplum Database Reference Guide* for more information.

HOST
:   Optional. Specifies Greenplum Database coordinator host name. If not specified, defaults to localhost or `$PGHOST` if set. You can also specify the coordinator host name on the command line using the `-h` option.

PORT
:   Optional. Specifies Greenplum Database coordinator port. If not specified, defaults to 5432 or `$PGPORT` if set. You can also specify the coordinator port on the command line using the `-p` option.

GPLOAD
:   Required. Begins the load specification section. A `GPLOAD` specification must have an `INPUT` and an `OUTPUT` section defined.

    INPUT
    :   Required. Defines the location and the format of the input data to be loaded. `gpload` will start one or more instances of the [gpfdist](gpfdist.html) file distribution program on the current host and create the required external table definition\(s\) in Greenplum Database that point to the source data. Note that the host from which you run `gpload` must be accessible over the network by all Greenplum Database hosts \(coordinator and segments\).

    SOURCE
    :   Required. The `SOURCE` block of an `INPUT` specification defines the location of a source file. An `INPUT` section can have more than one `SOURCE` block defined. Each `SOURCE` block defined corresponds to one instance of the [gpfdist](gpfdist.html) file distribution program that will be started on the local machine. Each `SOURCE` block defined must have a `FILE` specification.

    For more information about using the `gpfdist` parallel file server and single and multiple `gpfdist` instances, see [Loading and Unloading Data](../../admin_guide/load/topics/g-loading-and-unloading-data.html).

    LOCAL\_HOSTNAME
    :   Optional. Specifies the host name or IP address of the local machine on which `gpload` is running. If this machine is configured with multiple network interface cards \(NICs\), you can specify the host name or IP of each individual NIC to allow network traffic to use all NICs simultaneously. The default is to use the local machine's primary host name or IP only.

    PORT
    :   Optional. Specifies the specific port number that the [gpfdist](gpfdist.html) file distribution program should use. You can also supply a `PORT_RANGE` to select an available port from the specified range. If both `PORT` and `PORT_RANGE` are defined, then `PORT` takes precedence. If neither `PORT` or `PORT_RANGE` are defined, the default is to select an available port between 8000 and 9000.

    If multiple host names are declared in `LOCAL_HOSTNAME`, this port number is used for all hosts. This configuration is desired if you want to use all NICs to load the same file or set of files in a given directory location.

    PORT\_RANGE
    :   Optional. Can be used instead of `PORT` to supply a range of port numbers from which `gpload` can choose an available port for this instance of the [gpfdist](gpfdist.html) file distribution program.

    FILE
    :   Required. Specifies the location of a file, named pipe, or directory location on the local file system that contains data to be loaded. You can declare more than one file so long as the data is of the same format in all files specified.

    If the files are compressed using `gzip` or `bzip2` \(have a `.gz` or `.bz2` file extension\), the files will be uncompressed automatically \(provided that `gunzip` or `bunzip2` is in your path\).

    When specifying which source files to load, you can use the wildcard character \(`*`\) or other C-style pattern matching to denote multiple files. The files specified are assumed to be relative to the current directory from which `gpload` is run \(or you can declare an absolute path\).

    SSL
    :   Optional. Specifies usage of SSL encryption. If `SSL` is set to true, `gpload` starts the `gpfdist` server with the `--ssl` option and uses the `gpfdists://` protocol.

    CERTIFICATES\_PATH
    :   Required when SSL is `true`; cannot be specified when SSL is `false` or unspecified. The location specified in `CERTIFICATES_PATH` must contain the following files:

    -   The server certificate file, `server.crt`
    -   The server private key file, `server.key`
    -   The trusted certificate authorities, `root.crt`

    The root directory \(`/`\) cannot be specified as `CERTIFICATES_PATH`.

    FULLY\_QUALIFIED\_DOMAIN\_NAME
    :   Optional. Specifies whether `gpload` resolve hostnames to the fully qualified domain name \(FQDN\) or the local hostname. If the value is set to `true`, names are resolved to the FQDN. If the value is set to `false`, resolution is to the local hostname. The default is `false`.

    A fully qualified domain name might be required in some situations. For example, if the Greenplum Database system is in a different domain than an ETL application that is being accessed by `gpload`.

    COLUMNS
    :   Optional. Specifies the schema of the source data file\(s\) in the format of `field\_name:data\_type`. The `DELIMITER` character in the source file is what separates two data value fields \(columns\). A row is determined by a line feed character \(`0x0a`\).

    If the input `COLUMNS` are not specified, then the schema of the output `TABLE` is implied, meaning that the source data must have the same column order, number of columns, and data format as the target table.

    The default source-to-target mapping is based on a match of column names as defined in this section and the column names in the target `TABLE`. This default mapping can be overridden using the `MAPPING` section.

    TRANSFORM
    :   Optional. Specifies the name of the input transformation passed to `gpload`. For information about XML transformations, see "Loading and Unloading Data" in the *Greenplum Database Administrator Guide*.

    TRANSFORM\_CONFIG
    :   Required when `TRANSFORM` is specified. Specifies the location of the transformation configuration file that is specified in the `TRANSFORM` parameter, above.

    MAX\_LINE\_LENGTH
    :   Optional. An integer that specifies the maximum length of a line in the XML transformation data passed to `gpload`.

    FORMAT
    :   Optional. Specifies the format of the source data file\(s\) - either plain text \(`TEXT`\) or comma separated values \(`CSV`\) format. Defaults to `TEXT` if not specified. For more information about the format of the source data, see [Loading and Unloading Data](../../admin_guide/load/topics/g-loading-and-unloading-data.html).

    DELIMITER
    :   Optional. Specifies a single ASCII character that separates columns within each row \(line\) of data. The default is a tab character in TEXT mode, a comma in CSV mode. You can also specify a non- printable ASCII character or a non-printable unicode character, for example: `"\x1B"` or `"\u001B"`. The escape string syntax, `E'<character-code>'`, is also supported for non-printable characters. The ASCII or unicode character must be enclosed in single quotes. For example: `E'\x1B'` or `E'\u001B'`.

    ESCAPE
    :   Specifies the single character that is used for C escape sequences \(such as `\n`, `\t`, `\100`, and so on\) and for escaping data characters that might otherwise be taken as row or column delimiters. Make sure to choose an escape character that is not used anywhere in your actual column data. The default escape character is a \\ \(backslash\) for text-formatted files and a `"` \(double quote\) for csv-formatted files, however it is possible to specify another character to represent an escape. It is also possible to deactivate escaping in text-formatted files by specifying the value `'OFF'` as the escape value. This is very useful for data such as text-formatted web log data that has many embedded backslashes that are not intended to be escapes.

    NEWLINE
    :   Specifies the type of newline used in your data files, one of:

    -   LF \(Line feed, 0x0A\)
    -   CR \(Carriage return, 0x0D\)
    -   CRLF \(Carriage return plus line feed, 0x0D 0x0A\).

    If not specified, Greenplum Database detects the newline type by examining the first row of data that it receives, and uses the first newline type that it encounters.

    NULL\_AS
    :   Optional. Specifies the string that represents a null value. The default is `\N` \(backslash-N\) in `TEXT` mode, and an empty value with no quotations in `CSV` mode. You might prefer an empty string even in `TEXT` mode for cases where you do not want to distinguish nulls from empty strings. Any source data item that matches this string will be considered a null value.

    FILL\_MISSING\_FIELDS
    :   Optional. The default value is `false`. When reading a row of data that has missing trailing field values \(the row of data has missing data fields at the end of a line or row\), Greenplum Database returns an error.

    If the value is `true`, when reading a row of data that has missing trailing field values, the values are set to `NULL`. Blank rows, fields with a `NOT NULL` constraint, and trailing delimiters on a line will still report an error.

    See the `FILL MISSING FIELDS` clause of the [CREATE EXTERNAL TABLE](../../ref_guide/sql_commands/CREATE_EXTERNAL_TABLE.html) command.

    FORCE\_NOT\_NULL
    :   Optional. In CSV mode, processes each specified column as though it were quoted and hence not a NULL value. For the default null string in CSV mode \(nothing between two delimiters\), this causes missing values to be evaluated as zero-length strings.

    QUOTE
    :   Required when `FORMAT` is `CSV`. Specifies the quotation character for `CSV` mode. The default is double-quote \(`"`\).

    HEADER
    :   Optional. Specifies that the first line in the data file\(s\) is a header row \(contains the names of the columns\) and should not be included as data to be loaded. If using multiple data source files, all files must have a header row. The default is to assume that the input files do not have a header row.

    ENCODING
    :   Optional. Character set encoding of the source data. Specify a string constant \(such as `'SQL_ASCII'`\), an integer encoding number, or `'DEFAULT'` to use the default client encoding. If not specified, the default client encoding is used. For information about supported character sets, see the *Greenplum Database Reference Guide*.

    > **Note** If you *change* the `ENCODING` value in an existing `gpload` control file, you must manually drop any external tables that were creating using the previous `ENCODING` configuration. `gpload` does not drop and recreate external tables to use the new `ENCODING` if `REUSE_TABLES` is set to `true`.

    ERROR\_LIMIT
    :   Optional. Enables single row error isolation mode for this load operation. When enabled, input rows that have format errors will be discarded provided that the error limit count is not reached on any Greenplum Database segment instance during input processing. If the error limit is not reached, all good rows will be loaded and any error rows will either be discarded or captured as part of error log information. The default is to cancel the load operation on the first error encountered. Note that single row error isolation only applies to data rows with format errors; for example, extra or missing attributes, attributes of a wrong data type, or invalid client encoding sequences. Constraint errors, such as primary key violations, will still cause the load operation to be cancelled if encountered. For information about handling load errors, see [Loading and Unloading Data](../../admin_guide/load/topics/g-loading-and-unloading-data.html).

    LOG\_ERRORS
    :   Optional when `ERROR_LIMIT` is declared. Value is either `true` or `false`. The default value is `false`. If the value is `true`, rows with formatting errors are logged internally when running in single row error isolation mode. You can examine formatting errors with the Greenplum Database built-in SQL function `gp_read_error_log('<table_name>')`. If formatting errors are detected when loading data, `gpload` generates a warning message with the name of the table that contains the error information similar to this message.

    ```
    <timestamp>|WARN|1 bad row, please use GPDB built-in function gp_read_error_log('table-name') 
       to access the detailed error row
    ```

    :   If `LOG_ERRORS: true` is specified, `REUSE_TABLES: true` must be specified to retain the formatting errors in Greenplum Database error logs. If `REUSE_TABLES: true` is not specified, the error information is deleted after the `gpload` operation. Only summary information about formatting errors is returned. You can delete the formatting errors from the error logs with the Greenplum Database function `gp_truncate_error_log()`.

    > **Note** When `gpfdist` reads data and encounters a data formatting error, the error message includes a row number indicating the location of the formatting error. `gpfdist` attempts to capture the row that contains the error. However, `gpfdist` might not capture the exact row for some formatting errors.

    For more information about handling load errors, see "Loading and Unloading Data" in the *Greenplum Database Administrator Guide*. For information about the `gp_read_error_log()` function, see the [CREATE EXTERNAL TABLE](../../ref_guide/sql_commands/CREATE_EXTERNAL_TABLE.html) command.

    EXTERNAL
    :   Optional. Defines the schema of the external table database objects created by `gpload`.

    The default is to use the Greenplum Database `search_path`.

    :   SCHEMA
:   Required when `EXTERNAL` is declared. The name of the schema of the external table. If the schema does not exist, an error is returned.

:   If `%` \(percent character\) is specified, the schema of the table name specified by `TABLE` in the `OUTPUT` section is used. If the table name does not specify a schema, the default schema is used.

    OUTPUT
    :   Required. Defines the target table and final data column values that are to be loaded into the database.

    TABLE
    :   Required. The name of the target table to load into.

    MODE
    :   Optional. Defaults to `INSERT` if not specified. There are three available load modes:

    INSERT - Loads data into the target table using the following method:

    ```
    INSERT INTO <target_table> SELECT * FROM <input_data>;
    ```

    UPDATE - Updates the `UPDATE_COLUMNS` of the target table where the rows have `MATCH_COLUMNS` attribute values equal to those of the input data, and the optional `UPDATE_CONDITION` is true. `UPDATE` is not supported if the target table column name is a reserved keyword, has capital letters, or includes any character that requires quotes \(" "\) to identify the column.

    MERGE - Inserts new rows and updates the `UPDATE_COLUMNS` of existing rows where `FOOBAR` attribute values are equal to those of the input data, and the optional `MATCH_COLUMNS` is true. New rows are identified when the `MATCH_COLUMNS` value in the source data does not have a corresponding value in the existing data of the target table. In those cases, the **entire row** from the source file is inserted, not only the `MATCH` and `UPDATE` columns. If there are multiple new `MATCH_COLUMNS` values that are the same, only one new row for that value will be inserted. Use `UPDATE_CONDITION` to filter out the rows to discard. `MERGE` is not supported if the target table column name is a reserved keyword, has capital letters, or includes any character that requires quotes \(" "\) to identify the column.

    MATCH\_COLUMNS
    :   Required if `MODE` is `UPDATE` or `MERGE`. Specifies the column\(s\) to use as the join condition for the update. The attribute value in the specified target column\(s\) must be equal to that of the corresponding source data column\(s\) in order for the row to be updated in the target table.

    UPDATE\_COLUMNS
    :   Required if `MODE` is `UPDATE` or `MERGE`. Specifies the column\(s\) to update for the rows that meet the `MATCH_COLUMNS` criteria and the optional `UPDATE_CONDITION`.

    UPDATE\_CONDITION
    :   Optional. Specifies a Boolean condition \(similar to what you would declare in a `WHERE` clause\) that must be met in order for a row in the target table to be updated.

    MAPPING
    :   Optional. If a mapping is specified, it overrides the default source-to-target column mapping. The default source-to-target mapping is based on a match of column names as defined in the source `COLUMNS` section and the column names of the target `TABLE`. A mapping is specified as either:

    `<target_column_name>: <source_column_name>`

    or

    `<target_column_name>: '<expression>'`

    Where <expression\> is any expression that you would specify in the `SELECT` list of a query, such as a constant value, a column reference, an operator invocation, a function call, and so on.

PRELOAD
:   Optional. Specifies operations to run prior to the load operation. Right now the only preload operation is `TRUNCATE`.

    TRUNCATE
    :   Optional. If set to true, `gpload` will remove all rows in the target table prior to loading it. Default is false.

    REUSE\_TABLES
    :   Optional. If set to true, `gpload` will not drop the external table objects and staging table objects it creates. These objects will be reused for future load operations that use the same load specifications. This improves performance of trickle loads \(ongoing small loads to the same target table\).

    If `LOG_ERRORS: true` is specified, `REUSE_TABLES: true` must be specified to retain the formatting errors in Greenplum Database error logs. If `REUSE_TABLES: true` is not specified, formatting error information is deleted after the `gpload` operation.

    If the <external\_table\_name\> exists, the utility uses the existing table. The utility returns an error if the table schema does not match the `OUTPUT` table schema.

    STAGING\_TABLE
    :   Optional. Specify the name of the temporary external table that is created during a `gpload` operation. The external table is used by `gpfdist`. `REUSE_TABLES: true` must also specified. If `REUSE_TABLES` is false or not specified, `STAGING_TABLE` is ignored. By default, `gpload` creates a temporary external table with a randomly generated name.

    If external\_table\_name contains a period \(.\), `gpload` returns an error. If the table exists, the utility uses the table. The utility returns an error if the existing table schema does not match the `OUTPUT` table schema.

    The utility uses the value of [SCHEMA](#cfschema) in the `EXTERNAL` section as the schema for <external\_table\_name\>. If the `SCHEMA` value is `%`, the schema for <external\_table\_name\> is the same as the schema of the target table, the schema of [TABLE](#cftable) in the `OUTPUT` section.

    If `SCHEMA` is not set, the utility searches for the table \(using the schemas in the database `search_path`\). If the table is not found, external\_table\_name is created in the default `PUBLIC` schema.

    `gpload` creates the staging table using the distribution key\(s\) of the target table as the distribution key\(s\) for the staging table. If the target table was created `DISTRIBUTED RANDOMLY`, `gpload` uses `MATCH_COLUMNS` as the staging table's distribution key\(s\).

    FAST\_MATCH
    :   Optional. If set to true, `gpload` only searches the database for matching external table objects when reusing external tables. The utility does not check the external table column names and column types in the catalog table `pg_attribute` to ensure that the table can be used for a `gpload` operation. Set the value to true to improve `gpload` performance when reusing external table objects and the database catalog table `pg_attribute` contains a large number of rows. The utility returns an error and quits if the column definitions are not compatible.

    The default value is false, the utility checks the external table definition column names and column types.

    `REUSE_TABLES: true` must also specified. If `REUSE_TABLES` is false or not specified and `FAST_MATCH: true` is specified, `gpload` returns a warning message.

SQL
:   Optional. Defines SQL commands to run before and/or after the load operation. You can specify multiple `BEFORE` and/or `AFTER` commands. List commands in the order of desired execution.

    BEFORE
    :   Optional. An SQL command to run before the load operation starts. Enclose commands in quotes.

    AFTER
    :   Optional. An SQL command to run after the load operation completes. Enclose commands in quotes.

## <a id="section9"></a>Log File Format 

Log files output by `gpload` have the following format:

```
<timestamp>|<level>|<message>
```

Where <timestamp\> takes the form: `YYYY-MM-DD HH:MM:SS`, level is one of `DEBUG`, `LOG`, `INFO`, `ERROR`, and message is a normal text message.

Some `INFO` messages that may be of interest in the log files are \(where \# corresponds to the actual number of seconds, units of data, or failed rows\):

```
INFO|running time: <#.##> seconds
INFO|transferred <#.#> kB of <#.#> kB.
INFO|gpload succeeded
INFO|gpload succeeded with warnings
INFO|gpload failed
INFO|1 bad row
INFO|<#> bad rows
```

## <a id="section8"></a>Notes 

If your database object names were created using a double-quoted identifier \(delimited identifier\), you must specify the delimited name within single quotes in the `gpload` control file. For example, if you create a table as follows:

```
CREATE TABLE "MyTable" ("MyColumn" text);
```

Your YAML-formatted `gpload` control file would refer to the above table and column names as follows:

```
- COLUMNS:
   - '"MyColumn"': text
OUTPUT:
   - TABLE: public.'"MyTable"'
```

If the YAML control file contains the `ERROR_TABLE` element that was available in Greenplum Database 4.3.x, `gpload` logs a warning stating that `ERROR_TABLE` is not supported, and load errors are handled as if the `LOG_ERRORS` and `REUSE_TABLE` elements were set to `true`. Rows with formatting errors are logged internally when running in single row error isolation mode.

## <a id="section10"></a>Examples 

Run a load job as defined in `my_load.yml`:

```
gpload -f my_load.yml
```

Example load control file:

```
---
VERSION: 1.0.0.1
DATABASE: ops
USER: gpadmin
HOST: cdw-1
PORT: 5432
GPLOAD:
   INPUT:
    - SOURCE:
         LOCAL_HOSTNAME:
           - etl1-1
           - etl1-2
           - etl1-3
           - etl1-4
         PORT: 8081
         FILE: 
           - /var/load/data/*
    - COLUMNS:
           - name: text
           - amount: float4
           - category: text
           - descr: text
           - date: date
    - FORMAT: text
    - DELIMITER: '|'
    - ERROR_LIMIT: 25
    - LOG_ERRORS: true
   OUTPUT:
    - TABLE: payables.expenses
    - MODE: INSERT
   PRELOAD:
    - REUSE_TABLES: true 
   SQL:
   - BEFORE: "INSERT INTO audit VALUES('start', current_timestamp)"
   - AFTER: "INSERT INTO audit VALUES('end', current_timestamp)"
```

## <a id="section11"></a>See Also 

[gpfdist](gpfdist.html), [CREATE EXTERNAL TABLE](../../ref_guide/sql_commands/CREATE_EXTERNAL_TABLE.html)

