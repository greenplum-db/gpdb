# DROP TABLESPACE 

Removes a tablespace.

## Synopsis 

``` {#sql_command_synopsis}
DROP TABLESPACE [IF EXISTS] <tablespacename>
```

## Description 

`DROP TABLESPACE` removes a tablespace from the system.

A tablespace can only be dropped by its owner or a superuser. The tablespace must be empty of all database objects before it can be dropped. It is possible that objects in other databases may still reside in the tablespace even if no objects in the current database are using the tablespace.

## Parameters 

IF EXISTS
:   Do not throw an error if the tablespace does not exist. A notice is issued in this case.

tablespacename
:   The name of the tablespace to remove.

## Examples 

Remove the tablespace `mystuff`:

```
DROP TABLESPACE mystuff;
```

## Compatibility 

`DROP TABLESPACE` is a Greenplum Database extension.

## See Also 

[CREATE TABLESPACE](CREATE_TABLESPACE.html), [ALTER TABLESPACE](ALTER_TABLESPACE.html)

**Parent topic:** [SQL Command Reference](../sql_commands/sql_ref.html)

