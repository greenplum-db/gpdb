---
title: DROP TABLESPACE 
---

Removes a tablespace.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
DROP TABLESPACE [IF EXISTS] <tablespace_name>
```

## <a id="section3"></a>Description 

`DROP TABLESPACE` removes a tablespace from the system.

A tablespace can only be dropped by its owner or a superuser. The tablespace must be empty of all database objects before it can be dropped. It is possible that objects in other databases may still reside in the tablespace even if no objects in the current database are using the tablespace. Also, if the tablespace is listed in the [temp\_tablespaces](../config_params/guc-list.html) setting of any active session, `DROP TABLESPACE` might fail due to temporary files residing in the tablespace.

## <a id="section4"></a>Parameters 

IF EXISTS
:   Do not throw an error if the tablespace does not exist. A notice is issued in this case.

tablespace\_name
:   The name of the tablespace to remove.

## <a id="notes"></a>Notes 

Run `DROP TABLESPACE` during a period of low activity to avoid issues due to concurrent creation of tables and temporary objects. When a tablespace is dropped, there is a small window in which a table could be created in the tablespace that is currently being dropped. If this occurs, Greenplum Database returns a warning. This is an example of the `DROP TABLESPACE` warning.

```
testdb=# DROP TABLESPACE mytest; 
WARNING:  tablespace with oid "16415" is not empty  (seg1 192.168.8.145:25433 pid=29023)
WARNING:  tablespace with oid "16415" is not empty  (seg0 192.168.8.145:25432 pid=29022)
WARNING:  tablespace with oid "16415" is not empty
DROP TABLESPACE
```

The table data in the tablespace directory is not dropped. You can use the [ALTER TABLE](ALTER_TABLE.html) command to change the tablespace defined for the table and move the data to an existing tablespace.

## <a id="section5"></a>Examples 

Remove the tablespace `mystuff`:

```
DROP TABLESPACE mystuff;
```

## <a id="section6"></a>Compatibility 

`DROP TABLESPACE` is a Greenplum Database extension.

## <a id="section7"></a>See Also 

[CREATE TABLESPACE](CREATE_TABLESPACE.html), [ALTER TABLESPACE](ALTER_TABLESPACE.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

