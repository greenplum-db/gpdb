---
title: Optimizing Data Load and Query Performance 
---

Use the following tips to help optimize your data load and subsequent query performance.

-   Drop indexes before loading data into existing tables.

    Creating an index on pre-existing data is faster than updating it incrementally as each row is loaded. You can temporarily increase the `maintenance_work_mem` server configuration parameter to help speed up `CREATE INDEX` commands, though load performance is affected. Drop and recreate indexes only when there are no active users on the system.

-   Create indexes last when loading data into new tables. Create the table, load the data, and create any required indexes.
-   Run `ANALYZE` after loading data. If you significantly altered the data in a table, run `ANALYZE` or `VACUUM ANALYZE` to update table statistics for the query optimizer. Current statistics ensure that the optimizer makes the best decisions during query planning and avoids poor performance due to inaccurate or nonexistent statistics.
-   Run `VACUUM` after load errors. If the load operation does not run in single row error isolation mode, the operation stops at the first error. The target table contains the rows loaded before the error occurred. You cannot access these rows, but they occupy disk space. Use the `VACUUM` command to recover the wasted space.

**Parent topic:**[Loading and Unloading Data](../../load/topics/g-loading-and-unloading-data.html)

