# ALTER TYPE 

Changes the definition of a data type.

## Synopsis 

``` {#sql_command_synopsis}
ALTER TYPE <name>
   OWNER TO <new_owner> | SET SCHEMA <new_schema>
```

## Description 

`ALTER TYPE` changes the definition of an existing type. You can change the owner and the schema of a type.

You must own the type to use `ALTER TYPE`. To change the schema of a type, you must also have `CREATE` privilege on the new schema. To alter the owner, you must also be a direct or indirect member of the new owning role, and that role must have `CREATE` privilege on the type's schema. \(These restrictions enforce that altering the owner does not do anything that could be done by dropping and recreating the type. However, a superuser can alter ownership of any type.\)

## Parameters 

name
:   The name \(optionally schema-qualified\) of an existing type to alter.

new\_owner
:   The user name of the new owner of the type.

new\_schema
:   The new schema for the type.

## Examples 

To change the owner of the user-defined type `email` to `joe`:

```
ALTER TYPE email OWNER TO joe;
```

To change the schema of the user-defined type `email` to `customers`:

```
ALTER TYPE email SET SCHEMA customers;
```

## Compatibility 

There is no `ALTER TYPE` statement in the SQL standard.

## See Also 

[CREATE TYPE](CREATE_TYPE.html), [DROP TYPE](DROP_TYPE.html)

**Parent topic:** [SQL Command Reference](../sql_commands/sql_ref.html)

