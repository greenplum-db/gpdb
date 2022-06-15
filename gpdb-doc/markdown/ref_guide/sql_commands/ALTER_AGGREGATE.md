# ALTER AGGREGATE 

Changes the definition of an aggregate function

## Synopsis 

``` {#sql_command_synopsis}
ALTER AGGREGATE <name> ( <type> [ , ... ] ) RENAME TO <new_name>

ALTER AGGREGATE <name> ( <type> [ , ... ] ) OWNER TO <new_owner>

ALTER AGGREGATE <name> ( <type> [ , ... ] ) SET SCHEMA <new_schema>
```

## Description 

`ALTER AGGREGATE` changes the definition of an aggregate function.

You must own the aggregate function to use `ALTER AGGREGATE`. To change the schema of an aggregate function, you must also have `CREATE` privilege on the new schema. To alter the owner, you must also be a direct or indirect member of the new owning role, and that role must have `CREATE` privilege on the aggregate function's schema. \(These restrictions enforce that altering the owner does not do anything you could not do by dropping and recreating the aggregate function. However, a superuser can alter ownership of any aggregate function anyway.\)

## Parameters 

name
:   The name \(optionally schema-qualified\) of an existing aggregate function.

type
:   An input data type on which the aggregate function operates. To reference a zero-argument aggregate function, write \* in place of the list of input data types.

new\_name
:   The new name of the aggregate function.

new\_owner
:   The new owner of the aggregate function.

new\_schema
:   The new schema for the aggregate function.

## Examples 

To rename the aggregate function `myavg` for type `integer` to `my_average`:

```
ALTER AGGREGATE myavg(integer) RENAME TO my_average;
```

To change the owner of the aggregate function `myavg` for type `integer` to `joe`:

```
ALTER AGGREGATE myavg(integer) OWNER TO joe;
```

To move the aggregate function `myavg` for type `integer` into schema `myschema`:

```
ALTER AGGREGATE myavg(integer) SET SCHEMA myschema;
```

## Compatibility 

There is no `ALTER AGGREGATE` statement in the SQL standard.

## See Also 

[CREATE AGGREGATE](CREATE_AGGREGATE.html), [DROP AGGREGATE](DROP_AGGREGATE.html)

**Parent topic:** [SQL Command Reference](../sql_commands/sql_ref.html)

