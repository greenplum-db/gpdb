---
title: Example 4—Single gpfdist instance with error logging 
---

Uses the gpfdist protocol to create a readable external table, `ext_expenses,` from all files with the *txt* extension. The column delimiter is a pipe \( \| \) and NULL \(' '\) is a space.

Access to the external table is single row error isolation mode. Input data formatting errors are captured internally in Greenplum Database with a description of the error. See [Viewing Bad Rows in the Error Log](../load/topics/g-viewing-bad-rows-in-the-error-table-or-error-log.html) for information about investigating error rows. You can view the errors, fix the issues, and then reload the rejected data. If the error count on a segment is greater than five \(the `SEGMENT REJECT LIMIT` value\), the entire external table operation fails and no rows are processed.

```
=# CREATE EXTERNAL TABLE ext_expenses ( name text, 
   date date, amount float4, category text, desc1 text ) 
   LOCATION ('gpfdist://etlhost-1:8081/*.txt', 
             'gpfdist://etlhost-2:8082/*.txt')
   FORMAT 'TEXT' ( DELIMITER '|' NULL ' ')
   LOG ERRORS SEGMENT REJECT LIMIT 5;

```

To create the readable ext\_expenses table from CSV-formatted text files:

```
=# CREATE EXTERNAL TABLE ext_expenses ( name text, 
   date date,  amount float4, category text, desc1 text ) 
   LOCATION ('gpfdist://etlhost-1:8081/*.txt', 
             'gpfdist://etlhost-2:8082/*.txt')
   FORMAT 'CSV' ( DELIMITER ',' )
   LOG ERRORS SEGMENT REJECT LIMIT 5;

```



**Parent topic:**[Examples for Creating External Tables](../external/g-creating-external-tables---examples.html)

