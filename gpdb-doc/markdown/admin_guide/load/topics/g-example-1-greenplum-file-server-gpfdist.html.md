---
title: Example 1—Greenplum file server (gpfdist) 
---

```
=# CREATE WRITABLE EXTERNAL TABLE unload_expenses 
   ( LIKE expenses ) 
   LOCATION ('gpfdist://etlhost-1:8081/expenses1.out', 
             'gpfdist://etlhost-2:8081/expenses2.out')
 FORMAT 'TEXT' (DELIMITER ',')
 DISTRIBUTED BY (exp_id);

```

**Parent topic:** [Defining a File-Based Writable External Table](../../load/topics/g-defining-a-file-based-writable-external-table.html)

