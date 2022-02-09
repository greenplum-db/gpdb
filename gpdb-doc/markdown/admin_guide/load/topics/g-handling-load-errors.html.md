---
title: Handling Load Errors 
---

Readable external tables are most commonly used to select data to load into regular database tables. You use the `CREATE TABLE AS SELECT` or `INSERT INTO`commands to query the external table data. By default, if the data contains an error, the entire command fails and the data is not loaded into the target database table.

The `SEGMENT REJECT LIMIT` clause allows you to isolate format errors in external table data and to continue loading correctly formatted rows. Use `SEGMENT REJECT LIMIT`to set an error threshold, specifying the reject limit `count` as number of `ROWS` \(the default\) or as a `PERCENT` of total rows \(1-100\).

The entire external table operation is cancelled, and no rows are processed, if the number of error rows reaches the `SEGMENT REJECT LIMIT`. The limit of error rows is per-segment, not per entire operation. The operation processes all good rows, and it discards and optionally logs formatting errors for erroneous rows, if the number of error rows does not reach the `SEGMENT REJECT LIMIT`.

The `LOG ERRORS` clause allows you to keep error rows for further examination. For information about the `LOG ERRORS` clause, see the `CREATE EXTERNAL TABLE` command in the *Greenplum Database Reference Guide*.

When you set `SEGMENT REJECT LIMIT`, Greenplum scans the external data in single row error isolation mode. Single row error isolation mode applies to external data rows with format errors such as extra or missing attributes, attributes of a wrong data type, or invalid client encoding sequences. Greenplum does not check constraint errors, but you can filter constraint errors by limiting the `SELECT` from an external table at runtime. For example, to eliminate duplicate key errors:

```
=# INSERT INTO table_with_pkeys 
    SELECT DISTINCT * FROM external_table;

```

**Note:** When loading data with the `COPY` command or an external table, the value of the server configuration parameter `gp_initial_bad_row_limit` limits the initial number of rows that are processed that are not formatted properly. The default is to stop processing if the first 1000 rows contain formatting errors. See the *Greenplum Database Reference Guide* for information about the parameter.

-   **[Define an External Table with Single Row Error Isolation](../../load/topics/g-define-an-external-table-with-single-row-error-isolation.html)**  

-   **[Capture Row Formatting Errors and Declare a Reject Limit](../../load/topics/g-create-an-error-table-and-declare-a-reject-limit.html)**  

-   **[Viewing Bad Rows in the Error Log](../../load/topics/g-viewing-bad-rows-in-the-error-table-or-error-log.html)**  

-   **[Moving Data between Tables](../../load/topics/g-moving-data-between-tables.html)**  


**Parent topic:**[Loading and Unloading Data](../../load/topics/g-loading-and-unloading-data.html)

