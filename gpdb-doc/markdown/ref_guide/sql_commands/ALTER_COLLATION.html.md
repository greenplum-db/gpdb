---
title: ALTER COLLATION 
---

Changes the definition of a collation.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
ALTER COLLATION <name> RENAME TO <new_name>

ALTER COLLATION <name> OWNER TO <new_owner>

ALTER COLLATION <name> SET SCHEMA <new_schema>
```

## <a id="section4"></a>Parameters 

name
:   The name \(optionally schema-qualified\) of an existing collation.

new\_name
:   The new name of the collation.

new\_owner
:   The new owner of the collation.

new\_schema
:   The new schema for the collation.

## <a id="section3"></a>Description 

You must own the collation to use `ALTER COLLATION`. To alter the owner, you must also be a direct or indirect member of the new owning role, and that role must have `CREATE` privilege on the collation's schema. \(These restrictions enforce that altering the owner doesn't do anything you couldn't do by dropping and recreating the collation. However, a superuser can alter ownership of any collation anyway.\)

## <a id="section5"></a>Examples 

To rename the collation de\_DE to `german`:

```
ALTER COLLATION "de_DE" RENAME TO german;
```

To change the owner of the collation `en_US` to `joe`:

```
ALTER COLLATION "en_US" OWNER TO joe;
```

## <a id="section6"></a>Compatibility 

There is no `ALTER COLLATION` statement in the SQL standard.

## <a id="section7"></a>See Also 

[CREATE COLLATION](CREATE_COLLATION.html), [DROP COLLATION](DROP_COLLATION.html)

**Parent topic: **[SQL Commands](../sql_commands/sql_ref.html)

