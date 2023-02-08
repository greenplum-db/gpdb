# CREATE OPERATOR 

Defines a new operator.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
CREATE OPERATOR <name> ( 
       { FUNCTION | PROCEDURE } = <function\_name>
       [, LEFTARG = <left_type>] [, RIGHTARG = <right_type>]
       [, COMMUTATOR = <com_op>] [, NEGATOR = <neg_op>]
       [, RESTRICT = <res_proc>] [, JOIN = <join_proc>]
       [, HASHES] [, MERGES] )
```

## <a id="section3"></a>Description 

`CREATE OPERATOR` defines a new operator. The user who defines an operator becomes its owner. If a schema name is given, then the operator is created in the specified schema. Otherwise, it is created in the current schema.

The operator name is a sequence of up to `NAMEDATALEN`-1 \(63 by default\) characters from the following list: <code>+ - * / < > = ~ ! @ # % ^ & | ` ?</code>.

There are a few restrictions on your choice of name:

-   `--` and `/*` cannot appear anywhere in an operator name, since they will be taken as the start of a comment.
-   A multicharacter operator name cannot end in `+` or `-`, unless the name also contains at least one of these characters: <code>~ ! @ # % ^ & | ` ?</code>

    For example, `@-` is an allowed operator name, but `*-` is not. This restriction allows Greenplum Database to parse SQL-compliant commands without requiring spaces between tokens.

- The use of `=>` as an operator name is deprecated. It may be disallowed altogether in a future release.

The operator `!=` is mapped to `<>` on input, so these two names are always equivalent.

At least one of `LEFTARG` and `RIGHTARG` must be defined. For binary operators, both must be defined. For right unary operators, only `LEFTARG` should be defined, while for left unary operators only `RIGHTARG` should be defined.

**Note:**  Right unary, also called postfix, operators are deprecated and may be removed in a future Greenplum Database release.

The function\_name function must have been previously defined using `CREATE FUNCTION`, must be `IMMUTABLE`, and must be defined to accept the correct number of arguments \(either one or two\) of the indicated types.

In the syntax of `CREATE OPERATOR`, the keywords `FUNCTION` and `PROCEDURE` are equivalent, but the referenced function must in any case be a function, not a procedure. The use of the keyword `PROCEDURE` here is historical and deprecated.

The other clauses specify optional operator optimization clauses. Their meaning is detailed in the PostgreSQL [Operator Optimization Information](https://www.postgresql.org/docs/12/xoper-optimization.html) documentation.

To be able to create an operator, you must have `USAGE` privilege on the argument types and the return type, as well as `EXECUTE` privilege on the underlying function. If a commutator or negator operator is specified, you must own these operators.

## <a id="section4"></a>Parameters 

name
:   The \(optionally schema-qualified\) name of the operator to be defined. Refer to the *Description* above for allowable characters. The name can be schema-qualified, for example `CREATE OPERATOR myschema.+ (...)`. If not, then the operator is created in the current schema. Two operators in the same schema can have the same name if they operate on different data types. This is called *overloading*.

function\_name
:   The function used to implement this operator \(must be an `IMMUTABLE` function\).

left\_type
:   The data type of the operator's left operand, if any. This option would be omitted for a left-unary operator.

right\_type
:   The data type of the operator's right operand, if any. This option would be omitted for a right-unary operator.

com\_op
:   The commutator of this operator. The optional `COMMUTATOR` clause names an operator that is the commutator of the operator being defined. We say that operator A is the commutator of operator B if \(x A y\) equals \(y B x\) for all possible input values x, y. Notice that B is also the commutator of A. For example, operators `<` and `>` for a particular data type are usually each others commutators, and operator + is usually commutative with itself. But operator `-` is usually not commutative with anything. The left operand type of a commutable operator is the same as the right operand type of its commutator, and vice versa. So the name of the commutator operator is all that needs to be provided in the `COMMUTATOR` clause.

neg\_op
:   The negator of this operator. The optional `NEGATOR` clause names an operator that is the negator of the operator being defined. We say that operator A is the negator of operator B if both return Boolean results and \(x A y\) equals NOT \(x B y\) for all possible inputs x, y. Notice that B is also the negator of A. For example, `<` and `>=` are a negator pair for most data types. An operator's negator must have the same left and/or right operand types as the operator to be defined, so only the operator name need be given in the `NEGATOR` clause.

res\_proc
:   The restriction selectivity estimator function for this operator. The optional `RESTRICT` names a restriction selectivity estimation function for the operator. \(Note that this is a function name, not an operator name.\) `RESTRICT` clauses only make sense for binary operators that return `boolean`.

join\_proc
:   The join selectivity estimator function for this operator. The optional `JOIN` clause names a join selectivity estimation function for the operator. \(Note that this is a function name, not an operator name.\) `JOIN` clauses only make sense for binary operators that return `boolean`.

HASHES
:   Indicates this operator can support a hash join. The optional `HASHES` clause tells the system that it is permissible to use the hash join method for a join based on this operator. `HASHES` only makes sense for a binary operator that returns `boolean`. The hash join operator can only return true for pairs of left and right values that hash to the same hash code. If two values are put in different hash buckets, the join will never compare them, implicitly assuming that the result of the join operator must be false. Because of this, it never makes sense to specify `HASHES` for operators that do not represent equality.

MERGES
:   Indicates this operator can support a merge join. The `MERGES` clause, if present, tells the system that it is permissible to use the merge-join method for a join based on this operator. `MERGES` only makes sense for a binary operator that returns `boolean`, and in practice the operator must represent equality for some data type or pair of data types.

To give a schema-qualified operator name in com\_op or the other optional arguments, use the `OPERATOR()` syntax, for example:

```
COMMUTATOR = OPERATOR(myschema.===) ,
```

## <a id="section5"></a>Notes 

Refer to [User-defined Operators](https://www.postgresql.org/docs/12/xoper.html) in the PostgreSQL documentation for further information.

Any functions used to implement the operator must be defined as `IMMUTABLE`.

It is not possible to specify an operator's lexical precedence in `CREATE OPERATOR`, because the parser's precedence behavior is hard-wired. See [Operator Precedence](https://www.postgresql.org/docs/12/sql-syntax-lexical.html#SQL-PRECEDENCE) in the PostgreSQL documentation for precedence details.

Use [DROP OPERATOR](DROP_OPERATOR.html) to delete user-defined operators from a database. Use [ALTER OPERATOR](ALTER_OPERATOR.html) to modify operators in a database.

## <a id="section6"></a>Examples 

The following command defines a new operator, area-equality, for the data type `box`:

```
CREATE OPERATOR === (
    LEFTARG = box,
    RIGHTARG = box,
    FUNCTION = area_equal_function,
    COMMUTATOR = ===,
    NEGATOR = !==,
    RESTRICT = area_restriction_function,
    JOIN = area_join_function,
    HASHES, MERGES
);
```

The following example creates an operator for adding two complex numbers. The example assumes that we have already created the definition of type `complex`. First define the function that does the work, then define the operator:

```
CREATE FUNCTION complex_add(complex, complex)
    RETURNS complex
    AS 'filename', 'complex_add'
    LANGUAGE C IMMUTABLE STRICT;
CREATE OPERATOR + (
    leftarg = complex,
    rightarg = complex,
    procedure = complex_add,
    commutator = +
);
```

To use this operator in a query:

```
SELECT (a + b) AS c FROM test_complex;
```

## <a id="section7"></a>Compatibility 

`CREATE OPERATOR` is a Greenplum Database extension to the SQL standard. The SQL standard does not provide for user-defined operators.

## <a id="section8"></a>See Also 

[CREATE FUNCTION](CREATE_FUNCTION.html), [CREATE TYPE](CREATE_TYPE.html), [ALTER OPERATOR](ALTER_OPERATOR.html), [CREATE OPERATOR CLASS](CREATE_OPERATOR_CLASS.html), [DROP OPERATOR](DROP_OPERATOR.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

