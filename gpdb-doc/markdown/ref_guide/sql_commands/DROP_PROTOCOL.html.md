# DROP PROTOCOL 

Removes a data access protocol from a database.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
DROP PROTOCOL [IF EXISTS] <name>
```

## <a id="section3"></a>Description 

`DROP PROTOCOL` removes the specified protocol from a database. You specify a protocol name in the `CREATE EXTERNAL TABLE` command to read data from or write data to an external data source.

You must be a superuser or the protocol owner to drop a protocol.

> **Caution** If you drop a data access prococol, external tables that have been defined specifying the protocol will no longer be able to access the external data source.

## <a id="section4"></a>Parameters 

IF EXISTS
:   Do not throw an error if the protocol does not exist. Greenplum Database issues a notice in this case.

name
:   The name of an existing data access protocol.

## <a id="section5"></a>Notes 

Dropping a data access protocol, does not drop the protocol's call handlers. You must drop these functions manually.

Be sure to remove any shared libraries that were used by the protocol from the Greenplum Database hosts.

## <a id="section6"></a>Compatibility 

`DROP PROTOCOL` is a Greenplum Database extension.

## <a id="section7"></a>See Also 

[CREATE EXTERNAL TABLE](CREATE_EXTERNAL_TABLE.html), [CREATE PROTOCOL](CREATE_PROTOCOL.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

