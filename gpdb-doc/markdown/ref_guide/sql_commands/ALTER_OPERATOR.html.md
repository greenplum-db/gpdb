---
title: ALTER OPERATOR 
---

Changes the definition of an operator.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
ALTER OPERATOR <name> ( {<left_type> | NONE} , {<right_type> | NONE} ) 
   OWNER TO <new_owner>

ALTER OPERATOR <name> ( {<left_type> | NONE} , {<right_type> | NONE} ) 
    SET SCHEMA <new_schema>

```

## <a id="section3"></a>Description 

`ALTER OPERATOR` changes the definition of an operator. The only currently available functionality is to change the owner of the operator.

You must own the operator to use `ALTER OPERATOR`. To alter the owner, you must also be a direct or indirect member of the new owning role, and that role must have `CREATE` privilege on the operator's schema. \(These restrictions enforce that altering the owner does not do anything you could not do by dropping and recreating the operator. However, a superuser can alter ownership of any operator anyway.\)

## <a id="section4"></a>Parameters 

name
:   The name \(optionally schema-qualified\) of an existing operator.

left\_type
:   The data type of the operator's left operand; write `NONE` if the operator has no left operand.

right\_type
:   The data type of the operator's right operand; write `NONE` if the operator has no right operand.

new\_owner
:   The new owner of the operator.

new\_schema
:   The new schema for the operator.

## <a id="section5"></a>Examples 

Change the owner of a custom operator `a @@ b` for type `text`:

```
ALTER OPERATOR @@ (text, text) OWNER TO joe;
```

## <a id="section6"></a>Compatibility 

There is no `ALTER``OPERATOR` statement in the SQL standard.

## <a id="section7"></a>See Also 

[CREATE OPERATOR](CREATE_OPERATOR.html), [DROP OPERATOR](DROP_OPERATOR.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

