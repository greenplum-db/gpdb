# DROP DOMAIN 

Removes a domain.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
DROP DOMAIN [IF EXISTS] <name> [, ...]  [CASCADE | RESTRICT]
```

## <a id="section3"></a>Description 

`DROP DOMAIN` removes a previously defined domain. Only the owner of a domain can remove it.

## <a id="section4"></a>Parameters 

IF EXISTS
:   Do not throw an error if the domain does not exist. A notice is issued in this case.

name
:   The name \(optionally schema-qualified\) of an existing domain.

CASCADE
:   Automatically drop objects that depend on the domain \(such as table columns\), and in turn all objects that depend on those objects.

RESTRICT
:   Refuse to drop the domain if any objects depend on it. This is the default.

## <a id="section5"></a>Examples 

Remove the domain named `us_postal_code`:

```
DROP DOMAIN us_postal_code;
```

## <a id="section6"></a>Compatibility 

This command conforms to the SQL standard, except for the `IF EXISTS` option, which is a Greenplum Database extension.

## <a id="section7"></a>See Also 

[ALTER DOMAIN](ALTER_DOMAIN.html), [CREATE DOMAIN](CREATE_DOMAIN.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

