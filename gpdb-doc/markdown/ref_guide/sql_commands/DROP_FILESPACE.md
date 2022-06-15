# DROP FILESPACE 

Removes a filespace.

## Synopsis 

``` {#sql_command_synopsis}
DROP FILESPACE [IF EXISTS] <filespacename>
```

## Description 

`DROP FILESPACE` removes a filespace definition and its system-generated data directories from the system.

A filespace can only be dropped by its owner or a superuser. The filespace must be empty of all tablespace objects before it can be dropped. It is possible that tablespaces in other databases may still be using a filespace even if no tablespaces in the current database are using the filespace.

## Parameters 

IF EXISTS
:   Do not throw an error if the filespace does not exist. A notice is issued in this case.

tablespacename
:   The name of the filespace to remove.

## Examples 

Remove the tablespace `myfs`:

```
DROP FILESPACE myfs;
```

## Compatibility 

There is no `DROP FILESPACE` statement in the SQL standard or in PostgreSQL.

## See Also 

[ALTER FILESPACE](ALTER_FILESPACE.html), [DROP TABLESPACE](DROP_TABLESPACE.html), `gpfilespace` in the *Greenplum Database Utility Guide*

**Parent topic:** [SQL Command Reference](../sql_commands/sql_ref.html)

