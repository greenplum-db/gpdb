# DROP DOMAIN 

Removes a domain.

## Synopsis 

``` {#sql_command_synopsis}
DROP DOMAIN [IF EXISTS] <name> [, ...]  [CASCADE | RESTRICT]
```

## Description 

`DROP DOMAIN` removes a previously defined domain. You must be the owner of a domain to drop it.

## Parameters 

IF EXISTS
:   Do not throw an error if the domain does not exist. A notice is issued in this case.

name
:   The name \(optionally schema-qualified\) of an existing domain.

CASCADE
:   Automatically drop objects that depend on the domain \(such as table columns\).

RESTRICT
:   Refuse to drop the domain if any objects depend on it. This is the default.

## Examples 

Drop the domain named `zipcode`:

```
DROP DOMAIN zipcode;
```

## Compatibility 

This command conforms to the SQL standard, except for the `IF EXISTS` option, which is a Greenplum Database extension.

## See Also 

[ALTER DOMAIN](ALTER_DOMAIN.html), [CREATE DOMAIN](CREATE_DOMAIN.html)

**Parent topic:** [SQL Command Reference](../sql_commands/sql_ref.html)

