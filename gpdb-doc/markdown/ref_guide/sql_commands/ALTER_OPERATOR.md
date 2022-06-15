# ALTER OPERATOR 

Changes the definition of an operator.

## Synopsis 

``` {#sql_command_synopsis}
ALTER OPERATOR <name> ( {<lefttype> | NONE} , {<righttype> | NONE} ) 
   OWNER TO <newowner>
```

## Description 

`ALTER OPERATOR` changes the definition of an operator. The only currently available functionality is to change the owner of the operator.

You must own the operator to use `ALTER OPERATOR`. To alter the owner, you must also be a direct or indirect member of the new owning role, and that role must have `CREATE` privilege on the operator's schema. \(These restrictions enforce that altering the owner does not do anything you could not do by dropping and recreating the operator. However, a superuser can alter ownership of any operator anyway.\)

## Parameters 

name
:   The name \(optionally schema-qualified\) of an existing operator.

lefttype
:   The data type of the operator's left operand; write `NONE` if the operator has no left operand.

righttype
:   The data type of the operator's right operand; write `NONE` if the operator has no right operand.

newowner
:   The new owner of the operator.

## Examples 

Change the owner of a custom operator `a @@ b` for type `text`:

```
ALTER OPERATOR @@ (text, text) OWNER TO joe;
```

## Compatibility 

There is no `ALTER``OPERATOR` statement in the SQL standard.

## See Also 

[CREATE OPERATOR](CREATE_OPERATOR.html), [DROP OPERATOR](DROP_OPERATOR.html)

**Parent topic:** [SQL Command Reference](../sql_commands/sql_ref.html)

