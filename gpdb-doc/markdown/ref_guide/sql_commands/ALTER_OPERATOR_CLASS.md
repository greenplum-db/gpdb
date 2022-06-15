# ALTER OPERATOR CLASS 

Changes the definition of an operator class.

## Synopsis 

``` {#sql_command_synopsis}
ALTER OPERATOR CLASS <name> USING <index_method> RENAME TO <newname>

ALTER OPERATOR CLASS <name> USING <index_method> OWNER TO <newowner>
```

## Description 

`ALTER OPERATOR CLASS` changes the definition of an operator class.

You must own the operator class to use `ALTER OPERATOR CLASS`. To alter the owner, you must also be a direct or indirect member of the new owning role, and that role must have `CREATE` privilege on the operator class's schema. \(These restrictions enforce that altering the owner does not do anything you could not do by dropping and recreating the operator class. However, a superuser can alter ownership of any operator class anyway.\)

## Parameters 

name
:   The name \(optionally schema-qualified\) of an existing operator class.

index\_method
:   The name of the index method this operator class is for.

newname
:   The new name of the operator class.

newowner
:   The new owner of the operator class

## Compatibility 

There is no `ALTER OPERATOR CLASS` statement in the SQL standard.

## See Also 

[CREATE OPERATOR CLASS](CREATE_OPERATOR_CLASS.html), [DROP OPERATOR CLASS](DROP_OPERATOR_CLASS.html)

**Parent topic:** [SQL Command Reference](../sql_commands/sql_ref.html)

