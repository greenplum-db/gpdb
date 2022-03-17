---
title: Importing and Exporting Fixed Width Data 
---

Specify custom formats for fixed-width data with the Greenplum Database functions `fixedwith_in` and `fixedwidth_out`. These functions already exist in the file $GPHOME/share/postgresql/cdb\_external\_extensions.sql. The following example declares a custom format, then calls the `fixedwidth_in` function to format the data.

```
CREATE READABLE EXTERNAL TABLE students (
name varchar(20), address varchar(30), age int)
LOCATION ('file://<host>/file/path/')
FORMAT 'CUSTOM' (formatter=fixedwidth_in, 
         name='20', address='30', age='4');

```

The following options specify how to import fixed width data.

-   Read all the data.

    To load all the fields on a line of fixed with data, you must load them in their physical order. You must specify the field length, but cannot specify a starting and ending position. The fields names in the fixed width arguments must match the order in the field list at the beginning of the `CREATE TABLE` command.

-   Set options for blank and null characters.

    Trailing blanks are trimmed by default. To keep trailing blanks, use the `preserve_blanks=on` option. You can reset the trailing blanks option to the default with the `preserve_blanks=off` option.

    Use the null=`'null_string_value'` option to specify a value for null characters.

-   If you specify `preserve_blanks=on`, you must also define a value for null characters.
-   If you specify `preserve_blanks=off`, null is not defined, and the field contains only blanks, Greenplum writes a null to the table. If null is defined, Greenplum writes an empty string to the table.

    Use the `line_delim='line_ending'` parameter to specify the line ending character. The following examples cover most cases. The `E` specifies an escape string constant.

    ```
    line_delim=E'\n'
    line_delim=E'\r'
    line_delim=E'\r\n'
    line_delim='abc'
    ```


**Parent topic:**[Using a Custom Format](../../load/topics/g-using-a-custom-format.html)

