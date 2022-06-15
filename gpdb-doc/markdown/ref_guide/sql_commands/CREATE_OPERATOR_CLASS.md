# CREATE OPERATOR CLASS 

Defines a new operator class.

## Synopsis 

``` {#sql_command_synopsis}
CREATE OPERATOR CLASS <name> [DEFAULT] FOR TYPE <data_type>  
  USING <index_method> AS 
  { 
  OPERATOR <strategy_number> <op_name> [(<op_type>, <op_type>)] [RECHECK]
  | FUNCTION <support_number> <funcname> (<argument_type> [, ...] )
  | STORAGE <storage_type>
  } [, ... ]
```

## Description 

`CREATE OPERATOR CLASS` creates a new operator class. An operator class defines how a particular data type can be used with an index. The operator class specifies that certain operators will fill particular roles or strategies for this data type and this index method. The operator class also specifies the support procedures to be used by the index method when the operator class is selected for an index column. All the operators and functions used by an operator class must be defined before the operator class is created. Any functions used to implement the operator class must be defined as `IMMUTABLE`.

`CREATE OPERATOR CLASS` does not presently check whether the operator class definition includes all the operators and functions required by the index method, nor whether the operators and functions form a self-consistent set. It is the user's responsibility to define a valid operator class.

You must be a superuser to create an operator class.

## Parameters 

name
:   The \(optionally schema-qualified\) name of the operator class to be defined. Two operator classes in the same schema can have the same name only if they are for different index methods.

DEFAULT
:   Makes the operator class the default operator class for its data type. At most one operator class can be the default for a specific data type and index method.

data\_type
:   The column data type that this operator class is for.

index\_method
:   The name of the index method this operator class is for. Choices are `btree`, `bitmap`, and `gist`.

strategy\_number
:   The operators associated with an operator class are identified by *strategy numbers*, which serve to identify the semantics of each operator within the context of its operator class. For example, B-trees impose a strict ordering on keys, lesser to greater, and so operators like *less than* and *greater than or equal to* are interesting with respect to a B-tree. These strategies can be thought of as generalized operators. Each operator class specifies which actual operator corresponds to each strategy for a particular data type and interpretation of the index semantics. The corresponding strategy numbers for each index method are as follows:

    |Operation|Strategy Number|
    |---------|---------------|
    |less than|1|
    |less than or equal|2|
    |equal|3|
    |greater than or equal|4|
    |greater than|5|

    |Operation|Strategy Number|
    |---------|---------------|
    |strictly left of|1|
    |does not extend to right of|2|
    |overlaps|3|
    |does not extend to left of|4|
    |strictly right of|5|
    |same|6|
    |contains|7|
    |contained by|8|
    |does not extend above|9|
    |strictly below|10|
    |strictly above|11|
    |does not extend below|12|

operator\_name
:   The name \(optionally schema-qualified\) of an operator associated with the operator class.

op\_type
:   The operand data type\(s\) of an operator, or `NONE` to signify a left-unary or right-unary operator. The operand data types may be omitted in the normal case where they are the same as the operator class data type.

RECHECK
:   If present, the index is "lossy" for this operator, and so the rows retrieved using the index must be rechecked to verify that they actually satisfy the qualification clause involving this operator.

support\_number
:   Index methods require additional support routines in order to work. These operations are administrative routines used internally by the index methods. As with strategies, the operator class identifies which specific functions should play each of these roles for a given data type and semantic interpretation. The index method defines the set of functions it needs, and the operator class identifies the correct functions to use by assigning them to the *support function numbers* as follows:

    |Function|Support Number|
    |--------|--------------|
    |Compare two keys and return an integer less than zero, zero, or greater than zero, indicating whether the first key is less than, equal to, or greater than the second.|1|

    |Function|Support Number|
    |--------|--------------|
    |consistent - determine whether key satisfies the query qualifier.|1|
    |union - compute union of a set of keys.|2|
    |compress - compute a compressed representation of a key or value to be indexed.|3|
    |decompress - compute a decompressed representation of a compressed key.|4|
    |penalty - compute penalty for inserting new key into subtree with given subtree's key.|5|
    |picksplit - determine which entries of a page are to be moved to the new page and compute the union keys for resulting pages.|6|
    |equal - compare two keys and return true if they are equal.|7|

funcname
:   The name \(optionally schema-qualified\) of a function that is an index method support procedure for the operator class.

argument\_types
:   The parameter data type\(s\) of the function.

storage\_type
:   The data type actually stored in the index. Normally this is the same as the column data type, but the GiST index method allows it to be different. The `STORAGE` clause must be omitted unless the index method allows a different type to be used.

## Notes 

Because the index machinery does not check access permissions on functions before using them, including a function or operator in an operator class is the same as granting public execute permission on it. This is usually not an issue for the sorts of functions that are useful in an operator class.

The operators should not be defined by SQL functions. A SQL function is likely to be inlined into the calling query, which will prevent the optimizer from recognizing that the query matches an index.

Any functions used to implement the operator class must be defined as `IMMUTABLE`.

## Examples 

The following example command defines a GiST index operator class for the data type `_int4` \(array of int4\):

```
CREATE OPERATOR CLASS gist__int_ops
    DEFAULT FOR TYPE _int4 USING gist AS
        OPERATOR 3 &&,
        OPERATOR 6 = RECHECK,
        OPERATOR 7 @>,
        OPERATOR 8 <@,
        OPERATOR 20 @@ (_int4, query_int),
        FUNCTION 1 g_int_consistent (internal, _int4, int4),
        FUNCTION 2 g_int_union (bytea, internal),
        FUNCTION 3 g_int_compress (internal),
        FUNCTION 4 g_int_decompress (internal),
        FUNCTION 5 g_int_penalty (internal, internal, internal),
        FUNCTION 6 g_int_picksplit (internal, internal),
        FUNCTION 7 g_int_same (_int4, _int4, internal);
```

## Compatibility 

`CREATE OPERATOR CLASS` is a Greenplum Database extension. There is no `CREATE OPERATOR CLASS` statement in the SQL standard.

## See Also 

[ALTER OPERATOR CLASS](ALTER_OPERATOR_CLASS.html), [DROP OPERATOR CLASS](DROP_OPERATOR_CLASS.html), [CREATE FUNCTION](CREATE_FUNCTION.html)

**Parent topic:** [SQL Command Reference](../sql_commands/sql_ref.html)

