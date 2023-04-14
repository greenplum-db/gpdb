---
title: Example 1—Single gpfdist instance on single-NIC machine 
---

Creates a readable external table, `ext_expenses`, using the `gpfdist` protocol. The files are formatted with a pipe \(\|\) as the column delimiter.

```
=# CREATE EXTERNAL TABLE ext_expenses ( name text,
    date date, amount float4, category text, desc1 text )
    LOCATION ('gpfdist://etlhost-1:8081/*')
FORMAT 'TEXT' (DELIMITER '|');
```

**Parent topic:** [Examples for Creating External Tables](../external/g-creating-external-tables---examples.html)

