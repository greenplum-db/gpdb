# DROP PROTOCOL 

Removes a external table data access protocol from a database.

## Synopsis 

``` {#sql_command_synopsis}
DROP PROTOCOL [IF EXISTS] <name>
```

## Description 

`DROP PROTOCOL` removes the specified protocol from a database. A protocol name can be specified in the `CREATE EXTERNAL TABLE` command to read data from or write data to an external data source.

**Warning:** If you drop a data access prococol, external tables that have been defined with the protocol will no longer be able to access the external data source.

## Parameters 

IF EXISTS
:   Do not throw an error if the protocol does not exist. A notice is issued in this case.

name
:   The name of an existing data access protocol.

## Notes 

If you drop a data access protocol, the call handlers that defined in the database that are associated with the protocol are not dropped. You must drop the functions manually.

Shared libraries that were used by the protocol should also be removed from the Greenplum Database hosts.

## Compatibility 

`DROP PROTOCOL` is a Greenplum Database extension.

## See Also 

[CREATE EXTERNAL TABLE](CREATE_EXTERNAL_TABLE.html), [CREATE PROTOCOL](CREATE_PROTOCOL.html)

**Parent topic:** [SQL Command Reference](../sql_commands/sql_ref.html)

