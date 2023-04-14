# ALTER SCHEMA 

Changes the definition of a schema.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
ALTER SCHEMA <name> RENAME TO <new_name>

ALTER SCHEMA <name> OWNER TO { <new_owner> | CURRENT_USER | SESSION_USER }
```

## <a id="section3"></a>Description 

`ALTER SCHEMA` changes the definition of a schema.

You must own the schema to use `ALTER SCHEMA`. To rename a schema you must also have the `CREATE` privilege for the database. To alter the owner, you must also be a direct or indirect member of the new owning role, and you must have the `CREATE` privilege for the database. Note that superusers have all these privileges automatically.

## <a id="section4"></a>Parameters 

name
:   The name of an existing schema.

new\_name
:   The new name of the schema. The new name cannot begin with `pg_`, as such names are reserved for system schemas.

new\_owner
:   The new owner of the schema.

## <a id="section5"></a>Compatibility 

There is no `ALTER SCHEMA` statement in the SQL standard.

## <a id="section6"></a>See Also 

[CREATE SCHEMA](CREATE_SCHEMA.html), [DROP SCHEMA](DROP_SCHEMA.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

