# CREATE OPERATOR FAMILY 

Defines a new operator family.

## Synopsis 

``` {#sql_command_synopsis}
CREATE OPERATOR FAMILY <name>  USING <index_method>  
```

## Description 

`CREATE OPERATOR FAMILY` creates a new operator family. An operator family defines a collection of related operator classes, and perhaps some additional operators and support functions that are compatible with these operator classes but not essential for the functioning of any individual index. \(Operators and functions that are essential to indexes should be grouped within the relevant operator class, rather than being "loose" in the operator family. Typically, single-data-type operators are bound to operator classes, while cross-data-type operators can be loose in an operator family containing operator classes for both data types.\)

The new operator family is initially empty. It should be populated by issuing subsequent `CREATE OPERATOR CLASS` commands to add contained operator classes, and optionally `ALTER OPERATOR FAMILY`commands to add "loose" operators and their corresponding support functions.

If a schema name is given then the operator family is created in the specified schema. Otherwise it is created in the current schema. Two operator families in the same schema can have the same name only if they are for different index methods.

The user who defines an operator family becomes its owner. Presently, the creating user must be a superuser. \(This restriction is made because an erroneous operator family definition could confuse or even crash the server.\)

## Parameters 

name
:   The \(optionally schema-qualified\) name of the operator family to be defined. The name can be schema-qualified.

index\_method
:   The name of the index method this operator family is for.

## Compatibility 

`CREATE OPERATOR FAMILY` is a Greenplum Database extension. There is no `CREATE OPERATOR FAMILY` statement in the SQL standard.

## See Also 

[ALTER OPERATOR FAMILY](ALTER_OPERATOR_FAMILY.html), [DROP OPERATOR FAMILY](DROP_OPERATOR_FAMILY.html), [CREATE FUNCTION](CREATE_FUNCTION.html), [ALTER OPERATOR CLASS](ALTER_OPERATOR_CLASS.html), [CREATE OPERATOR CLASS](CREATE_OPERATOR_CLASS.html), [DROP OPERATOR CLASS](DROP_OPERATOR_CLASS.html)

**Parent topic:** [SQL Command Reference](../sql_commands/sql_ref.html)

