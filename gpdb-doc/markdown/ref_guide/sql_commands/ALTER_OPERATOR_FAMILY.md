# ALTER OPERATOR FAMILY 

Changes the definition of an operator family.

## Synopsis 

``` {#sql_command_synopsis}
ALTER OPERATOR FAMILY <name> USING <index_method> ADD
  {  OPERATOR <strategy_number> <operator_name> ( <op_type>, <op_type> ) [ RECHECK ]
    | FUNCTION <support_number> [ ( <op_type> [ , <op_type> ] ) ] <funcname> ( <argument_type> [, ...] )
  } [, ... ]
ALTER OPERATOR FAMILY <name> USING <index_method> DROP
  {  OPERATOR s<trategy_number> ( <op_type>, <op_type> ) 
    | FUNCTION <support_number> [ ( <op_type> [ , <op_type> ] ) 
  } [, ... ]

ALTER OPERATOR FAMILY <name> USING <index_method> RENAME TO <newname>

ALTER OPERATOR FAMILY <name> USING <index_method> OWNER TO <newowner>
```

## Description 

`ALTER OPERATOR FAMILY` changes the definition of an operator family. You can add operators and support functions to the family, remove them from the family, or change the family's name or owner.

When operators and support functions are added to a family with `ALTER OPERATOR FAMILY`, they are not part of any specific operator class within the family, but are just "loose" within the family. This indicates that these operators and functions are compatible with the family's semantics, but are not required for correct functioning of any specific index. \(Operators and functions that are so required should be declared as part of an operator class, instead; see [CREATE OPERATOR CLASS](CREATE_OPERATOR_CLASS.html).\) You can drop loose members of a family from the family at any time, but members of an operator class cannot be dropped without dropping the whole class and any indexes that depend on it. Typically, single-data-type operators and functions are part of operator classes because they are needed to support an index on that specific data type, while cross-data-type operators and functions are made loose members of the family.

You must be a superuser to use `ALTER OPERATOR FAMILY`. \(This restriction is made because an erroneous operator family definition could confuse or even crash the server.\)

`ALTER OPERATOR FAMILY` does not presently check whether the operator family definition includes all the operators and functions required by the index method, nor whether the operators and functions form a self-consistent set. It is the user's responsibility to define a valid operator family.

`OPERATOR` and `FUNCTION` clauses can appear in any order.

## Parameters 

name
:   The name \(optionally schema-qualified\) of an existing operator family.

index\_method
:   The name of the index method this operator family is for.

strategy\_number
:   The index method's strategy number for an operator associated with the operator family.

operator\_name
:   The name \(optionally schema-qualified\) of an operator associated with the operator family.

op\_type
:   In an `OPERATOR` clause, the operand data type\(s\) of the operator, or `NONE` to signify a left-unary or right-unary operator. Unlike the comparable syntax in `CREATE OPERATOR CLASS`, the operand data types must always be specified.In an `ADD FUNCTION` clause, the operand data type\(s\) the function is intended to support, if different from the input data type\(s\) of the function. For B-tree and hash indexes it is not necessary to specify op\_type since the function's input data type\(s\) are always the correct ones to use. For `GiST` indexes it is necessary to specify the input data type the function is to be used with. In a `DROP FUNCTION` clause, the operand data type\(s\) the function is intended to support must be specified.

RECHECK
:   If present, the index is "lossy" for this operator, and so the rows retrieved using the index must be rechecked to verify that they actually satisfy the qualification clause involving this operator.

support\_number
:   The index method's support procedure number for a function associated with the operator family.

funcname
:   The name \(optionally schema-qualified\) of a function that is an index method support procedure for the operator family.

argument\_types
:   The parameter data type\(s\) of the function.

newname
:   The new name of the operator family.

newowner
:   The new owner of the operator family.

## Compatibility 

There is no `ALTER OPERATOR FAMILY` statement in the SQL standard.

## Notes 

Notice that the `DROP` syntax only specifies the "slot" in the operator family, by strategy or support number and input data type\(s\). The name of the operator or function occupying the slot is not mentioned. Also, for `DROP FUNCTION` the type\(s\) to specify are the input data type\(s\) the function is intended to support; for `GiST` indexes this might have nothing to do with the actual input argument types of the function.

Because the index machinery does not check access permissions on functions before using them, including a function or operator in an operator family is tantamount to granting public execute permission on it. This is usually not an issue for the sorts of functions that are useful in an operator family.

The operators should not be defined by SQL functions. A SQL function is likely to be inlined into the calling query, which will prevent the optimizer from recognizing that the query matches an index.

## Examples 

The following example command adds cross-data-type operators and support functions to an operator family that already contains B-tree operator classes for data types int4 and int2.:

```
ALTER OPERATOR FAMILY integer_ops USING btree ADD

  -- int4 vs int2
  OPERATOR 1 < (int4, int2) ,
  OPERATOR 2 <= (int4, int2) ,
  OPERATOR 3 = (int4, int2) ,
  OPERATOR 4 >= (int4, int2) ,
  OPERATOR 5 > (int4, int2) ,
  FUNCTION 1 btint42cmp(int4, int2) ,

  -- int2 vs int4
  OPERATOR 1 < (int2, int4) ,
  OPERATOR 2 <= (int2, int4) ,
  OPERATOR 3 = (int2, int4) ,
  OPERATOR 4 >= (int2, int4) ,
  OPERATOR 5 > (int2, int4) ,
  FUNCTION 1 btint24cmp(int2, int4) ;
```

To remove these entries:

```
ALTER OPERATOR FAMILY integer_ops USING btree DROP

  -- int4 vs int2
  OPERATOR 1 (int4, int2) ,
  OPERATOR 2 (int4, int2) ,
  OPERATOR 3 (int4, int2) ,
  OPERATOR 4 (int4, int2) ,
  OPERATOR 5 (int4, int2) ,
  FUNCTION 1 (int4, int2) ,

  -- int2 vs int4
  OPERATOR 1 (int2, int4) ,
  OPERATOR 2 (int2, int4) ,
  OPERATOR 3 (int2, int4) ,
  OPERATOR 4 (int2, int4) ,
  OPERATOR 5 (int2, int4) ,
  FUNCTION 1 (int2, int4) ;
```

## See Also 

[CREATE OPERATOR FAMILY](CREATE_OPERATOR_FAMILY.html), [DROP OPERATOR FAMILY](DROP_OPERATOR_FAMILY.html), [ALTER OPERATOR CLASS](ALTER_OPERATOR_CLASS.html), [CREATE OPERATOR CLASS](CREATE_OPERATOR_CLASS.html), [DROP OPERATOR CLASS](DROP_OPERATOR_CLASS.html)

**Parent topic:** [SQL Command Reference](../sql_commands/sql_ref.html)

