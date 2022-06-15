# DROP CAST 

Removes a cast.

## Synopsis 

``` {#sql_command_synopsis}
DROP CAST [IF EXISTS] (<sourcetype> AS <targettype>) [CASCADE | RESTRICT]
```

## Description 

`DROP CAST` will delete a previously defined cast. To be able to drop a cast, you must own the source or the target data type. These are the same privileges that are required to create a cast.

## Parameters 

IF EXISTS
:   Do not throw an error if the cast does not exist. A notice is issued in this case.

sourcetype
:   The name of the source data type of the cast.

targettype
:   The name of the target data type of the cast.

CASCADE
RESTRICT
:   These keywords have no effect since there are no dependencies on casts.

## Examples 

To drop the cast from type `text` to type `int`:

```
DROP CAST (text AS int);
```

## Compatibility 

There `DROP CAST` command conforms to the SQL standard.

## See Also 

[CREATE CAST](CREATE_CAST.html)

**Parent topic:** [SQL Command Reference](../sql_commands/sql_ref.html)

