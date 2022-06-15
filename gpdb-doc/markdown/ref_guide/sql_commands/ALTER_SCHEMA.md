# ALTER SCHEMA 

Changes the definition of a schema.

## Synopsis 

``` {#sql_command_synopsis}
ALTER SCHEMA <name> RENAME TO <newname>

ALTER SCHEMA <name> OWNER TO <newowner>
```

## Description 

`ALTER SCHEMA` changes the definition of a schema.

You must own the schema to use `ALTER SCHEMA`. To rename a schema you must also have the `CREATE` privilege for the database. To alter the owner, you must also be a direct or indirect member of the new owning role, and you must have the `CREATE` privilege for the database. Note that superusers have all these privileges automatically.

## Parameters 

name
:   The name of an existing schema.

newname
:   The new name of the schema. The new name cannot begin with `pg_`, as such names are reserved for system schemas.

newowner
:   The new owner of the schema.

## Compatibility 

There is no `ALTER SCHEMA` statement in the SQL standard.

## See Also 

[CREATE SCHEMA](CREATE_SCHEMA.html), [DROP SCHEMA](DROP_SCHEMA.html)

**Parent topic:** [SQL Command Reference](../sql_commands/sql_ref.html)

