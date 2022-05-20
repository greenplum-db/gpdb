# DROP CONVERSION 

Removes a conversion.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
DROP CONVERSION [IF EXISTS] <name> [CASCADE | RESTRICT]
```

## <a id="section3"></a>Description 

`DROP CONVERSION` removes a previously defined conversion. To be able to drop a conversion, you must own the conversion.

## <a id="section4"></a>Parameters 

IF EXISTS
:   Do not throw an error if the conversion does not exist. A notice is issued in this case.

name
:   The name of the conversion. The conversion name may be schema-qualified.

CASCADE
RESTRICT
:   These keywords have no effect since there are no dependencies on conversions.

## <a id="section5"></a>Examples 

Drop the conversion named `myname`:

```
DROP CONVERSION myname;
```

## <a id="section6"></a>Compatibility 

There is no `DROP CONVERSION` statement in the SQL standard. The standard has `CREATE TRANSLATION` and `DROP TRANSLATION` statements that are similar to the Greenplum Database `CREATE CONVERSION` and `DROP CONVERSION` statements.

## <a id="section7"></a>See Also 

[ALTER CONVERSION](ALTER_CONVERSION.html), [CREATE CONVERSION](CREATE_CONVERSION.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

