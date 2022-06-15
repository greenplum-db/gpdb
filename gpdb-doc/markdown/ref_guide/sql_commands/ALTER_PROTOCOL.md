# ALTER PROTOCOL 

Changes the definition of a protocol.

## Synopsis 

``` {#sql_command_synopsis}
ALTER PROTOCOL <name> RENAME TO <newname>

ALTER PROTOCOL <name> OWNER TO <newowner>
```

## Description 

`ALTER PROTOCOL` changes the definition of a protocol. Only the protocol name or owner can be altered.

You must own the protocol to use `ALTER PROTOCOL`. To alter the owner, you must also be a direct or indirect member of the new owning role, and that role must have `CREATE` privilege on schema of the conversion.

These restrictions are in place to ensure that altering the owner only makes changes that could by made by dropping and recreating the protocol. Note that a superuser can alter ownership of any protocol.

## Parameters 

name
:   The name \(optionally schema-qualified\) of an existing protocol.

newname
:   The new name of the protocol.

newowner
:   The new owner of the protocol.

## Examples 

To rename the conversion `GPDBauth` to `GPDB_authentication`:

```
ALTER PROTOCOL GPDBauth RENAME TO GPDB_authentication;
```

To change the owner of the conversion `GPDB_authentication` to `joe`:

```
ALTER PROTOCOL GPDB_authentication OWNER TO joe;
```

## Compatibility 

There is no `ALTER PROTOCOL` statement in the SQL standard.

## See Also 

[CREATE EXTERNAL TABLE](CREATE_EXTERNAL_TABLE.html), [CREATE PROTOCOL](CREATE_PROTOCOL.html)

**Parent topic:** [SQL Command Reference](../sql_commands/sql_ref.html)

