# DROP FUNCTION 

Removes a function.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
DROP FUNCTION [IF EXISTS] name ( [ [argmode] [argname] argtype [, ...] ] )
    [CASCADE | RESTRICT]
```

## <a id="section3"></a>Description 

`DROP FUNCTION` removes the definition of an existing function. To run this command the user must be the owner of the function. The argument types to the function must be specified, since several different functions may exist with the same name and different argument lists.

## <a id="section4"></a>Parameters 

IF EXISTS
:   Do not throw an error if the function does not exist. A notice is issued in this case.

name
:   The name \(optionally schema-qualified\) of an existing function.

argmode
:   The mode of an argument: either `IN`, `OUT`, `INOUT`, or `VARIADIC`. If omitted, the default is `IN`. Note that `DROP FUNCTION` does not actually pay any attention to `OUT` arguments, since only the input arguments are needed to determine the function's identity. So it is sufficient to list the `IN`, `INOUT`, and `VARIADIC` arguments.

argname
:   The name of an argument. Note that `DROP FUNCTION` does not actually pay any attention to argument names, since only the argument data types are needed to determine the function's identity.

argtype
:   The data type\(s\) of the function's arguments \(optionally schema-qualified\), if any.

CASCADE
:   Automatically drop objects that depend on the function \(such as operators\).

RESTRICT
:   Refuse to drop the function if any objects depend on it. This is the default.

## <a id="section5"></a>Examples 

This command removes the square root function:

```
DROP FUNCTION sqrt(integer);
```

## <a id="section6"></a>Compatibility 

A `DROP FUNCTION` statement is defined in the SQL standard, but it is not compatible with this command.

## <a id="section7"></a>See Also 

[CREATE FUNCTION](CREATE_FUNCTION.html), [ALTER FUNCTION](ALTER_FUNCTION.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

