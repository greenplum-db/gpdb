# CREATE EXTERNAL TABLE 

Defines a new external table.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
CREATE [READABLE] EXTERNAL [TEMPORARY | TEMP] TABLE <table_name>     
    ( <column_name> <data_type> [, ...] | LIKE <other_table >)
     LOCATION ('file://<seghost>[:<port>]/<path>/<file>' [, ...])
       | ('gpfdist://<filehost>[:<port>]/<file_pattern>[#transform=<trans_name>]'
           [, ...]
       | ('gpfdists://<filehost>[:<port>]/<file_pattern>[#transform=<trans_name>]'
           [, ...])
       | ('pxf://<path-to-data>?PROFILE=<profile_name>[&SERVER=<server_name>][&<custom-option>=<value>[...]]'))
       | ('s3://<S3_endpoint>[:<port>]/<bucket_name>/[<S3_prefix>] [region=<S3-region>] [config=<config_file> | config_server=<url>]')
     [ON MASTER]
     FORMAT 'TEXT' 
           [( [HEADER]
              [DELIMITER [AS] '<delimiter>' | 'OFF']
              [NULL [AS] '<null string>']
              [ESCAPE [AS] '<escape>' | 'OFF']
              [NEWLINE [ AS ] 'LF' | 'CR' | 'CRLF']
              [FILL MISSING FIELDS] )]
          | 'CSV'
           [( [HEADER]
              [QUOTE [AS] '<quote>'] 
              [DELIMITER [AS] '<delimiter>']
              [NULL [AS] '<null string>']
              [FORCE NOT NULL <column> [, ...]]
              [ESCAPE [AS] '<escape>']
              [NEWLINE [ AS ] 'LF' | 'CR' | 'CRLF']
              [FILL MISSING FIELDS] )]
          | 'CUSTOM' (Formatter=<<formatter_specifications>>)
    [ OPTIONS ( <key> '<value>' [, ...] ) ]
    [ ENCODING '<encoding>' ]
      [ [LOG ERRORS [PERSISTENTLY]] SEGMENT REJECT LIMIT <count>
      [ROWS | PERCENT] ]

CREATE [READABLE] EXTERNAL WEB [TEMPORARY | TEMP] TABLE <table_name>     
   ( <column_name> <data_type> [, ...] | LIKE <other_table >)
      LOCATION ('http://<webhost>[:<port>]/<path>/<file>' [, ...])
    | EXECUTE '<command>' [ON ALL 
                          | MASTER
                          | <number_of_segments>
                          | HOST ['<segment_hostname>'] 
                          | SEGMENT <segment_id> ]
      FORMAT 'TEXT' 
            [( [HEADER]
               [DELIMITER [AS] '<delimiter>' | 'OFF']
               [NULL [AS] '<null string>']
               [ESCAPE [AS] '<escape>' | 'OFF']
               [NEWLINE [ AS ] 'LF' | 'CR' | 'CRLF']
               [FILL MISSING FIELDS] )]
           | 'CSV'
            [( [HEADER]
               [QUOTE [AS] '<quote>'] 
               [DELIMITER [AS] '<delimiter>']
               [NULL [AS] '<null string>']
               [FORCE NOT NULL <column> [, ...]]
               [ESCAPE [AS] '<escape>']
               [NEWLINE [ AS ] 'LF' | 'CR' | 'CRLF']
               [FILL MISSING FIELDS] )]
           | 'CUSTOM' (Formatter=<<formatter specifications>>)
     [ OPTIONS ( <key> '<value>' [, ...] ) ]
     [ ENCODING '<encoding>' ]
     [ [LOG ERRORS [PERSISTENTLY]] SEGMENT REJECT LIMIT <count>
       [ROWS | PERCENT] ]

CREATE WRITABLE EXTERNAL [TEMPORARY | TEMP] TABLE <table_name>
    ( <column_name> <data_type> [, ...] | LIKE <other_table >)
     LOCATION('gpfdist://<outputhost>[:<port>]/<filename>[#transform=<trans_name>]'
          [, ...])
      | ('gpfdists://<outputhost>[:<port>]/<file_pattern>[#transform=<trans_name>]'
          [, ...])
      FORMAT 'TEXT' 
               [( [DELIMITER [AS] '<delimiter>']
               [NULL [AS] '<null string>']
               [ESCAPE [AS] '<escape>' | 'OFF'] )]
          | 'CSV'
               [([QUOTE [AS] '<quote>'] 
               [DELIMITER [AS] '<delimiter>']
               [NULL [AS] '<null string>']
               [FORCE QUOTE <column> [, ...]] | * ]
               [ESCAPE [AS] '<escape>'] )]

           | 'CUSTOM' (Formatter=<<formatter specifications>>)
    [ OPTIONS ( <key> '<value>' [, ...] ) ]
    [ ENCODING '<write_encoding>' ]
    [ DISTRIBUTED BY ({<column> [<opclass>]}, [ ... ] ) | DISTRIBUTED RANDOMLY ]

CREATE WRITABLE EXTERNAL [TEMPORARY | TEMP] TABLE <table_name>
    ( <column_name> <data_type> [, ...] | LIKE <other_table >)
     LOCATION('s3://<S3_endpoint>[:<port>]/<bucket_name>/[<S3_prefix>] [region=<S3-region>] [config=<config_file> | config_server=<url>]')
      [ON MASTER]
      FORMAT 'TEXT' 
               [( [DELIMITER [AS] '<delimiter>']
               [NULL [AS] '<null string>']
               [ESCAPE [AS] '<escape>' | 'OFF'] )]
          | 'CSV'
               [([QUOTE [AS] '<quote>'] 
               [DELIMITER [AS] '<delimiter>']
               [NULL [AS] '<null string>']
               [FORCE QUOTE <column> [, ...]] | * ]
               [ESCAPE [AS] '<escape>'] )]

CREATE WRITABLE EXTERNAL WEB [TEMPORARY | TEMP] TABLE <table_name>
    ( <column_name> <data_type> [, ...] | LIKE <other_table> )
    EXECUTE '<command>' [ON ALL]
    FORMAT 'TEXT' 
               [( [DELIMITER [AS] '<delimiter>']
               [NULL [AS] '<null string>']
               [ESCAPE [AS] '<escape>' | 'OFF'] )]
          | 'CSV'
               [([QUOTE [AS] '<quote>'] 
               [DELIMITER [AS] '<delimiter>']
               [NULL [AS] '<null string>']
               [FORCE QUOTE <column> [, ...]] | * ]
               [ESCAPE [AS] '<escape>'] )]
           | 'CUSTOM' (Formatter=<<formatter specifications>>)
    [ OPTIONS ( <key> '<value>' [, ...] ) ]
    [ ENCODING '<write_encoding>' ]
    [ DISTRIBUTED BY ({<column> [<opclass>]}, [ ... ] ) | DISTRIBUTED RANDOMLY ]
```

## <a id="section3"></a>Description 

`CREATE EXTERNAL TABLE` or `CREATE EXTERNAL WEB TABLE` creates a new readable external table definition in Greenplum Database. Readable external tables are typically used for fast, parallel data loading. Once an external table is defined, you can query its data directly \(and in parallel\) using SQL commands. For example, you can select, join, or sort external table data. You can also create views for external tables. DML operations \(`UPDATE`, `INSERT`, `DELETE`, or `TRUNCATE`\) are not allowed on readable external tables, and you cannot create indexes on readable external tables.

`CREATE WRITABLE EXTERNAL TABLE` or `CREATE WRITABLE EXTERNAL WEB TABLE` creates a new writable external table definition in Greenplum Database. Writable external tables are typically used for unloading data from the database into a set of files or named pipes. Writable external web tables can also be used to output data to an executable program. Writable external tables can also be used as output targets for Greenplum parallel MapReduce calculations. Once a writable external table is defined, data can be selected from database tables and inserted into the writable external table. Writable external tables only allow `INSERT` operations – `SELECT`, `UPDATE`, `DELETE` or `TRUNCATE` are not allowed.

The main difference between regular external tables and external web tables is their data sources. Regular readable external tables access static flat files, whereas external web tables access dynamic data sources – either on a web server or by running OS commands or scripts.

See [Working with External Data](../../admin_guide/external/g-working-with-file-based-ext-tables.html) for detailed information about working with external tables.

## <a id="section4"></a>Parameters 

READABLE \| WRITABLE
:   Specifies the type of external table, readable being the default. Readable external tables are used for loading data into Greenplum Database. Writable external tables are used for unloading data.

WEB
:   Creates a readable or writable external web table definition in Greenplum Database. There are two forms of readable external web tables – those that access files via the `http://` protocol or those that access data by running OS commands. Writable external web tables output data to an executable program that can accept an input stream of data. External web tables are not rescannable during query execution.

:   The `s3` protocol does not support external web tables. You can, however, create an external web table that runs a third-party tool to read data from or write data to S3 directly.

TEMPORARY \| TEMP
:   If specified, creates a temporary readable or writable external table definition in Greenplum Database. Temporary external tables exist in a special schema; you cannot specify a schema name when you create the table. Temporary external tables are automatically dropped at the end of a session.

:   An existing permanent table with the same name is not visible to the current session while the temporary table exists, unless you reference the permanent table with its schema-qualified name.

table\_name
:   The name of the new external table.

column\_name
:   The name of a column to create in the external table definition. Unlike regular tables, external tables do not have column constraints or default values, so do not specify those.

LIKE other\_table
:   The `LIKE` clause specifies a table from which the new external table automatically copies all column names, data types and Greenplum distribution policy. If the original table specifies any column constraints or default column values, those will not be copied over to the new external table definition.

data\_type
:   The data type of the column.

LOCATION \('protocol://\[host\[:port\]\]/path/file' \[, ...\]\)
:   If you use the `pxf` protocol to access an external data source, refer to [pxf:// Protocol](../../admin_guide/external/g-pxf-protocol.html) for information about the `pxf` protocol.

:   If you use the `s3` protocol to read or write to S3, refer to [s3:// Protocol](../../admin_guide/external/g-s3-protocol.html#amazon-emr/section_stk_c2r_kx) for additional information about the `s3` protocol `LOCATION` clause syntax.

:   For readable external tables, specifies the URI of the external data source\(s\) to be used to populate the external table or web table. Regular readable external tables allow the `gpfdist` or `file` protocols. External web tables allow the `http` protocol. If `port` is omitted, port `8080` is assumed for `http` and `gpfdist` protocols. If using the `gpfdist` protocol, the `path` is relative to the directory from which `gpfdist` is serving files \(the directory specified when you started the `gpfdist` program\). Also, `gpfdist` can use wildcards or other C-style pattern matching \(for example, a whitespace character is `[[:space:]]`\) to denote multiple files in a directory. For example:

    ```
    'gpfdist://filehost:8081/*'
    'gpfdist://masterhost/my_load_file'
    'file://seghost1/dbfast1/external/myfile.txt'
    'http://intranet.example.com/finance/expenses.csv'
    ```

     For writable external tables, specifies the URI location of the `gpfdist` process or S3 protocol that will collect data output from the Greenplum segments and write it to one or more named files. For `gpfdist` the `path` is relative to the directory from which `gpfdist` is serving files \(the directory specified when you started the `gpfdist` program\). If multiple `gpfdist` locations are listed, the segments sending data will be evenly divided across the available output locations. For example:

    ```
    'gpfdist://outputhost:8081/data1.out',
    'gpfdist://outputhost:8081/data2.out'
    ```

    With two `gpfdist` locations listed as in the above example, half of the segments would send their output data to the `data1.out` file and the other half to the `data2.out` file.

    With the option `#transform=trans\_name`, you can specify a transform to apply when loading or extracting data. The trans\_name is the name of the transform in the YAML configuration file you specify with the you run the `gpfdist` utility. For information about specifying a transform, see [`gpfdist`](../../utility_guide/ref/gpfdist.html) in the *Greenplum Utility Guide*.

ON MASTER
:   Restricts all table-related operations to the Greenplum coordinator segment. Permitted only on readable and writable external tables created with the `s3` or custom protocols. The `gpfdist`, `gpfdists`, `pxf`, and `file` protocols do not support `ON MASTER`.

:   > **Note** Be aware of potential resource impacts when reading from or writing to external tables you create with the `ON MASTER` clause. You may encounter performance issues when you restrict table operations solely to the Greenplum coordinator segment.

EXECUTE 'command' \[ON ...\]
:   Allowed for readable external web tables or writable external tables only. For readable external web tables, specifies the OS command to be run by the segment instances. The command can be a single OS command or a script. The `ON` clause is used to specify which segment instances will run the given command.

    -   ON ALL is the default. The command will be run by every active \(primary\) segment instance on all segment hosts in the Greenplum Database system. If the command runs a script, that script must reside in the same location on all of the segment hosts and be executable by the Greenplum superuser \(`gpadmin`\).
    -   ON MASTER runs the command on the coordinator host only.
        > **Note** Logging is not supported for external web tables when the `ON MASTER` clause is specified.

    -   ON number means the command will be run by the specified number of segments. The particular segments are chosen randomly at runtime by the Greenplum Database system. If the command runs a script, that script must reside in the same location on all of the segment hosts and be executable by the Greenplum superuser \(`gpadmin`\).
    -   HOST means the command will be run by one segment on each segment host \(once per segment host\), regardless of the number of active segment instances per host.
    -   HOST segment\_hostname means the command will be run by all active \(primary\) segment instances on the specified segment host.
    -   SEGMENT segment\_id means the command will be run only once by the specified segment. You can determine a segment instance's ID by looking at the content number in the system catalog table [gp\_segment\_configuration](../system_catalogs/gp_segment_configuration.html). The content ID of the Greenplum Database coordinator is always `-1`.

    For writable external tables, the command specified in the `EXECUTE` clause must be prepared to have data piped into it. Since all segments that have data to send will write their output to the specified command or program, the only available option for the `ON` clause is `ON ALL`.

FORMAT 'TEXT \| CSV' \(options\)
:   When the `FORMAT` clause identfies delimited text \(`TEXT`\) or comma separated values \(`CSV`\) format, formatting options are similar to those available with the PostgreSQL [COPY](COPY.html) command. If the data in the file does not use the default column delimiter, escape character, null string and so on, you must specify the additional formatting options so that the data in the external file is read correctly by Greenplum Database. For information about using a custom format, see "Loading and Unloading Data" in the *Greenplum Database Administrator Guide*.

:   If you use the `pxf` protocol to access an external data source, refer to [Accessing External Data with PXF](../../admin_guide/external/pxf-overview.html) for information about using PXF.

FORMAT 'CUSTOM' \(formatter=formatter\_specification\)
:   Specifies a custom data format. The formatter\_specification specifies the function to use to format the data, followed by comma-separated parameters to the formatter function. The length of the formatter specification, the string including `Formatter=`, can be up to approximately 50K bytes.

:   If you use the `pxf` protocol to access an external data source, refer to [Accessing External Data with PXF](../../admin_guide/external/pxf-overview.html) for information about using PXF.

:   For general information about using a custom format, see "Loading and Unloading Data" in the *Greenplum Database Administrator Guide*.

DELIMITER
:   Specifies a single ASCII character that separates columns within each row \(line\) of data. The default is a tab character in `TEXT` mode, a comma in `CSV` mode. In `TEXT` mode for readable external tables, the delimiter can be set to `OFF` for special use cases in which unstructured data is loaded into a single-column table.

:   For the `s3` protocol, the delimiter cannot be a newline character \(`\n`\) or a carriage return character \(`\r`\).

NULL
:   Specifies the string that represents a `NULL` value. The default is `\N` \(backslash-N\) in `TEXT` mode, and an empty value with no quotations in `CSV` mode. You might prefer an empty string even in `TEXT` mode for cases where you do not want to distinguish `NULL` values from empty strings. When using external and web tables, any data item that matches this string will be considered a `NULL` value.

:   As an example for the `text` format, this `FORMAT` clause can be used to specify that the string of two single quotes \(`''`\) is a `NULL` value.

:   ```
FORMAT 'text' (delimiter ',' null '\'\'\'\'' )
```

ESCAPE
:   Specifies the single character that is used for C escape sequences \(such as `\n`,`\t`,`\100`, and so on\) and for escaping data characters that might otherwise be taken as row or column delimiters. Make sure to choose an escape character that is not used anywhere in your actual column data. The default escape character is a \\ \(backslash\) for text-formatted files and a `"` \(double quote\) for csv-formatted files, however it is possible to specify another character to represent an escape. It is also possible to deactivate escaping in text-formatted files by specifying the value `'OFF'` as the escape value. This is very useful for data such as text-formatted web log data that has many embedded backslashes that are not intended to be escapes.

NEWLINE
:   Specifies the newline used in your data files – `LF` \(Line feed, 0x0A\), `CR` \(Carriage return, 0x0D\), or `CRLF` \(Carriage return plus line feed, 0x0D 0x0A\). If not specified, a Greenplum Database segment will detect the newline type by looking at the first row of data it receives and using the first newline type encountered.

HEADER
:   For readable external tables, specifies that the first line in the data file\(s\) is a header row \(contains the names of the table columns\) and should not be included as data for the table. If using multiple data source files, all files must have a header row.

:   For the `s3` protocol, the column names in the header row cannot contain a newline character \(`\n`\) or a carriage return \(`\r`\).

:   The `pxf` protocol does not support the `HEADER` formatting option.

QUOTE
:   Specifies the quotation character for `CSV` mode. The default is double-quote \(`"`\).

FORCE NOT NULL
:   In `CSV` mode, processes each specified column as though it were quoted and hence not a `NULL` value. For the default null string in `CSV` mode \(nothing between two delimiters\), this causes missing values to be evaluated as zero-length strings.

FORCE QUOTE
:   In `CSV` mode for writable external tables, forces quoting to be used for all non-`NULL` values in each specified column. If `*` is specified then non-`NULL` values will be quoted in all columns. `NULL` output is never quoted.

FILL MISSING FIELDS
:   In both `TEXT` and `CSV` mode for readable external tables, specifying `FILL MISSING FIELDS` will set missing trailing field values to `NULL` \(instead of reporting an error\) when a row of data has missing data fields at the end of a line or row. Blank rows, fields with a `NOT NULL` constraint, and trailing delimiters on a line will still report an error.

OPTIONS key 'value'\[, key' value' ...\]
:   Optional. Specifies parameters and values as key-value pairs that are set to a custom data access protocol when the protocol is used as a external table protocol for an external table. It is the responsibility of the custom data access protocol to process and validate the key-value pairs.

ENCODING 'encoding'
:   Character set encoding to use for the external table. Specify a string constant \(such as `'SQL_ASCII'`\), an integer encoding number, or `DEFAULT` to use the default server encoding. See [Character Set Support](../character_sets.html).

LOG ERRORS \[PERSISTENTLY\]
:   This is an optional clause that can precede a `SEGMENT REJECT LIMIT` clause to log information about rows with formatting errors. The error log data is stored internally. If error log data exists for a specified external table, new data is appended to existing error log data. The error log data is not replicated to mirror segments.

:   The data is deleted when the external table is dropped unless you specify the keyword `PERSISTENTLY`. If the keyword is specified, the log data persists after the external table is dropped.

:   The error log data is accessed with the Greenplum Database built-in SQL function `gp_read_error_log()`, or with the SQL function `gp_read_persistent_error_log()` if the `PERSISTENTLY` keyword is specified.

:   If you use the `PERSISTENTLY` keyword, you must install the functions that manage the persistent error log information.

:   See [Notes](#section8) for information about the error log information and built-in functions for viewing and managing error log information.

SEGMENT REJECT LIMIT count \[ROWS \| PERCENT\]
:   Runs a `COPY FROM` operation in single row error isolation mode. If the input rows have format errors they will be discarded provided that the reject limit count is not reached on any Greenplum segment instance during the load operation. The reject limit count can be specified as number of rows \(the default\) or percentage of total rows \(1-100\). If `PERCENT` is used, each segment starts calculating the bad row percentage only after the number of rows specified by the parameter `gp_reject_percent_threshold` has been processed. The default for `gp_reject_percent_threshold` is 300 rows. Constraint errors such as violation of a `NOT NULL`, `CHECK`, or `UNIQUE` constraint will still be handled in "all-or-nothing" input mode. If the limit is not reached, all good rows will be loaded and any error rows discarded.

:   > **Note** When reading an external table, Greenplum Database limits the initial number of rows that can contain formatting errors if the `SEGMENT REJECT LIMIT` is not triggered first or is not specified. If the first 1000 rows are rejected, the `COPY` operation is stopped and rolled back.

The limit for the number of initial rejected rows can be changed with the Greenplum Database server configuration parameter `gp_initial_bad_row_limit`. See [Server Configuration Parameters](../config_params/guc_config.html) for information about the parameter.

DISTRIBUTED BY \(\{column \[opclass\]\}, \[ ... \] \)
DISTRIBUTED RANDOMLY
:   Used to declare the Greenplum Database distribution policy for a writable external table. By default, writable external tables are distributed randomly. If the source table you are exporting data from has a hash distribution policy, defining the same distribution key column\(s\) and operator class\(es\), `oplcass`, for the writable external table will improve unload performance by eliminating the need to move rows over the interconnect. When you issue an unload command such as `INSERT INTO wex_table SELECT * FROM source_table`, the rows that are unloaded can be sent directly from the segments to the output location if the two tables have the same hash distribution policy.

## <a id="section5"></a>Examples 

Start the `gpfdist` file server program in the background on port `8081` serving files from directory `/var/data/staging`:

```
gpfdist -p 8081 -d /var/data/staging -l /home/<gpadmin>/log &
```

Create a readable external table named `ext_customer` using the `gpfdist` protocol and any text formatted files \(`*.txt`\) found in the `gpfdist` directory. The files are formatted with a pipe \(`|`\) as the column delimiter and an empty space as `NULL`. Also access the external table in single row error isolation mode:

```
CREATE EXTERNAL TABLE ext_customer
   (id int, name text, sponsor text) 
   LOCATION ( 'gpfdist://filehost:8081/*.txt' ) 
   FORMAT 'TEXT' ( DELIMITER '|' NULL ' ')
   LOG ERRORS SEGMENT REJECT LIMIT 5;
```

Create the same readable external table definition as above, but with CSV formatted files:

```
CREATE EXTERNAL TABLE ext_customer 
   (id int, name text, sponsor text) 
   LOCATION ( 'gpfdist://filehost:8081/*.csv' ) 
   FORMAT 'CSV' ( DELIMITER ',' );
```

Create a readable external table named `ext_expenses` using the `file` protocol and several CSV formatted files that have a header row:

```
CREATE EXTERNAL TABLE ext_expenses (name text, date date, 
amount float4, category text, description text) 
LOCATION ( 
'file://seghost1/dbfast/external/expenses1.csv',
'file://seghost1/dbfast/external/expenses2.csv',
'file://seghost2/dbfast/external/expenses3.csv',
'file://seghost2/dbfast/external/expenses4.csv',
'file://seghost3/dbfast/external/expenses5.csv',
'file://seghost3/dbfast/external/expenses6.csv' 
)
FORMAT 'CSV' ( HEADER );
```

Create a readable external web table that runs a script once per segment host:

```
CREATE EXTERNAL WEB TABLE log_output (linenum int, message 
text)  EXECUTE '/var/load_scripts/get_log_data.sh' ON HOST 
 FORMAT 'TEXT' (DELIMITER '|');
```

Create a writable external table named `sales_out` that uses `gpfdist` to write output data to a file named `sales.out`. The files are formatted with a pipe \(`|`\) as the column delimiter and an empty space as `NULL`.

```
CREATE WRITABLE EXTERNAL TABLE sales_out (LIKE sales) 
   LOCATION ('gpfdist://etl1:8081/sales.out')
   FORMAT 'TEXT' ( DELIMITER '|' NULL ' ')
   DISTRIBUTED BY (txn_id);
```

Create a writable external web table that pipes output data received by the segments to an executable script named `to_adreport_etl.sh`:

```
CREATE WRITABLE EXTERNAL WEB TABLE campaign_out 
(LIKE campaign) 
 EXECUTE '/var/unload_scripts/to_adreport_etl.sh'
 FORMAT 'TEXT' (DELIMITER '|');
```

Use the writable external table defined above to unload selected data:

```
INSERT INTO campaign_out SELECT * FROM campaign WHERE 
customer_id=123;
```

## <a id="section8"></a>Notes 

When you specify the `LOG ERRORS` clause, Greenplum Database captures errors that occur while reading the external table data. For information about the error log format, see [Viewing Bad Rows in the Error Log](../../admin_guide/load/topics/g-viewing-bad-rows-in-the-error-table-or-error-log.html#topic58).

You can view and manage the captured error log data. The functions to manage log data depend on whether the data is persistent \(the `PERSISTENTLY` keyword is used with the `LOG ERRORS` clause\).

-   Functions that manage non-persistent error log data from external tables that were defined without the `PERSISTENTLY` keyword.
    -   The built-in SQL function `gp_read_error_log('table_name')` displays error log information for an external table. This example displays the error log data from the external table `ext_expenses`.

        ```
        SELECT * from gp_read_error_log('ext_expenses');
        ```

        The function returns no data if you created the external table with the `LOG ERRORS PERSISTENTLY` clause, or if the external table does not exist.

    -   The built-in SQL function `gp_truncate_error_log('table_name')` deletes the error log data for table\_name. This example deletes the error log data captured from the external table `ext_expenses`:

        ```
        SELECT gp_truncate_error_log('ext_expenses'); 
        ```

        Dropping the table also deletes the table's log data. The function does not truncate log data if the external table is defined with the `LOG ERRORS PERSISTENTLY` clause.

        The function returns `FALSE` if the table does not exist.

-   Functions that manage persistent error log data from external tables that were defined with the `PERSISTENTLY` keyword.
    -   The SQL function `gp_read_persistent_error_log('table_name')` displays persistent log data for an external table.

        The function returns no data if you created the external table without the `PERSISTENTLY` keyword. The function returns persistent log data for an external table even after the table has been dropped.

    -   The SQL function `gp_truncate_persistent_error_log('table_name')` truncates persistent log data for a table.

        For persistent log data, you must manually delete the data. Dropping the external table does not delete persistent log data.

-   These items apply to both non-persistent and persistent error log data and the related functions.
    -   The `gp_read_*` functions require `SELECT` privilege on the table.
    -   The `gp_truncate_*` functions require owner privilege on the table.
    -   You can use the `*` wildcard character to delete error log information for existing tables in the current database. Specify the string `*.*` to delete all database error log information, including error log information that was not deleted due to previous database issues. If `*` is specified, database owner privilege is required. If `*.*` is specified, operating system super-user privilege is required. Non-persistent and persistent error log data must be deleted with their respective `gp_truncate_*` functions.

When multiple Greenplum Database external tables are defined with the `gpfdist`, `gpfdists`, or `file` protocol and access the same named pipe a Linux system, Greenplum Database restricts access to the named pipe to a single reader. An error is returned if a second reader attempts to access the named pipe.

## <a id="section6"></a>Compatibility 

`CREATE EXTERNAL TABLE` is a Greenplum Database extension. The SQL standard makes no provisions for external tables.

## <a id="section7"></a>See Also 

[CREATE TABLE AS](CREATE_TABLE_AS.html), [CREATE TABLE](CREATE_TABLE.html), [COPY](COPY.html), [SELECT INTO](SELECT_INTO.html), [INSERT](INSERT.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

