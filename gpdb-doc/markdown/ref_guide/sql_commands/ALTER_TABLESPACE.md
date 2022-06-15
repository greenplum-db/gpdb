# ALTER TABLESPACE 

Changes the definition of a tablespace.

## Synopsis 

``` {#sql_command_synopsis}
ALTER TABLESPACE <name> RENAME TO <newname>

ALTER TABLESPACE <name> OWNER TO <newowner>
```

## Description 

`ALTER TABLESPACE` changes the definition of a tablespace.

You must own the tablespace to use `ALTER TABLESPACE`. To alter the owner, you must also be a direct or indirect member of the new owning role. \(Note that superusers have these privileges automatically.\)

## Parameters 

name
:   The name of an existing tablespace.

newname
:   The new name of the tablespace. The new name cannot begin with pg\_ or gp\_ \(reserved for system tablespaces\).

newowner
:   The new owner of the tablespace.

## Examples 

Rename tablespace `index_space` to `fast_raid`:

```
ALTER TABLESPACE index_space RENAME TO fast_raid;
```

Change the owner of tablespace `index_space`:

```
ALTER TABLESPACE index_space OWNER TO mary;
```

## Compatibility 

There is no `ALTER TABLESPACE` statement in the SQL standard.

## See Also 

[CREATE TABLESPACE](CREATE_TABLESPACE.html), [DROP TABLESPACE](DROP_TABLESPACE.html)

**Parent topic:** [SQL Command Reference](../sql_commands/sql_ref.html)

