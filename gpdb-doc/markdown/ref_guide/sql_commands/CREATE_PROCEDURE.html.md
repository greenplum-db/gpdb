# CREATE PROCEDURE 

Defines a new procedure.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
CREATE [OR REPLACE] PROCEDURE <name>    
    ( [ [<argmode>] [<argname>] <argtype> [ { DEFAULT | = } <default_expr> ] [, ...] ] )
  { LANGUAGE <lang_name>
    | TRANSFORM { FOR TYPE <type_name> } [, ... ]
    | { [ EXTERNAL ] SECURITY INVOKER | [ EXTERNAL ] SECURITY DEFINER }
    | SET <configuration_parameter> { TO <value> | = <value> | FROM CURRENT }
    | AS '<definition>'
    | AS '<obj_file>', '<link_symbol>' 
  } ...
```

## <a id="section3"></a>Description 

`CREATE PROCEDURE` defines a new procedure. `CREATE OR REPLACE PROCEDURE` either creates a new procedure, or replaces an existing definition. To define a procedure, the user must have the `USAGE` privilege on the language.

If a schema name is included, then the procedure is created in the specified schema. Otherwise it is created in the current schema. The name of the new procedure must not match any existing procedure with the same input argument types in the same schema. However, procedures and functions of different argument types may share a name \(overloading\).

To update the current definition of an existing procedure, use `CREATE OR REPLACE PROCEDURE`. It is not possible to change the name or argument types of a procedure this way \(this would actually create a new, distinct procedure\).

When `CREATE OR REPLACE PROCEDURE` is used to replace an existing procedure, the ownership and permissions of the procedure do not change. All other procedure properties are assigned the values specified or implied in the command. You must own the procedure to replace it \(this includes being a member of the owning role\).

The user that creates the procedure becomes the owner of the procedure.

To be able to create a procedure, you must have `USAGE` privilege on the argument types.

For more information about creating procedures, refer to the [User Defined Procedures](https://www.postgresql.org/docs/12/xproc.html) section of the PostgreSQL documentation.

## <a id="section5"></a>Parameters 

name
:   The name \(optionally schema-qualified\) of the procedure to create.

argmode
:   The mode of an argument: either `IN`, `INOUT`, or `VARIADIC`. If omitted, the default is `IN`. \(`OUT` arguments are currently not supported for procedures. Use `INOUT` instead.\)

argname
:   The name of an argument.

argtype
:   The data type\(s\) of the procedure's arguments \(optionally schema-qualified\), if any. The argument types may be base, composite, or domain types, or may reference the type of a table column.

:   Depending on the implementation language it may also be allowed to specify pseudotypes such as `cstring`. Pseudotypes indicate that the actual argument type is either incompletely specified, or outside the set of ordinary SQL data types.

:   The type of a column is referenced by writing `table_name.column_name%TYPE`. Using this feature can sometimes help make a procedure independent of changes to the definition of a table.

default\_expr
:   An expression to be used as the default value if the parameter is not specified. The expression must be coercible to the argument type of the parameter. Each input parameter in the argument list that follows a parameter with a default value must have a default value as well.

lang\_name
:   The name of the language that the procedure is implemented in. May be `SQL`, `C`, `internal`, or the name of a user-defined procedural language, e.g. `plpgsql`. Enclosing the name in single quotes is deprecated and requires matching case.

TRANSFORM { FOR TYPE type\_name } [, ... ] }
:   Lists which transforms a call to the procedure should apply. Transforms convert between SQL types and language-specific data types. Procedural language implementations usually have hardcoded knowledge of the built-in types, so those don't need to be listed here. If a procedural language implementation does not know how to handle a type and no transform is supplied, it will fall back to a default behavior for converting data types, but this depends on the implementation.

\[EXTERNAL\] SECURITY INVOKER
\[EXTERNAL\] SECURITY DEFINER
:   `SECURITY INVOKER` \(the default\) indicates that the procedure is to be run with the privileges of the user that calls it.
:   `SECURITY DEFINER` specifies that the procedure is to be run with the privileges of the user that created it.
:   The key word `EXTERNAL` is allowed for SQL conformance, but it is optional since, unlike in SQL, this feature applies to all procedures not just external ones.
:   A `SECURITY DEFINER` procedure cannot execute transaction control statements \(for example, `COMMIT` and `ROLLBACK`, depending on the language\).

configuration\_parameter
value
:   The `SET` clause applies a value to a session configuration parameter when the procedure is entered. The configuration parameter is restored to its prior value when the procedure exits. `SET FROM CURRENT` saves the value of the parameter that is current when `CREATE PROCEDURE` is run as the value to be applied when the procedure is entered.
:   If a `SET` clause is attached to a procedure, then the effects of a `SET LOCAL` command executed inside the procedure for the same variable are restricted to the procedure: the configuration parameter's prior value is still restored at procedure exit. However, an ordinary `SET` command (without `LOCAL`) overrides the `SET` clause, much as it would do for a previous `SET LOCAL` command: the effects of such a command will persist after procedure exit, unless the current transaction is rolled back.
:   If a `SET` clause is attached to a procedure, then that procedure cannot execute transaction control statements \(for example, `COMMIT` and `ROLLBACK`, depending on the language\).
:   See [SET](SET.html) for more information about allowed parameter names and values.

definition
:   A string constant defining the procedure; the meaning depends on the language. It may be an internal procedure name, the path to an object file, an SQL command, or text in a procedural language.

:   It is often helpful to use dollar quoting \(refer to [Dollar-Quoted String Constants
](https://www.postgresql.org/docs/12/sql-syntax-lexical.html#SQL-SYNTAX-DOLLAR-QUOTING) in the PostgreSQL documentation\) to write the procedure definition string, rather than the normal single quote syntax. Without dollar quoting, any single quotes or backslashes in the procedure definition must be escaped by doubling them.

obj\_file, link\_symbol
:   This form of the `AS` clause is used for dynamically loadable C language procedures when the procedure name in the C language source code is not the same as the name of the SQL procedure. The string obj\_file is the name of the file containing the dynamically loadable object, and is interpreted as for the [LOAD](LOAD.html) command. The string link\_symbol is the name of the procedure in the C language source code. If the link symbol is omitted, it is assumed to be the same as the name of the SQL procedure being defined.
:   When repeated `CREATE PROCEDURE` calls refer to the same object file, the file is only loaded once per session. To unload and reload the file \(perhaps during development\), start a new session.

## <a id="section6"></a>Notes 

See [CREATE FUNCTION](CREATE_FUNCTION.html) for more details on function creation that also apply to procedures.

Use [CALL](CALL.html) to execute a procedure.

## <a id="section8"></a>Examples 

```
CREATE PROCEDURE insert_data(a integer, b integer)
LANGUAGE SQL
AS $$
INSERT INTO tbl VALUES (a);
INSERT INTO tbl VALUES (b);
$$;

CALL insert_data(1, 2);
```

## <a id="section9"></a>Compatibility 

A `CREATE PROCEDURE` command is defined in the SQL standard. The Greenplum Database version is similar but not fully compatible. For details see also [CREATE FUNCTION](CREATE_FUNCTION.html).

## <a id="section10"></a>See Also 

[ALTER PROCEDURE](ALTER_PROCEDURE.html), [DROP PROCEDURE](DROP_PROCEDURE.html), [CREATE FUNCTION](CREATE_FUNCTION.html), [CALL](CALL.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

