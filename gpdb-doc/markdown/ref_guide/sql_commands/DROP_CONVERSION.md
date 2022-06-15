# DROP CONVERSION 

Removes a conversion.

## Synopsis 

``` {#sql_command_synopsis}
DROP CONVERSION [IF EXISTS] <name> [CASCADE | RESTRICT]
```

## Description 

`DROP CONVERSION` removes a previously defined conversion. To be able to drop a conversion, you must own the conversion.

## Parameters 

IF EXISTS
:   Do not throw an error if the conversion does not exist. A notice is issued in this case.

name
:   The name of the conversion. The conversion name may be schema-qualified.

CASCADE
RESTRICT
:   These keywords have no effect since there are no dependencies on conversions.

## Examples 

Drop the conversion named `myname`:

```
DROP CONVERSION myname;
```

## Compatibility 

There is no `DROP CONVERSION` statement in the SQL standard.

## See Also 

[ALTER CONVERSION](ALTER_CONVERSION.html), [CREATE CONVERSION](CREATE_CONVERSION.html)

**Parent topic:** [SQL Command Reference](../sql_commands/sql_ref.html)

