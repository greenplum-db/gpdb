---
title: Example 2—Multiple gpfdist instances 
---

Creates a readable external table, *ext\_expenses,* using the gpfdist protocol from all files with the *txt* extension. The column delimiter is a pipe \( \| \) and NULL \(' '\) is a space.

```
=# CREATE EXTERNAL TABLE ext_expenses ( name text, 
   date date,  amount float4, category text, desc1 text ) 
   LOCATION ('gpfdist://etlhost-1:8081/*.txt', 
             'gpfdist://etlhost-2:8081/*.txt')
   FORMAT 'TEXT' ( DELIMITER '|' NULL ' ') ;

```

**Parent topic:** [Examples for Creating External Tables](../external/g-creating-external-tables---examples.html)

