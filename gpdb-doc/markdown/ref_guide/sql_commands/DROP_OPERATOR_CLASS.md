# DROP OPERATOR CLASS 

Removes an operator class.

## Synopsis 

``` {#sql_command_synopsis}
DROP OPERATOR CLASS [IF EXISTS] <name> USING <index_method> [CASCADE | RESTRICT]
```

## Description 

`DROP OPERATOR` drops an existing operator class. To execute this command you must be the owner of the operator class.

## Parameters 

IF EXISTS
:   Do not throw an error if the operator class does not exist. A notice is issued in this case.

name
:   The name \(optionally schema-qualified\) of an existing operator class.

index\_method
:   The name of the index access method the operator class is for.

CASCADE
:   Automatically drop objects that depend on the operator class.

RESTRICT
:   Refuse to drop the operator class if any objects depend on it. This is the default.

## Examples 

Remove the B-tree operator class `widget_ops`:

```
DROP OPERATOR CLASS widget_ops USING btree;
```

This command will not succeed if there are any existing indexes that use the operator class. Add `CASCADE` to drop such indexes along with the operator class.

## Compatibility 

There is no `DROP OPERATOR CLASS` statement in the SQL standard.

## See Also 

[ALTER OPERATOR CLASS](ALTER_OPERATOR_CLASS.html), [CREATE OPERATOR CLASS](CREATE_OPERATOR_CLASS.html)

**Parent topic:** [SQL Command Reference](../sql_commands/sql_ref.html)

