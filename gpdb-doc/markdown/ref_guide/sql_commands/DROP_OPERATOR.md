# DROP OPERATOR 

Removes an operator.

## Synopsis 

``` {#sql_command_synopsis}
DROP OPERATOR [IF EXISTS] <name> ( {<lefttype> | NONE} , 
    {<righttype> | NONE} ) [CASCADE | RESTRICT]
```

## Description 

`DROP OPERATOR` drops an existing operator from the database system. To execute this command you must be the owner of the operator.

## Parameters 

IF EXISTS
:   Do not throw an error if the operator does not exist. A notice is issued in this case.

name
:   The name \(optionally schema-qualified\) of an existing operator.

lefttype
:   The data type of the operator's left operand; write `NONE` if the operator has no left operand.

righttype
:   The data type of the operator's right operand; write `NONE` if the operator has no right operand.

CASCADE
:   Automatically drop objects that depend on the operator.

RESTRICT
:   Refuse to drop the operator if any objects depend on it. This is the default.

## Examples 

Remove the power operator `a^b` for type `integer`:

```
DROP OPERATOR ^ (integer, integer);
```

Remove the left unary bitwise complement operator `~b` for type `bit`:

```
DROP OPERATOR ~ (none, bit);
```

Remove the right unary factorial operator `x!` for type `bigint`:

```
DROP OPERATOR ! (bigint, none);
```

## Compatibility 

There is no `DROP OPERATOR` statement in the SQL standard.

## See Also 

[ALTER OPERATOR](ALTER_OPERATOR.html), [CREATE OPERATOR](CREATE_OPERATOR.html)

**Parent topic:** [SQL Command Reference](../sql_commands/sql_ref.html)

