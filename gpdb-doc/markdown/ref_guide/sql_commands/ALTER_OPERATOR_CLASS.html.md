# ALTER OPERATOR CLASS 

Changes the definition of an operator class.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
ALTER OPERATOR CLASS <name> USING <index_method> RENAME TO <new_name>

ALTER OPERATOR CLASS <name> USING <index_method> OWNER TO { <new_owner> | CURRENT_USER | SESSION_USER }

ALTER OPERATOR CLASS <name> USING <index_method> SET SCHEMA <new_schema>
```

## <a id="section3"></a>Description 

`ALTER OPERATOR CLASS` changes the definition of an operator class.

You must own the operator class to use `ALTER OPERATOR CLASS`. To alter the owner, you must also be a direct or indirect member of the new owning role, and that role must have `CREATE` privilege on the operator class's schema. \(These restrictions enforce that altering the owner does not do anything you could not do by dropping and recreating the operator class. However, a superuser can alter ownership of any operator class anyway.\)

## <a id="section4"></a>Parameters 

name
:   The name \(optionally schema-qualified\) of an existing operator class.

index\_method
:   The name of the index method this operator class is for.

new\_name
:   The new name of the operator class.

new\_owner
:   The new owner of the operator class

new\_schema
:   The new schema for the operator class.

## <a id="section5"></a>Compatibility 

There is no `ALTER OPERATOR CLASS` statement in the SQL standard.

## <a id="section6"></a>See Also 

[CREATE OPERATOR CLASS](CREATE_OPERATOR_CLASS.html), [DROP OPERATOR CLASS](DROP_OPERATOR_CLASS.html), [ALTER OPERATOR FAMILY](ALTER_OPERATOR_FAMILY.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

