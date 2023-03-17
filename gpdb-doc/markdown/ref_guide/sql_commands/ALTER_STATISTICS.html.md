# ALTER STATISTICS 

Changes the definition of an extended statistics object.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
ALTER STATISTICS <name> RENAME TO <new_name>

ALTER STATISTICS <name> OWNER TO { <new_owner> | CURRENT_USER | SESSION_USER }

ALTER STATISTICS <name> SET SCHEMA <new_schema>
```

## <a id="section3"></a>Description 

`ALTER STATISTICS` changes the parameters of an existing extended statistics object. Any parameters not specifically set in the `ALTER STATISTICS` command retain their prior settings.

You must own the statistics object to use `ALTER STATISTICS`. To change a statistics object's schema, you must also have `CREATE` privilege on the new schema. To alter the owner, you must also be a direct or indirect member of the new owning role, and that role must have `CREATE` privilege on the statistics object's schema. \(These restrictions enforce that altering the owner doesn't do anything you couldn't do by dropping and recreating the statistics object. However, a superuser can alter ownership of any statistics object anyway.\)

## <a id="section4"></a>Parameters 

name
:   The name \(optionally schema-qualified\) of the statistics object to be altered.

new\_owner
:   The user name of the new owner of the statistics object.

new\_name
:   The new name for the statistics object.

new\_schema
:   The new schema for the statistics object.


## <a id="section6"></a>Compatibility 

There is no `ALTER STATISTICS` statement in the SQL standard.

## <a id="section7"></a>See Also 

[CREATE STATISTICS](CREATE_STATISTICS.html), [DROP STATISTICS](DROP_STATISTICS.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

