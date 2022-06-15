# DROP SEQUENCE 

Removes a sequence.

## Synopsis 

``` {#sql_command_synopsis}
DROP SEQUENCE [IF EXISTS] <name> [, ...] [CASCADE | RESTRICT]
```

## Description 

`DROP SEQUENCE` removes a sequence generator table. You must own the sequence to drop it \(or be a superuser\).

## Parameters 

IF EXISTS
:   Do not throw an error if the sequence does not exist. A notice is issued in this case.

name
:   The name \(optionally schema-qualified\) of the sequence to remove.

CASCADE
:   Automatically drop objects that depend on the sequence.

RESTRICT
:   Refuse to drop the sequence if any objects depend on it. This is the default.

## Examples 

Remove the sequence `myserial`:

```
DROP SEQUENCE myserial;
```

## Compatibility 

`DROP SEQUENCE` is fully conforming with the SQL standard, except that the standard only allows one sequence to be dropped per command. Also, the `IF EXISTS` option is a Greenplum Database extension.

## See Also 

[ALTER SEQUENCE](ALTER_SEQUENCE.html), [CREATE SEQUENCE](CREATE_SEQUENCE.html)

**Parent topic:** [SQL Command Reference](../sql_commands/sql_ref.html)

