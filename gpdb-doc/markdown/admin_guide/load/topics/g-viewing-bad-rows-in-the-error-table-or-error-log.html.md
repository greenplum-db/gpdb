---
title: Viewing Bad Rows in the Error Log 
---

If you use single row error isolation \(see [Define an External Table with Single Row Error Isolation](g-define-an-external-table-with-single-row-error-isolation.html) or [Running COPY in Single Row Error Isolation Mode](g-running-copy-in-single-row-error-isolation-mode.html)\), any rows with formatting errors are logged internally by Greenplum Database.

Greenplum Database captures the following error information in a table format:

|column|type|description|
|------|----|-----------|
|cmdtime|timestamptz|Timestamp when the error occurred.|
|relname|text|The name of the external table or the target table of a `COPY` command.|
|filename|text|The name of the load file that contains the error.|
|linenum|int|If `COPY` was used, the line number in the load file where the error occurred. For external tables using file:// protocol or gpfdist:// protocol and CSV format, the file name and line number is logged.|
|bytenum|int|For external tables with the gpfdist:// protocol and data in TEXT format: the byte offset in the load file where the error occurred. gpfdist parses TEXT files in blocks, so logging a line number is not possible. CSV files are parsed a line at a time so line number tracking is possible for CSV files.|
|errmsg|text|The error message text.|
|rawdata|text|The raw data of the rejected row.|
|rawbytes|bytea|In cases where there is a database encoding error \(the client encoding used cannot be converted to a server-side encoding\), it is not possible to log the encoding error as *rawdata*. Instead the raw bytes are stored and you will see the octal code for any non seven bit ASCII characters.|

You can use the Greenplum Database built-in SQL function `gp_read_error_log()` to display formatting errors that are logged internally. For example, this command displays the error log information for the table *ext\_expenses*:

```
SELECT gp_read_error_log('ext_expenses');
```

For information about managing formatting errors that are logged internally, see the command `COPY` or `CREATE EXTERNAL TABLE` in the *Greenplum Database Reference Guide*.

**Parent topic:** [Handling Load Errors](../../load/topics/g-handling-load-errors.html)

