# DROP TYPE 

Removes a data type.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
DROP TYPE [IF EXISTS] <name> [, ...] [CASCADE | RESTRICT]
```

## <a id="section3"></a>Description 

`DROP TYPE` will remove a user-defined data type. Only the owner of a type can remove it.

## <a id="section4"></a>Parameters 

IF EXISTS
:   Do not throw an error if the type does not exist. A notice is issued in this case.

name
:   The name \(optionally schema-qualified\) of the data type to remove.

CASCADE
:   Automatically drop objects that depend on the type \(such as table columns, functions, operators\).

RESTRICT
:   Refuse to drop the type if any objects depend on it. This is the default.

## <a id="section5"></a>Examples 

Remove the data type `box`;

```
DROP TYPE box;
```

## <a id="section6"></a>Compatibility 

This command is similar to the corresponding command in the SQL standard, apart from the `IF EXISTS` option, which is a Greenplum Database extension. But note that much of the `CREATE TYPE` command and the data type extension mechanisms in Greenplum Database differ from the SQL standard.

## <a id="section7"></a>See Also 

[ALTER TYPE](ALTER_TYPE.html), [CREATE TYPE](CREATE_TYPE.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

