# ALTER PROCEDURE 

Changes the definition of a procedure.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
ALTER PROCEDURE <name> [ ( [ [<argmode>] [<argname>] <argtype> [, ...] ] ) ] 
   <action> [, ... ] [RESTRICT]

ALTER PROCEDURE <name> [ ( [ [<argmode>] [<argname>] <argtype> [, ...] ] ) ]
   RENAME TO <new_name>

ALTER PROCEDURE <name> [ ( [ [<argmode>] [<argname>] <argtype> [, ...] ] ) ]
   OWNER TO { <new_owner> | CURRENT_USER | SESSION_USER }

ALTER PROCEDURE <name> [ ( [ [<argmode>] [<argname>] <argtype> [, ...] ] ) ]
   SET SCHEMA <new_schema>

ALTER PROCEDURE <name> [ ( [ [<argmode>] [<argname>] <argtype> [, ...] ] ) ]
   DEPENDS ON EXTENSION <extension_name>

where <action> is one of:

    { [EXTERNAL] SECURITY INVOKER | [EXTERNAL] SECURITY DEFINER }
    SET <configuration_parameter> { TO | = } { <value> | DEFAULT }
    SET <configuration_parameter> FROM CURRENT
    RESET <configuration_parameter>
    RESET ALL
```

## <a id="section3"></a>Description 

`ALTER PROCEDURE` changes the definition of a procedure.

You must own the procedure to use `ALTER PROCEDURE`. To change a procedure's schema, you must also have `CREATE` privilege on the new schema. To alter the owner, you must also be a direct or indirect member of the new owning role, and that role must have `CREATE` privilege on the procedure's schema. \(These restrictions enforce that altering the owner doesn't do anything you couldn't do by dropping and recreating the procedure. However, a superuser can alter ownership of any procedure anyway.\)


## <a id="section4"></a>Parameters 

name
:   The name \(optionally schema-qualified\) of an existing procedure. If no argument list is specified, the name must be unique in its schema.

argmode
:   The mode of an argument: `IN` or `VARIADIC`. If omitted, the default is `IN`.

argname
:   The name of an argument. Note that `ALTER PROCEDURE` does not actually pay any attention to argument names, since only the argument data types are needed to determine the function's identity.

argtype
:   The data type\(s\) of the procedure's arguments \(optionally schema-qualified\), if any.

new\_name
:   The new name of the procedure.

new\_owner
:   The new owner of the procedure. Note that if the procedure is marked `SECURITY DEFINER`, it will subsequently run as the new owner.

new\_schema
:   The new schema for the procedure.

extension\_name
:   The name of the extension that the procedure is to depend on.

\[ EXTERNAL \] SECURITY INVOKER
\[ EXTERNAL \] SECURITY DEFINER
:   Change whether the procedure is a security definer or not. The key word `EXTERNAL` is ignored for SQL conformance. See [CREATE PROCEDURE](CREATE_PROCEDURE.html) for more information about this capability.

configuration\_parameter
value
:   Set or change the value of a configuration parameter when the procedure is called. If value is `DEFAULT` or, equivalently, `RESET` is used, the procedure-local setting is removed, and the procedure runs with the value present in its environment. Use `RESET ALL` to clear all procedure-local settings. `SET FROM CURRENT` saves the value of the parameter that is current when `ALTER PROCEDURE` is run as the value to be applied when the procedure is entered.
:   See [SET](SET.html) for more information about allowed parameter names and values.

RESTRICT
:   Ignored for conformance with the SQL standard.

## <a id="section5"></a>Notes 

Greenplum Database has limitations on the use of functions defined as `STABLE` or `VOLATILE`. See [CREATE FUNCTION](CREATE_FUNCTION.html) for more information.

## <a id="section6"></a>Examples 

To rename the procedure `insert_data` with two arguments of type `integer` to `insert_record`:

```
ALTER PROCEDURE insert_data(integer, integer) RENAME TO insert_record;
```

To change the owner of the procedure `insert_data` with two arguments of type `integer` to `joe`:

```
ALTER PROCEDURE insert_data(integer, integer) OWNER TO joe;
```

To change the schema of the procedure `insert_data` with two arguments of type `integer` to `accounting`:

```
ALTER PROCEDURE insert_data(integer, integer) SET SCHEMA accounting;
```

To mark the procedure `insert_data(integer, integer)` as being dependent on the extension `myext`:

```
ALTER PROCEDURE insert_data(integer, integer) DEPENDS ON EXTENSION myext;
```

To adjust the search path that is automatically set for a procedure:

```
ALTER PROCEDURE check_password(text) SET search_path = admin, pg_temp;
```

To disable automatic setting of `search_path` for a procedure:

```
ALTER PROCEDURE check_password(text) RESET search_path;
```

The procedure will now execute with whatever search path is used by its caller.

## <a id="section7"></a>Compatibility 

This statement is partially compatible with the `ALTER PROCEDURE` statement in the SQL standard. The standard allows more properties of a procedure to be modified, but does not provide the ability to rename a procedure, make a procedure a security definer, attach configuration parameter values to a procedure, or change the owner, schema, or volatility of a procedure. The standard also requires the `RESTRICT` key word, which is optional in Greenplum Database.

## <a id="section8"></a>See Also 

[CREATE PROCEDURE](CREATE_PROCEDURE.html), [DROP PROCEDURE](DROP_PROCEDURE.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

