# ALTER FUNCTION 

Changes the definition of a function.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
ALTER FUNCTION <name> [ ( [ [<argmode>] [<argname>] <argtype> [, ...] ] ) ] 
   <action> [, ... ] [RESTRICT]

ALTER FUNCTION <name> [ ( [ [<argmode>] [<argname>] <argtype> [, ...] ] ) ]
   RENAME TO <new_name>

ALTER FUNCTION <name> [ ( [ [<argmode>] [<argname>] <argtype> [, ...] ] ) ]
   OWNER TO { <new_owner> | CURRENT_USER | SESSION_USER }

ALTER FUNCTION <name> [ ( [ [<argmode>] [<argname>] <argtype> [, ...] ] ) ]
   SET SCHEMA <new_schema>

ALTER FUNCTION <name> [ ( [ [<argmode>] [<argname>] <argtype> [, ...] ] ) ]
   DEPENDS ON EXTENSION <extension_name>

where <action> is one of:

    { CALLED ON NULL INPUT | RETURNS NULL ON NULL INPUT | STRICT }
    { IMMUTABLE | STABLE | VOLATILE }
    [ NOT ] LEAKPROOF
    { [EXTERNAL] SECURITY INVOKER | [EXTERNAL] SECURITY DEFINER }
    PARALLEL { UNSAFE | RESTRICTED | SAFE }
    EXECUTE ON { ANY | MASTER | ALL SEGMENTS | INITPLAN }
    COST <execution_cost>
    ROWS <result_rows>
    SUPPORT <support_function>
    SET <configuration_parameter> { TO | = } { <value> | DEFAULT }
    SET <configuration_parameter> FROM CURRENT
    RESET <configuration_parameter>
    RESET ALL
```

## <a id="section3"></a>Description 

`ALTER FUNCTION` changes the definition of a function.

You must own the function to use `ALTER FUNCTION`. To change a function's schema, you must also have `CREATE` privilege on the new schema. To alter the owner, you must also be a direct or indirect member of the new owning role, and that role must have `CREATE` privilege on the function's schema. \(These restrictions enforce that altering the owner does not do anything you could not do by dropping and recreating the function. However, a superuser can alter ownership of any function anyway.\)

## <a id="section4"></a>Parameters 

name
:   The name \(optionally schema-qualified\) of an existing function. If no argument list is specified, the name must be unique in its schema.

argmode
:   The mode of an argument: either `IN`, `OUT`, `INOUT`, or `VARIADIC`. If omitted, the default is `IN`. Note that `ALTER FUNCTION` does not actually pay any attention to `OUT` arguments, since only the input arguments are needed to determine the function's identity. So it is sufficient to list the `IN`, `INOUT`, and `VARIADIC` arguments.

argname
:   The name of an argument. Note that [ALTER FUNCTION](ALTER_FUNCTION.html) does not actually pay any attention to argument names, since only the argument data types are needed to determine the function's identity.

argtype
:   The data type\(s\) of the function's arguments \(optionally schema-qualified\), if any.

new\_name
:   The new name of the function.

new\_owner
:   The new owner of the function. Note that if the function is marked `SECURITY DEFINER`, it will subsequently run as the new owner.

new\_schema
:   The new schema for the function.

extension\_name
:   The name of the extension that the function is to depend on.

CALLED ON NULL INPUT
RETURNS NULL ON NULL INPUT
STRICT
:   `CALLED ON NULL INPUT` changes the function so that it will be invoked when some or all of its arguments are null. `RETURNS NULL ON NULL INPUT` or `STRICT` changes the function so that it is not invoked if any of its arguments are null; instead, a null result is assumed automatically. See [CREATE FUNCTION](CREATE_FUNCTION.html) for more information.

IMMUTABLE
STABLE
VOLATILE
:   Change the volatility of the function to the specified setting. See [CREATE FUNCTION](CREATE_FUNCTION.html) for details.

\[ EXTERNAL \] SECURITY INVOKER
\[ EXTERNAL \] SECURITY DEFINER
:   Change whether the function is a security definer or not. The key word `EXTERNAL` is ignored for SQL conformance. See [CREATE FUNCTION](CREATE_FUNCTION.html) for more information about this capability.

PARALLEL
:   Change whether the function is deemed safe for parallelism. See [CREATE FUNCTION](CREATE_FUNCTION.html) for details.

LEAKPROOF
:   Change whether the function is considered leakproof or not. See [CREATE FUNCTION](CREATE_FUNCTION.html) for more information about this capability.

EXECUTE ON ANY
EXECUTE ON MASTER
EXECUTE ON ALL SEGMENTS
EXECUTE ON INITPLAN
:   The `EXECUTE ON` attributes specify where \(coordinator or segment instance\) a function runs when it is invoked during the query execution process.

:   `EXECUTE ON ANY` \(the default\) indicates that the function can be run on the coordinator, or any segment instance, and it returns the same result regardless of where it is run. Greenplum Database determines where the function runs.

:   `EXECUTE ON MASTER` indicates that the function must run only on the coordinator instance.

:   `EXECUTE ON ALL SEGMENTS` indicates that the function must run on all primary segment instances, but not the coordinator, for each invocation. The overall result of the function is the `UNION ALL` of the results from all segment instances.

:   `EXECUTE ON INITPLAN` indicates that the function contains an SQL command that dispatches queries to the segment instances and requires special processing on the coordinator instance by Greenplum Database when possible.

:   For more information about the `EXECUTE ON` attributes, see [CREATE FUNCTION](CREATE_FUNCTION.html).

COST execution\_cost
:   Change the estimated execution cost of the function. See [CREATE FUNCTION](CREATE_FUNCTION.html) for more information.

ROWS result\_rows
:   Change the estimated number of rows returned by a set-returning function. See [CREATE FUNCTION](CREATE_FUNCTION.html) for more information.

SUPPORT support\_function
:   Set or change the planner support function to use for this function. You must be superuser to use this option.
:   This option cannot be used to remove the support function altogether, since it must name a new support function. Use `CREATE OR REPLACE FUNCTION` if you require that.

configuration\_parameter
value
:   Set or change the value of a configuration parameter when the function is called. If value is `DEFAULT` or, equivalently, `RESET` is used, the function-local setting is removed, and the function runs with the value present in its environment. Use `RESET ALL` to clear all function-local settings. `SET FROM CURRENT` saves the value of the parameter that is current when `ALTER FUNCTION` is run as the value to be applied when the function is entered.
:   See [SET](SET.html) for more information about allowed parameter names and values.

RESTRICT
:   Ignored for conformance with the SQL standard.

## <a id="section5"></a>Notes 

Greenplum Database has limitations on the use of functions defined as `STABLE` or `VOLATILE`. See [CREATE FUNCTION](CREATE_FUNCTION.html) for more information.

## <a id="section6"></a>Examples 

To rename the function `sqrt` for type `integer` to `square_root`:

```
ALTER FUNCTION sqrt(integer) RENAME TO square_root;
```

To change the owner of the function `sqrt` for type `integer` to `joe`:

```
ALTER FUNCTION sqrt(integer) OWNER TO joe;
```

To change the schema of the function `sqrt` for type `integer` to `maths`:

```
ALTER FUNCTION sqrt(integer) SET SCHEMA maths;
```

To mark the function `sqrt` for type `integer` as being dependent on the extension `mathlib`:

```
ALTER FUNCTION sqrt(integer) DEPENDS ON EXTENSION mathlib;
```

To adjust the search path that is automatically set for a function:

```
ALTER FUNCTION check_password(text) search_path = admin, pg_temp;
```

To disable automatic setting of `search_path` for a function:

```
ALTER FUNCTION check_password(text) RESET search_path;
```

The function will now execute with whatever search path is used by its caller.

## <a id="section7"></a>Compatibility 

This statement is partially compatible with the `ALTER FUNCTION` statement in the SQL standard. The standard allows more properties of a function to be modified, but does not provide the ability to rename a function, make a function a security definer, attach configuration parameter values to a function, or change the owner, schema, or volatility of a function. The standard also requires the `RESTRICT` key word, which is optional in Greenplum Database.

## <a id="section8"></a>See Also 

[CREATE FUNCTION](CREATE_FUNCTION.html), [DROP FUNCTION](DROP_FUNCTION.html), [ALTER PROCEDURE](ALTER_PROCEDURE.html), [ALTER ROUTINE](ALTER_ROUTINE.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

