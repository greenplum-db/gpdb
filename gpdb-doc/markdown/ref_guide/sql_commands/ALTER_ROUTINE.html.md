# ALTER ROUTINE 

Changes the definition of a routine.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
ALTER ROUTINE <name> [ ( [ [<argmode>] [<argname>] <argtype> [, ...] ] ) ] 
   <action> [, ... ] [RESTRICT]

ALTER ROUTINE <name> [ ( [ [<argmode>] [<argname>] <argtype> [, ...] ] ) ]
   RENAME TO <new_name>

ALTER ROUTINE <name> [ ( [ [<argmode>] [<argname>] <argtype> [, ...] ] ) ]
   OWNER TO { <new_owner> | CURRENT_USER | SESSION_USER }

ALTER ROUTINE <name> [ ( [ [<argmode>] [<argname>] <argtype> [, ...] ] ) ]
   SET SCHEMA <new_schema>

ALTER ROUTINE <name> [ ( [ [<argmode>] [<argname>] <argtype> [, ...] ] ) ]
   DEPENDS ON EXTENSION <extension_name>

where <action> is one of (depending on the type of routine):

    { IMMUTABLE | STABLE | VOLATILE }
    [ NOT ] LEAKPROOF
    { [EXTERNAL] SECURITY INVOKER | [EXTERNAL] SECURITY DEFINER }
    PARALLEL { UNSAFE | RESTRICTED | SAFE }
    EXECUTE ON { ANY | MASTER | ALL SEGMENTS | INITPLAN }
    COST <execution_cost>
    ROWS <result_rows>
    SET <configuration_parameter> { TO | = } { <value> | DEFAULT }
    SET <configuration_parameter> FROM CURRENT
    RESET <configuration_parameter>
    RESET ALL
```

## <a id="section3"></a>Description 

`ALTER ROUTINE` changes the definition of a routine, which can be an aggregate function, a normal function, or a procedure. Refer to [ALTER AGGREGATE](ALTER_AGGREGATE.html), [ALTER FUNCTION](ALTER_FUNCTION.html), and [ALTER PROCEDURE](ALTER_PROCEDURE.html) for the description of the parameters, more examples, and further details.


## <a id="section6"></a>Examples 

To rename the routine `foo` for type `integer` to `foobar`:

```
ALTER ROUTINE foo(integer) RENAME TO foobar;
```

This command will work independent of whether `foo` is an aggregate, function, or procedure.

## <a id="section7"></a>Compatibility 

This statement is partially compatible with the `ALTER ROUTINE` statement in the SQL standard. Refer to [ALTER FUNCTION](ALTER_FUNCTION.html) and [ALTER PROCEDURE](ALTER_PROCEDURE.html) for more details. Allowing routine names to refer to aggregate functions is a Greenplum Database extension.

## <a id="section8"></a>See Also 

[ALTER AGGREGATE](ALTER_AGGREGATE.html), [ALTER FUNCTION](ALTER_FUNCTION.html), [ALTER PROCEDURE](ALTER_PROCEDURE.html), [DROP ROUTINE](DROP_ROUTINE.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

