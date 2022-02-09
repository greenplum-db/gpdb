---
title: Moving Data between Tables 
---

You can use `CREATE TABLE AS` or `INSERT...SELECT` to load external and external web table data into another \(non-external\) database table, and the data will be loaded in parallel according to the external or external web table definition.

If an external table file or external web table data source has an error, one of the following will happen, depending on the isolation mode used:

-   **Tables without error isolation mode**: any operation that reads from that table fails. Loading from external and external web tables without error isolation mode is an all or nothing operation.
-   **Tables with error isolation mode**: the entire file will be loaded, except for the problematic rows \(subject to the configured REJECT\_LIMIT\)

**Parent topic:**[Handling Load Errors](../../load/topics/g-handling-load-errors.html)

