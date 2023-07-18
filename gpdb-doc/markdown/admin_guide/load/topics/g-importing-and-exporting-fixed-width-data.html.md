---
title: Importing and Exporting Fixed Width Data 
---

Each column/field in fixed-width text data contains a certain number of character positions. Use a Greenplum Database custom format for fixed-width data by specifying the built-in formatter functions `fixedwith_in` (read) and `fixedwidth_out` (write). 

The following example creates an external table that specifies the `file` protocol and references a directory. When the external table is `SELECT`ed, Greenplum invokes the `fixedwidth_in` formatter function to format the data.

```
CREATE READABLE EXTERNAL TABLE students (
    name varchar(20), address varchar(30), age int)
LOCATION ('file://<host>/file/path/')
FORMAT 'CUSTOM' (formatter=fixedwidth_in, name='20', address='30', age='4');
```

The following options specify how to import fixed width data.

-   Read all the data.

    To load all of the fields on a line of fixed-width data, you must load them in their physical order. You must specify `<field_name>=<field_lenth>` for each field; you cannot specify a starting and ending position. The field names that you specify in the `FORMAT` options must match the order in which you define the columns in the `CREATE EXTERNAL TABLE` command.

-   Set options for blank and null characters.

    Trailing blanks are trimmed by default. To keep trailing blanks, use the `preserve_blanks=on` option. You can reset the trailing blanks option back to the default by specifying the `preserve_blanks=off` option.

    Use the `null='null_string_value'` option to specify a value for null characters.

-   If you specify `preserve_blanks=on`, you must also define a value for null characters.
-   If you specify `preserve_blanks=off`, null is not defined, and the field contains only blanks, Greenplum writes a null to the table. If null is defined, Greenplum writes an empty string to the table.

    Use the `line_delim='line_ending'` option to specify the line ending character. The following examples cover most cases. The `E` specifies an escape string constant.

    ```
    line_delim=E'\n'
    line_delim=E'\r'
    line_delim=E'\r\n'
    line_delim='abc'
    ```


**Parent topic:** [Using a Custom Format](../../load/topics/g-using-a-custom-format.html)

