# DROP EXTERNAL TABLE 

Removes an external table definition.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
DROP EXTERNAL [WEB] TABLE [IF EXISTS] <name> [CASCADE | RESTRICT]
```

## <a id="section3"></a>Description 

`DROP EXTERNAL TABLE` drops an existing external table definition from the database system. The external data sources or files are not deleted. To run this command you must be the owner of the external table.

## <a id="section4"></a>Parameters 

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

## <a id="section5"></a>Examples 

Remove the external table named `staging` if it exists:

```
DROP EXTERNAL TABLE IF EXISTS staging;
```

## <a id="section6"></a>Compatibility 

There is no `DROP EXTERNAL TABLE` statement in the SQL standard.

## <a id="section7"></a>See Also 

[CREATE EXTERNAL TABLE](CREATE_EXTERNAL_TABLE.html), [ALTER EXTERNAL TABLE](ALTER_EXTERNAL_TABLE.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

