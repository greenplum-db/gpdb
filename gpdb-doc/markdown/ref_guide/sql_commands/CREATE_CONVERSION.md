# CREATE CONVERSION 

Defines a new encoding conversion.

## Synopsis 

``` {#sql_command_synopsis}
CREATE [DEFAULT] CONVERSION <name> FOR <source_encoding> TO 
     <dest_encoding> FROM <funcname>
```

## Description 

`CREATE CONVERSION` defines a new conversion between character set encodings. Conversion names may be used in the convert function to specify a particular encoding conversion. Also, conversions that are marked `DEFAULT` can be used for automatic encoding conversion between client and server. For this purpose, two conversions, from encoding A to B and from encoding B to A, must be defined.

To create a conversion, you must have `EXECUTE` privilege on the function and `CREATE` privilege on the destination schema.

## Parameters 

DEFAULT
:   Indicates that this conversion is the default for this particular source to destination encoding. There should be only one default encoding in a schema for the encoding pair.

name
:   The name of the conversion. The conversion name may be schema-qualified. If it is not, the conversion is defined in the current schema. The conversion name must be unique within a schema.

source\_encoding
:   The source encoding name.

dest\_encoding
:   The destination encoding name.

funcname
:   The function used to perform the conversion. The function name may be schema-qualified. If it is not, the function will be looked up in the path. The function must have the following signature:

:   ```
conv_proc(
    integer,  -- source encoding ID
    integer,  -- destination encoding ID
    cstring,  -- source string (null terminated C string)
    internal, -- destination (fill with a null terminated C string)
    integer   -- source string length
) RETURNS void;
```

## Notes 

Note that in this release of Greenplum Database, user-defined functions used in a user-defined conversion must be defined as `IMMUTABLE`. Any compiled code \(shared library files\) for custom functions must be placed in the same location on every host in your Greenplum Database array \(master and all segments\). This location must also be in the `LD_LIBRARY_PATH` so that the server can locate the files.

## Examples 

To create a conversion from encoding `UTF8` to `LATIN1` using `myfunc`:

```
CREATE CONVERSION myconv FOR 'UTF8' TO 'LATIN1' FROM myfunc;
```

## Compatibility 

There is no `CREATE CONVERSION` statement in the SQL standard.

## See Also 

[ALTER CONVERSION](ALTER_CONVERSION.html), [CREATE FUNCTION](CREATE_FUNCTION.html), [DROP CONVERSION](DROP_CONVERSION.html)

**Parent topic:** [SQL Command Reference](../sql_commands/sql_ref.html)

