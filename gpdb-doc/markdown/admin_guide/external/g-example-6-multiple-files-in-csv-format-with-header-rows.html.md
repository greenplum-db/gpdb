---
title: Example 6—Multiple files in CSV format with header rows 
---

Creates a readable external table, *ext\_expenses,* using the `file` protocol. The files are `CSV` format and have a header row.

```
=# CREATE EXTERNAL TABLE ext_expenses ( name text, 
   date date,  amount float4, category text, desc1 text ) 
   LOCATION ('file://filehost/data/international/*', 
             'file://filehost/data/regional/*',
             'file://filehost/data/supplement/*.csv')
   FORMAT 'CSV' (HEADER);

```

**Parent topic:** [Examples for Creating External Tables](../external/g-creating-external-tables---examples.html)

