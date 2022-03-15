---
title: Capture Row Formatting Errors and Declare a Reject Limit 
---

The following SQL fragment captures formatting errors internally in Greenplum Database and declares a reject limit of 10 rows.

```
LOG ERRORS SEGMENT REJECT LIMIT 10 ROWS
```

Use the built-in SQL function `gp_read_error_log()` to read the error log data. For information about viewing log errors, see [Viewing Bad Rows in the Error Log](g-viewing-bad-rows-in-the-error-table-or-error-log.html).

**Parent topic:**[Handling Load Errors](../../load/topics/g-handling-load-errors.html)

