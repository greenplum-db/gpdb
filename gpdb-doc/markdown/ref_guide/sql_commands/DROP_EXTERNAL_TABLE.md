# DROP EXTERNAL TABLE 

Removes an external table definition.

## Synopsis 

``` {#sql_command_synopsis}
DROP EXTERNAL [WEB] TABLE [IF EXISTS] <name> [CASCADE | RESTRICT]
```

## Description 

`DROP EXTERNAL TABLE` drops an existing external table definition from the database system. The external data sources or files are not deleted. To execute this command you must be the owner of the external table.

## Parameters 

WEB
:   Optional keyword for dropping external web tables.

IF EXISTS
:   Do not throw an error if the external table does not exist. A notice is issued in this case.

name
:   The name \(optionally schema-qualified\) of an existing external table.

CASCADE
:   Automatically drop objects that depend on the external table \(such as views\).

RESTRICT
:   Refuse to drop the external table if any objects depend on it. This is the default.

## Examples 

Remove the external table named `staging` if it exists:

```
DROP EXTERNAL TABLE IF EXISTS staging;
```

## Compatibility 

There is no `DROP EXTERNAL TABLE` statement in the SQL standard.

## See Also 

[CREATE EXTERNAL TABLE](CREATE_EXTERNAL_TABLE.html)

**Parent topic:** [SQL Command Reference](../sql_commands/sql_ref.html)

