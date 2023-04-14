# DROP SERVER 

Removes a foreign server descriptor.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
DROP SERVER [ IF EXISTS ] <name> [, ...] [ CASCADE | RESTRICT ]
```

## <a id="section3"></a>Description 

`DROP SERVER` removes an existing foreign server descriptor. The user running this command must be the owner of the server.

## <a id="section4"></a>Parameters 

IF EXISTS
:   Do not throw an error if the server does not exist. Greenplum Database issues a notice in this case.

name
:   The name of an existing server.

CASCADE
:   Automatically drop objects that depend on the server \(such as user mappings\), and in turn all objects that depend on those objects.

RESTRICT
:   Refuse to drop the server if any object depends on it. This is the default.

## <a id="section6"></a>Examples 

Drop the server named `foo` if it exists:

```
DROP SERVER IF EXISTS foo;
```

## <a id="section7"></a>Compatibility 

`DROP SERVER` conforms to ISO/IEC 9075-9 \(SQL/MED\). The `IF EXISTS` clause is a Greenplum Database extension.

## <a id="section8"></a>See Also 

[CREATE SERVER](CREATE_SERVER.html), [ALTER SERVER](ALTER_SERVER.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

