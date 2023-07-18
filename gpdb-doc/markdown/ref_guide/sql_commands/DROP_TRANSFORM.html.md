# DROP TRANSFORM

Removes a transform.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
DROP TRANSFORM [IF EXISTS] FOR <type_name> LANGUAGE <lang_name> [CASCADE | RESTRICT]
```

## <a id="section3"></a>Description 

`DROP TRANSFORM` removes a previously defined transform.

To drop a transform, you must own the type and the language. These are the same privileges that are required to create a transform.

## <a id="section4"></a>Parameters 

IF EXISTS
:   Do not throw an error if the transform does not exist. Greenplum Database issues a notice in this case.

type\_name
:   The name of the data type of the transform.

lang\_name
:   The name of the language of the transform.

CASCADE
:   Automatically drop objects that depend on the transform, and in turn all objects that depend on those objects.

RESTRICT
:   Refuse to drop the transform if any objects depend on it. This is the default.

## <a id="section5"></a>Examples 

To drop the transform for type `hstore` and language `plpython3u`:

``` sql
DROP TRANSFORM FOR hstore LANGUAGE plpython3u;
```

## <a id="section6"></a>Compatibility 

This form of `DROP TRANSFORM` is a Greenplum Database extension. See [CREATE TRANSFORM](CREATE_TRANSFORM.html) for details.

## <a id="section7"></a>See Also 

[CREATE TRANSFORM](CREATE_TRANSFORM.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

