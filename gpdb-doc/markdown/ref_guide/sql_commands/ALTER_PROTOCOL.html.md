# ALTER PROTOCOL 

Changes the definition of a protocol.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
ALTER PROTOCOL <name> RENAME TO <newname>

ALTER PROTOCOL <name> OWNER TO <newowner>
```

## <a id="section3"></a>Description 

`ALTER PROTOCOL` changes the definition of a protocol. Only the protocol name or owner can be altered.

You must own the protocol to use `ALTER PROTOCOL`. To alter the owner, you must also be a direct or indirect member of the new owning role, and that role must have `CREATE` privilege on schema of the conversion.

These restrictions are in place to ensure that altering the owner only makes changes that could by made by dropping and recreating the protocol. Note that a superuser can alter ownership of any protocol.

## <a id="section4"></a>Parameters 

name
:   The name \(optionally schema-qualified\) of an existing protocol.

newname
:   The new name of the protocol.

newowner
:   The new owner of the protocol.

## <a id="section5"></a>Examples 

To rename the conversion `GPDBauth` to `GPDB_authentication`:

```
ALTER PROTOCOL GPDBauth RENAME TO GPDB_authentication;
```

To change the owner of the conversion `GPDB_authentication` to `joe`:

```
ALTER PROTOCOL GPDB_authentication OWNER TO joe;
```

## <a id="section6"></a>Compatibility 

There is no `ALTER PROTOCOL` statement in the SQL standard.

## <a id="seea"></a>See Also 

[CREATE EXTERNAL TABLE](CREATE_EXTERNAL_TABLE.html), [CREATE PROTOCOL](CREATE_PROTOCOL.html), [DROP PROTOCOL](DROP_PROTOCOL.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

