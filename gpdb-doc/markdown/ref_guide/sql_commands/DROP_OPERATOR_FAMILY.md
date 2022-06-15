# DROP OPERATOR FAMILY 

Removes an operator family.

## Synopsis 

``` {#sql_command_synopsis}
DROP OPERATOR FAMILY [IF EXISTS] <name> USING <index_method> [CASCADE | RESTRICT]
```

## Description 

`DROP OPERATOR FAMILY` drops an existing operator family. To execute this command you must be the owner of the operator family.

`DROP OPERATOR FAMILY` includes dropping any operator classes contained in the family, but it does not drop any of the operators or functions referenced by the family. If there are any indexes depending on operator classes within the family, you will need to specify `CASCADE` for the drop to complete.

## Parameters 

IF EXISTS
:   Do not throw an error if the operator family does not exist. A notice is issued in this case.

name
:   The name \(optionally schema-qualified\) of an existing operator family.

index\_method
:   The name of the index access method the operator family is for.

CASCADE
:   Automatically drop objects that depend on the operator family.

RESTRICT
:   Refuse to drop the operator family if any objects depend on it. This is the default.

## Examples 

Remove the B-tree operator family `float_ops`:

```
DROP OPERATOR FAMILY float_ops USING btree;
```

This command will not succeed if there are any existing indexes that use the operator family. Add `CASCADE` to drop such indexes along with the operator family.

## Compatibility 

There is no `DROP OPERATOR FAMILY` statement in the SQL standard.

## See Also 

[ALTER OPERATOR FAMILY](ALTER_OPERATOR_FAMILY.html), [CREATE OPERATOR FAMILY](CREATE_OPERATOR_FAMILY.html), [ALTER OPERATOR CLASS](ALTER_OPERATOR_CLASS.html), [CREATE OPERATOR CLASS](CREATE_OPERATOR_CLASS.md#bx20941), [DROP OPERATOR CLASS](DROP_OPERATOR_CLASS.html)

**Parent topic:** [SQL Command Reference](../sql_commands/sql_ref.html)

