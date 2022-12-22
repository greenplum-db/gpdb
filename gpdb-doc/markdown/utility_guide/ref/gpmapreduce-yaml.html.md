# gpmapreduce.yaml 

gpmapreduce configuration file.

## <a id="section2"></a>Synopsis 

```
%YAML 1.1
---
[VERSION](#VERSION): 1.0.0.2
[DATABASE](#DATABASE): dbname
[USER](#USER): db_username
[HOST](#HOST): coordinator_hostname
[PORT](#PORT): coordinator_port
```

```
  - [DEFINE](#DEFINE): 
  - [INPUT](#INPUT):
     [NAME](#NAME): input_name
     [FILE](#FILE): 
       - *hostname*: /path/to/file
     [GPFDIST](#GPFDIST):
       - *hostname*:port/file_pattern
     [TABLE](#TABLE): table_name
     [QUERY](#QUERY): SELECT_statement
     [EXEC](#EXEC): command_string
     [COLUMNS](#COLUMNS):
       - field_name data_type
     [FORMAT](#FORMAT): TEXT | CSV
     [DELIMITER](#DELIMITER): delimiter_character
     [ESCAPE](#ESCAPE): escape_character
     [NULL](#NULL): null_string
     [QUOTE](#QUOTE): csv_quote_character
     [ERROR\_LIMIT](#ERROR_LIMIT): integer
     [ENCODING](#ENCODING): database_encoding
```

```
  - [OUTPUT](#OUTPUT):
     [NAME](#OUTPUTNAME): output_name
     [FILE](#OUTPUTFILE): file_path_on_client
     [TABLE](#OUTPUTTABLE): table_name
     [KEYS](#KEYS):        - column_name
     [MODE](#MODE): REPLACE | APPEND
```

```
  - [MAP](#MAP):
     [NAME](#NAME): function_name
     [FUNCTION](#FUNCTION): function_definition
     [LANGUAGE](#LANGUAGE): perl | python | c
     [LIBRARY](#LIBRARY): /path/filename.so
     [PARAMETERS](#PARAMETERS): 
       - nametype
     [RETURNS](#RETURNS): 
       - nametype
     [OPTIMIZE](#OPTIMIZE): STRICT IMMUTABLE
     [MODE](#MODE): SINGLE | MULTI
```

```
  - [TRANSITION \| CONSOLIDATE \| FINALIZE](#TCF):
     [NAME](#TCFNAME): function_name
     [FUNCTION](#FUNCTION): function_definition
     [LANGUAGE](#LANGUAGE): perl | python | c
     [LIBRARY](#LIBRARY): /path/filename.so
     [PARAMETERS](#PARAMETERS): 
       - nametype
     [RETURNS](#RETURNS): 
       - nametype
     [OPTIMIZE](#OPTIMIZE): STRICT IMMUTABLE
     [MODE](#TCFMODE): SINGLE | MULTI
```

```
  - [REDUCE](#REDUCE):
     [NAME](#REDUCENAME): reduce_job_name
     [TRANSITION](#TRANSITION): transition_function_name
     [CONSOLIDATE](#CONSOLIDATE): consolidate_function_name
     [FINALIZE](#FINALIZE): finalize_function_name
     [INITIALIZE](#INITIALIZE): value
     [KEYS](#REDUCEKEYS):
       - key_name
```

```
  - [TASK](#TASK):
     [NAME](#TASKNAME): task_name
     [SOURCE](#SOURCE): input_name
     [MAP](#TASKMAP): map_function_name
     [REDUCE](#REDUCE): reduce_function_name
[EXECUTE](#EXECUTE):
```

```
  - [RUN](#RUN):
     [SOURCE](#EXECUTESOURCE): input_or_task_name
     [TARGET](#TARGET): output_name
     [MAP](#EXECUTEMAP): map_function_name
     [REDUCE](#EXECUTEREDUCE): reduce_function_name...
```

## <a id="section4"></a>Description 

You specify the input, map and reduce tasks, and the output for the Greenplum MapReduce `gpmapreduce` program in a YAML-formatted configuration file. \(This reference page uses the name `gpmapreduce.yaml` when referring to this file; you may choose your own name for the file.\)

The `gpmapreduce` utility processes the YAML configuration file in order, using indentation \(spaces\) to determine the document hierarchy and the relationships between the sections. The use of white space in the file is significant.

## <a id="section5"></a>Keys and Values 

VERSION
:   Required. The version of the Greenplum MapReduce YAML specification. Current supported versions are 1.0.0.1, 1.0.0.2, and 1.0.0.3.

DATABASE
:   Optional. Specifies which database in Greenplum to connect to. If not specified, defaults to the default database or `$PGDATABASE` if set.

USER
:   Optional. Specifies which database role to use to connect. If not specified, defaults to the current user or `$PGUSER` if set. You must be a Greenplum superuser to run functions written in untrusted Python and Perl. Regular database users can run functions written in trusted Perl. You also must be a database superuser to run MapReduce jobs that contain [FILE](#FILE), [GPFDIST](#GPFDIST) and [EXEC](#EXEC) input types.

HOST
:   Optional. Specifies Greenplum coordinator host name. If not specified, defaults to localhost or `$PGHOST` if set.

PORT
:   Optional. Specifies Greenplum coordinator port. If not specified, defaults to 5432 or `$PGPORT` if set.

DEFINE
:   Required. A sequence of definitions for this MapReduce document. The `DEFINE` section must have at least one `INPUT` definition.

    INPUT
    :   Required. Defines the input data. Every MapReduce document must have at least one input defined. Multiple input definitions are allowed in a document, but each input definition can specify only one of these access types: a file, a `gpfdist` file reference, a table in the database, an SQL command, or an operating system command. See `` for information about this reference.

    NAME
    :   A name for this input. Names must be unique with regards to the names of other objects in this MapReduce job \(such as map function, task, reduce function and output names\). Also, names cannot conflict with existing objects in the database \(such as tables, functions or views\).

    FILE
    :   A sequence of one or more input files in the format: `seghostname:/path/to/filename`. You must be a Greenplum Database superuser to run MapReduce jobs with `FILE` input. The file must reside on a Greenplum segment host.

    GPFDIST
    :   A sequence identifying one or more running `gpfdist` file servers in the format: `hostname[:port]/file_pattern`. You must be a Greenplum Database superuser to run MapReduce jobs with `GPFDIST` input.

    TABLE
    :   The name of an existing table in the database.

    QUERY
    :   A SQL `SELECT` command to run within the database.

    EXEC
    :   An operating system command to run on the Greenplum segment hosts. The command is run by all segment instances in the system by default. For example, if you have four segment instances per segment host, the command will be run four times on each host. You must be a Greenplum Database superuser to run MapReduce jobs with `EXEC` input.

    COLUMNS
    :   Optional. Columns are specified as: `column_name``[``data_type``]`. If not specified, the default is `value text`. The [DELIMITER](#DELIMITER) character is what separates two data value fields \(columns\). A row is determined by a line feed character \(`0x0a`\).

    FORMAT
    :   Optional. Specifies the format of the data - either delimited text \(`TEXT`\) or comma separated values \(`CSV`\) format. If the data format is not specified, defaults to `TEXT`.

    DELIMITER
    :   Optional for [FILE](#FILE), [GPFDIST](#GPFDIST) and [EXEC](#EXEC) inputs. Specifies a single character that separates data values. The default is a tab character in `TEXT` mode, a comma in `CSV` mode. The delimiter character must only appear between any two data value fields. Do not place a delimiter at the beginning or end of a row.

    ESCAPE
    :   Optional for [FILE](#FILE), [GPFDIST](#GPFDIST) and [EXEC](#EXEC) inputs. Specifies the single character that is used for C escape sequences \(such as `\n`,`\t`,`\100`, and so on\) and for escaping data characters that might otherwise be taken as row or column delimiters. Make sure to choose an escape character that is not used anywhere in your actual column data. The default escape character is a \\ \(backslash\) for text-formatted files and a `"` \(double quote\) for csv-formatted files, however it is possible to specify another character to represent an escape. It is also possible to deactivate escaping by specifying the value `'OFF'` as the escape value. This is very useful for data such as text-formatted web log data that has many embedded backslashes that are not intended to be escapes.

    NULL
    :   Optional for [FILE](#FILE), [GPFDIST](#GPFDIST) and [EXEC](#EXEC) inputs. Specifies the string that represents a null value. The default is `\N` in `TEXT` format, and an empty value with no quotations in `CSV` format. You might prefer an empty string even in `TEXT` mode for cases where you do not want to distinguish nulls from empty strings. Any input data item that matches this string will be considered a null value.

    QUOTE
    :   Optional for [FILE](#FILE), [GPFDIST](#GPFDIST) and [EXEC](#EXEC) inputs. Specifies the quotation character for `CSV` formatted files. The default is a double quote \(`"`\). In `CSV` formatted files, data value fields must be enclosed in double quotes if they contain any commas or embedded new lines. Fields that contain double quote characters must be surrounded by double quotes, and the embedded double quotes must each be represented by a pair of consecutive double quotes. It is important to always open and close quotes correctly in order for data rows to be parsed correctly.

    ERROR\_LIMIT
    :   If the input rows have format errors they will be discarded provided that the error limit count is not reached on any Greenplum segment instance during input processing. If the error limit is not reached, all good rows will be processed and any error rows discarded.

    ENCODING
    :   Character set encoding to use for the data. Specify a string constant \(such as `'SQL_ASCII'`\), an integer encoding number, or `DEFAULT` to use the default client encoding. See [Character Set Support](../../ref_guide/character_sets.html) for more information.

    OUTPUT
    :   Optional. Defines where to output the formatted data of this MapReduce job. If output is not defined, the default is `STDOUT` \(standard output of the client\). You can send output to a file on the client host or to an existing table in the database.

    NAME
    :   A name for this output. The default output name is `STDOUT`. Names must be unique with regards to the names of other objects in this MapReduce job \(such as map function, task, reduce function and input names\). Also, names cannot conflict with existing objects in the database \(such as tables, functions or views\).

    FILE
    :   Specifies a file location on the MapReduce client machine to output data in the format: `/path/to/filename`.

    TABLE
    :   Specifies the name of a table in the database to output data. If this table does not exist prior to running the MapReduce job, it will be created using the distribution policy specified with [KEYS](#KEYS).

    KEYS
    :   Optional for [TABLE](#OUTPUTTABLE) output. Specifies the column\(s\) to use as the Greenplum Database distribution key. If the [EXECUTE](#EXECUTE) task contains a [REDUCE](#REDUCE) definition, then the `REDUCE` keys will be used as the table distribution key by default. Otherwise, the first column of the table will be used as the distribution key.

    MODE
    :   Optional for [TABLE](#OUTPUTTABLE) output. If not specified, the default is to create the table if it does not already exist, but error out if it does exist. Declaring `APPEND` adds output data to an existing table \(provided the table schema matches the output format\) without removing any existing data. Declaring `REPLACE` will drop the table if it exists and then recreate it. Both `APPEND` and `REPLACE` will create a new table if one does not exist.

    MAP
    :   Required. Each `MAP` function takes data structured in \(`key`, `value`\) pairs, processes each pair, and generates zero or more output \(`key`, `value`\) pairs. The Greenplum MapReduce framework then collects all pairs with the same key from all output lists and groups them together. This output is then passed to the [REDUCE](#TASKREDUCE) task, which is comprised of [TRANSITION \| CONSOLIDATE \| FINALIZE](#TCF) functions. There is one predefined `MAP` function named `IDENTITY` that returns \(`key`, `value`\) pairs unchanged. Although \(`key`, `value`\) are the default parameters, you can specify other prototypes as needed.

    TRANSITION \| CONSOLIDATE \| FINALIZE
    :   `TRANSITION`, `CONSOLIDATE` and `FINALIZE` are all component pieces of [REDUCE](#REDUCE). A `TRANSITION` function is required. `CONSOLIDATE` and `FINALIZE` functions are optional. By default, all take `state` as the first of their input [PARAMETERS](#PARAMETERS), but other prototypes can be defined as well.

    A `TRANSITION` function iterates through each value of a given key and accumulates values in a `state` variable. When the transition function is called on the first value of a key, the `state` is set to the value specified by [INITIALIZE](#INITIALIZE) of a [REDUCE](#REDUCE) job \(or the default state value for the data type\). A transition takes two arguments as input; the current state of the key reduction, and the next value, which then produces a new `state`.

    If a `CONSOLIDATE` function is specified, `TRANSITION` processing is performed at the segment-level before redistributing the keys across the Greenplum interconnect for final aggregation \(two-phase aggregation\). Only the resulting `state` value for a given key is redistributed, resulting in lower interconnect traffic and greater parallelism. `CONSOLIDATE` is handled like a `TRANSITION`, except that instead of `(state + value) => state`, it is `(state + state) => state`.

    If a `FINALIZE` function is specified, it takes the final `state` produced by `CONSOLIDATE` \(if present\) or `TRANSITION` and does any final processing before emitting the final result. `TRANSITION` and `CONSOLIDATE`functions cannot return a set of values. If you need a `REDUCE` job to return a set, then a `FINALIZE` is necessary to transform the final state into a set of output values.

    NAME
    :   Required. A name for the function. Names must be unique with regards to the names of other objects in this MapReduce job \(such as function, task, input and output names\). You can also specify the name of a function built-in to Greenplum Database. If using a built-in function, do not supply [LANGUAGE](#LANGUAGE) or a [FUNCTION](#FUNCTION) body.

    FUNCTION
    :   Optional. Specifies the full body of the function using the specified [LANGUAGE](#LANGUAGE). If `FUNCTION` is not specified, then a built-in database function corresponding to [NAME](#TCFNAME) is used.

    LANGUAGE
    :   Required when [FUNCTION](#FUNCTION) is used. Specifies the implementation language used to interpret the function. This release has language support for `perl`, `python`, and `C`. If calling a built-in database function, `LANGUAGE` should not be specified.

    LIBRARY
    :   Required when [LANGUAGE](#LANGUAGE) is C \(not allowed for other language functions\). To use this attribute, [VERSION](#VERSION) must be 1.0.0.2. The specified library file must be installed prior to running the MapReduce job, and it must exist in the same file system location on all Greenplum hosts \(coordinator and segments\).

    PARAMETERS
    :   Optional. Function input parameters. The default type is `text`.

    `MAP` default - `key text`, `value text`

    `TRANSITION` default - `state text`, `value text`

    `CONSOLIDATE` default - `state1 text`, `state2 text` \(must have exactly two input parameters of the same data type\)

    `FINALIZE` default - `state text` \(single parameter only\)

    RETURNS
    :   Optional. The default return type is `text`.

    `MAP` default - `key text`, `value text`

    `TRANSITION` default - `state text` \(single return value only\)

    `CONSOLIDATE` default - `state text` \(single return value only\)

    `FINALIZE` default - `value text`

    OPTIMIZE
    :   Optional optimization parameters for the function:

    `STRICT` - function is not affected by `NULL` values

    `IMMUTABLE` - function will always return the same value for a given input

    MODE
    :   Optional. Specifies the number of rows returned by the function.

    `MULTI` - returns 0 or more rows per input record. The return value of the function must be an array of rows to return, or the function must be written as an iterator using `yield` in Python or `return_next` in Perl. `MULTI` is the default mode for `MAP` and `FINALIZE` functions.

    `SINGLE` - returns exactly one row per input record. `SINGLE` is the only mode supported for `TRANSITION` and `CONSOLIDATE` functions. When used with `MAP` and `FINALIZE` functions, `SINGLE` mode can provide modest performance improvement.

    REDUCE
    :   Required. A `REDUCE` definition names the [TRANSITION \| CONSOLIDATE \| FINALIZE](#TCF) functions that comprise the reduction of \(`key`, `value`\) pairs to the final result set. There are also several predefined `REDUCE` jobs you can run, which all operate over a column named `value`:

    `IDENTITY` - returns \(key, value\) pairs unchanged

    `SUM` - calculates the sum of numeric data

    `AVG` - calculates the average of numeric data

    `COUNT` - calculates the count of input data

    `MIN` - calculates minimum value of numeric data

    `MAX` - calculates maximum value of numeric data

    NAME
    :   Required. The name of this `REDUCE` job. Names must be unique with regards to the names of other objects in this MapReduce job \(function, task, input and output names\). Also, names cannot conflict with existing objects in the database \(such as tables, functions or views\).

    TRANSITION
    :   Required. The name of the `TRANSITION` function.

    CONSOLIDATE
    :   Optional. The name of the `CONSOLIDATE` function.

    FINALIZE
    :   Optional. The name of the `FINALIZE` function.

    INITIALIZE
    :   Optional for `text` and `float` data types. Required for all other data types. The default value for text is `''` . The default value for float is `0.0` . Sets the initial `state` value of the `TRANSITION` function.

    KEYS
    :   Optional. Defaults to `[key, *]`. When using a multi-column reduce it may be necessary to specify which columns are key columns and which columns are value columns. By default, any input columns that are not passed to the `TRANSITION` function are key columns, and a column named `key` is always a key column even if it is passed to the `TRANSITION` function. The special indicator `*` indicates all columns not passed to the `TRANSITION` function. If this indicator is not present in the list of keys then any unmatched columns are discarded.

    TASK
    :   Optional. A `TASK` defines a complete end-to-end `INPUT`/`MAP`/`REDUCE` stage within a Greenplum MapReduce job pipeline. It is similar to [EXECUTE](#EXECUTE) except it is not immediately run. A task object can be called as [INPUT](#INPUT) to further processing stages.

    NAME
    :   Required. The name of this task. Names must be unique with regards to the names of other objects in this MapReduce job \(such as map function, reduce function, input and output names\). Also, names cannot conflict with existing objects in the database \(such as tables, functions or views\).

    SOURCE
    :   The name of an [INPUT](#INPUT) or another `TASK`.

    MAP
    :   Optional. The name of a [MAP](#MAP) function. If not specified, defaults to `IDENTITY`.

    REDUCE
    :   Optional. The name of a [REDUCE](#REDUCE) function. If not specified, defaults to `IDENTITY`.

EXECUTE
:   Required. `EXECUTE` defines the final `INPUT`/`MAP`/`REDUCE` stage within a Greenplum MapReduce job pipeline.

    RUN
    :   SOURCE
:   Required. The name of an [INPUT](#INPUT) or [TASK](#TASK).

TARGET
:   Optional. The name of an [OUTPUT](#OUTPUT). The default output is `STDOUT`.

MAP
:   Optional. The name of a [MAP](#MAP) function. If not specified, defaults to `IDENTITY`.

REDUCE
:   Optional. The name of a [REDUCE](#REDUCE) function. Defaults to `IDENTITY`.

## <a id="section11"></a>See Also 

[gpmapreduce](gpmapreduce.html)

