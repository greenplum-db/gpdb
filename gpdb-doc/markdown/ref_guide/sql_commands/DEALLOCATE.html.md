# DEALLOCATE 

Deallocates a prepared statement.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
DEALLOCATE [PREPARE] { <name> | ALL }
```

## <a id="section3"></a>Description 

`DEALLOCATE` is used to deallocate a previously prepared SQL statement. If you do not explicitly deallocate a prepared statement, it is deallocated when the session ends.

For more information on prepared statements, see [PREPARE](PREPARE.html).

## <a id="section4"></a>Parameters 

PREPARE
:   Optional key word which is ignored.

name
:   The name of the prepared statement to deallocate.

ALL
:   Deallocate all prepared statements

## <a id="section5"></a>Examples 

Deallocate the previously prepared statement named `insert_names`:

```
DEALLOCATE insert_names;
```

## <a id="section6"></a>Compatibility 

The SQL standard includes a `DEALLOCATE` statement, but it is only for use in embedded SQL.

## <a id="section7"></a>See Also 

[EXECUTE](EXECUTE.html), [PREPARE](PREPARE.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

