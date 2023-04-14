---
title: Loading Data Using an External Table 
---

Use SQL commands such as `INSERT` and `SELECT` to query a readable external table, the same way that you query a regular database table. For example, to load travel expense data from an external table, `ext_expenses`, into a database table, `expenses_travel`:

```
=# INSERT INTO expenses_travel 
    SELECT * from ext_expenses where category='travel';

```

To load all data into a new database table:

```
=# CREATE TABLE expenses AS SELECT * from ext_expenses;

```

**Parent topic:** [Loading and Unloading Data](../../load/topics/g-loading-and-unloading-data.html)

