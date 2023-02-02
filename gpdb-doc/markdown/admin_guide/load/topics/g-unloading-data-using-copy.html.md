---
title: Unloading Data Using COPY 
---

`COPY TO` copies data from a table to a file \(or standard input\) on the Greenplum coordinator host using a single process on the Greenplum coordinator instance. Use `COPY` to output a table's entire contents, or filter the output using a `SELECT` statement. For example:

```
COPY (SELECT * FROM country WHERE country_name LIKE 'A%') 
TO '/home/gpadmin/a_list_countries.out';

```

**Parent topic:** [Unloading Data from Greenplum Database](../../load/topics/g-unloading-data-from-greenplum-database.html)

