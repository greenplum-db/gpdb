# EXECUTE 

Runs a prepared SQL statement.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
EXECUTE <name> [ (<parameter> [, ...] ) ]
```

## <a id="section3"></a>Description 

`EXECUTE` is used to run a previously prepared statement. Since prepared statements only exist for the duration of a session, the prepared statement must have been created by a `PREPARE` statement run earlier in the current session.

If the `PREPARE` statement that created the statement specified some parameters, a compatible set of parameters must be passed to the `EXECUTE` statement, or else Greenplum Database raises an error. Because \(unlike functions\) prepared statements are not overloaded based on the type or number of their parameters, the name of a prepared statement must be unique within a database session.

For more information on the creation and usage of prepared statements, see [PREPARE](PREPARE.html).

## <a id="section4"></a>Parameters 

name
:   The name of the prepared statement to run.

parameter
:   The actual value of a parameter to the prepared statement. This must be an expression yielding a value that is compatible with the data type of this parameter, as was determined when the prepared statement was created.

## <a id="section4b"></a>Outputs

The command tag returned by `EXECUTE` is that of the prepared statement, and not `EXECUTE`.

## <a id="section5"></a>Examples 

Create a prepared statement for an `INSERT` statement, and then run it:

```
PREPARE fooplan (int, text, bool, numeric) AS
    INSERT INTO foo VALUES($1, $2, $3, $4);
EXECUTE fooplan(1, 'Hunter Valley', 't', 200.00);
```

## <a id="section6"></a>Compatibility 

The SQL standard includes an `EXECUTE` statement, but it is only for use in embedded SQL. This version of the `EXECUTE` statement also uses a somewhat different syntax.

## <a id="section7"></a>See Also 

[DEALLOCATE](DEALLOCATE.html), [PREPARE](PREPARE.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

