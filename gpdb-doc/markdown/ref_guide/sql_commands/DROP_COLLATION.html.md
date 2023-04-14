# DROP COLLATION 

Removes a previously defined collation.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
DROP COLLATION [ IF EXISTS ] <name> [ CASCADE | RESTRICT ]
```

## <a id="section3"></a>Description

`DROP COLLATION` removes a previously defined collation. To be able to drop a collation, you must own the collation.

## <a id="section4"></a>Parameters 

`IF EXISTS`
:   Do not throw an error if the collation does not exist. A notice is issued in this case.

name
:   The name of the collation. The collation name can be schema-qualified.

`CASCADE`
:   Automatically drop objects that depend on the collation, and in turn all objects that depend on those objects.

`RESTRICT`
:   Refuse to drop the collation if any objects depend on it. This is the default.


## <a id="section6"></a>Examples 

To drop the collation named `german`:

```
DROP COLLATION german;
```

## <a id="section7"></a>Compatibility 

The `DROP COLLATION` command conforms to the SQL standard, apart from the `IF EXISTS` option, which is a Greenplum Database extension.

## <a id="section8"></a>See Also 

[ALTER COLLATION](ALTER_COLLATION.html), [CREATE COLLATION](CREATE_COLLATION.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

