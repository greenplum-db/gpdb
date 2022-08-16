---
title: DROP SCHEMA 
---

Removes a schema.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
DROP SCHEMA [IF EXISTS] <name> [, ...] [CASCADE | RESTRICT]
```

## <a id="section3"></a>Description 

`DROP SCHEMA` removes schemas from the database. A schema can only be dropped by its owner or a superuser. Note that the owner can drop the schema \(and thereby all contained objects\) even if he does not own some of the objects within the schema.

## <a id="section4"></a>Parameters 

IF EXISTS
:   Do not throw an error if the schema does not exist. A notice is issued in this case.

name
:   The name of the schema to remove.

CASCADE
:   Automatically drops any objects contained in the schema \(tables, functions, etc.\).

RESTRICT
:   Refuse to drop the schema if it contains any objects. This is the default.

## <a id="section5"></a>Examples 

Remove the schema `mystuff` from the database, along with everything it contains:

```
DROP SCHEMA mystuff CASCADE;
```

## <a id="section6"></a>Compatibility 

`DROP SCHEMA` is fully conforming with the SQL standard, except that the standard only allows one schema to be dropped per command. Also, the `IF EXISTS` option is a Greenplum Database extension.

## <a id="section7"></a>See Also 

[CREATE SCHEMA](CREATE_SCHEMA.html), [ALTER SCHEMA](ALTER_SCHEMA.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

