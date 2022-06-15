# DROP SCHEMA 

Removes a schema.

## Synopsis 

``` {#sql_command_synopsis}
DROP SCHEMA [IF EXISTS] <name> [, ...] [CASCADE | RESTRICT]
```

## Description 

`DROP SCHEMA` removes schemas from the database. A schema can only be dropped by its owner or a superuser. Note that the owner can drop the schema \(and thereby all contained objects\) even if he does not own some of the objects within the schema.

## Parameters 

IF EXISTS
:   Do not throw an error if the schema does not exist. A notice is issued in this case.

name
:   The name of the schema to remove.

CASCADE
:   Automatically drops any objects contained in the schema \(tables, functions, etc.\).

RESTRICT
:   Refuse to drop the schema if it contains any objects. This is the default.

## Examples 

Remove the schema `mystuff` from the database, along with everything it contains:

```
DROP SCHEMA mystuff CASCADE;
```

## Compatibility 

`DROP SCHEMA` is fully conforming with the SQL standard, except that the standard only allows one schema to be dropped per command. Also, the `IF EXISTS` option is a Greenplum Database extension.

## See Also 

[CREATE SCHEMA](CREATE_SCHEMA.html), [ALTER SCHEMA](ALTER_SCHEMA.html)

**Parent topic:** [SQL Command Reference](../sql_commands/sql_ref.html)

