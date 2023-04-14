# DROP OPERATOR 

Removes an operator.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
DROP OPERATOR [IF EXISTS] <name> ( {<left_type> | NONE} , 
    {<right_type> | NONE} ) [, ...] [CASCADE | RESTRICT]
```

## <a id="section3"></a>Description 

`DROP OPERATOR` drops an existing operator from the database system. To run this command you must be the owner of the operator.

## <a id="section4"></a>Parameters 

IF EXISTS
:   Do not throw an error if the operator does not exist. A notice is issued in this case.

name
:   The name \(optionally schema-qualified\) of an existing operator.

left\_type
:   The data type of the operator's left operand; write `NONE` if the operator has no left operand.

right\_type
:   The data type of the operator's right operand; write `NONE` if the operator has no right operand.

CASCADE
:   Automatically drop objects that depend on the operator \(such as views using it\), and in turn all objects that depend on those objects.

RESTRICT
:   Refuse to drop the operator if any objects depend on it. This is the default.

## <a id="section5"></a>Examples 

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

Remove multiple operators in one command:

```
DROP OPERATOR ~ (none, bit), ! (bigint, none);
```

## <a id="section6"></a>Compatibility 

There is no `DROP OPERATOR` statement in the SQL standard.

## <a id="section7"></a>See Also 

[ALTER OPERATOR](ALTER_OPERATOR.html), [CREATE OPERATOR](CREATE_OPERATOR.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

