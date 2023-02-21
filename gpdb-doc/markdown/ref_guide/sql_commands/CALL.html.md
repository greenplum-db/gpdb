# CALL 

Invokes a procedure.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
CALL <name> ( [ <argument> ] [, ...] )
```

## <a id="section3"></a>Description 

`CALL` executes a procedure.

If the procedure has any output parameters, then a result row will be returned, containing the values of those parameters.

## <a id="section4"></a>Parameters 

name
:   The name \(optionally schema-qualified\) of the procedure.

argument
:   An input argument for the procedure call.

## <a id="section4"></a>Notes

The user must have `EXECUTE` privilege on the procedure in order to be allowed to invoke it.

To call a function \(not a procedure\), use [SELECT](SELECT.html) instead.

If `CALL` is invoked in a transaction block, then the called procedure cannot run transaction control statements. Transaction control statements are only allowed if `CALL` is invoked in its own transaction.

PL/pgSQL handles output parameters in `CALL` commands differently; refer to [Calling a Procedure](https://www.postgresql.org/docs/12/plpgsql-control-structures.html#PLPGSQL-STATEMENTS-CALLING-PROCEDURE) in the PostgreSQL documentation for more information.

## <a id="section5"></a>Examples 

```
CALL do_db_maintenance();
```

## <a id="section6"></a>Compatibility 

`CALL` conforms to the SQL standard.

## <a id="section7"></a>See Also 

[CREATE PROCEDURE](CREATE_PROCEDURE.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

