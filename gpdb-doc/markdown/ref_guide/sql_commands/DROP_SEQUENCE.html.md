# DROP SEQUENCE 

Removes a sequence.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
DROP SEQUENCE [IF EXISTS] <name> [, ...] [CASCADE | RESTRICT]
```

## <a id="section3"></a>Description 

`DROP SEQUENCE` removes sequence number generators. You must own the sequence to drop it \(or be a superuser\).

## <a id="section4"></a>Parameters 

IF EXISTS
:   Do not throw an error if the sequence does not exist. Greenplum Database issues a notice in this case.

name
:   The name \(optionally schema-qualified\) of the sequence to remove.

CASCADE
:   Automatically drop objects that depend on the sequence, and in turn all objects that depend on those objects.

RESTRICT
:   Refuse to drop the sequence if any objects depend on it. This is the default.

## <a id="section5"></a>Examples 

Remove the sequence `myserial`:

```
DROP SEQUENCE myserial;
```

## <a id="section6"></a>Compatibility 

`DROP SEQUENCE` conforms to the SQL standard, except that the standard allows only one sequence to be dropped per command. Also, the `IF EXISTS` option is a Greenplum Database extension.

## <a id="section7"></a>See Also 

[ALTER SEQUENCE](ALTER_SEQUENCE.html), [CREATE SEQUENCE](CREATE_SEQUENCE.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

