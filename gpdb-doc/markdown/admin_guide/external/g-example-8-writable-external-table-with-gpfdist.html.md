---
title: Example 8—Writable External Table with gpfdist 
---

Creates a writable external table, *sales\_out,* that uses gpfdist to write output data to the file *sales.out*. The column delimiter is a pipe \( \| \) and NULL \(' '\) is a space. The file will be created in the directory specified when you started the gpfdist file server.

```
=# CREATE WRITABLE EXTERNAL TABLE sales_out (LIKE sales) 
   LOCATION ('gpfdist://etl1:8081/sales.out')
   FORMAT 'TEXT' ( DELIMITER '|' NULL ' ')
   DISTRIBUTED BY (txn_id);
```

**Parent topic:** [Examples for Creating External Tables](../external/g-creating-external-tables---examples.html)

