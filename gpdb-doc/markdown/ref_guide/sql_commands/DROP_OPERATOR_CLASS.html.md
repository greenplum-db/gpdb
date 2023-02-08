# DROP OPERATOR CLASS 

Removes an operator class.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
DROP OPERATOR CLASS [IF EXISTS] <name> USING <index_method> [CASCADE | RESTRICT]
```

## <a id="section3"></a>Description 

`DROP OPERATOR` drops an existing operator class. To run this command you must be the owner of the operator class.

`DROP OPERATOR CLASS` does not drop any of the operators or functions referenced by the class. If there are any indexes depending on the operator class, you will need to specify `CASCADE` for the drop to complete.

## <a id="section4"></a>Parameters 

IF EXISTS
:   Do not throw an error if the operator class does not exist. A notice is issued in this case.

name
:   The name \(optionally schema-qualified\) of an existing operator class.

index\_method
:   The name of the index access method the operator class is for.

CASCADE
:   Automatically drop objects that depend on the operator class \(such as indexes\), and in turn all objects that depend on those objects.

RESTRICT
:   Refuse to drop the operator class if any objects depend on it. This is the default.

## <a id="section4"></a>Notes

`DROP OPERATOR CLASS` will not drop the operator family containing the class, even if there is nothing else left in the family \(in particular, in the case where the family was implicitly created by `CREATE OPERATOR CLASS`\). An empty operator family is harmless, but for the sake of tidiness you might wish to remove the family with `DROP OPERATOR FAMILY`; or perhaps better, use `DROP OPERATOR FAMILY` in the first place.

## <a id="section5"></a>Examples 

Remove the B-tree operator class `widget_ops`:

```
DROP OPERATOR CLASS widget_ops USING btree;
```

This command will not succeed if there are any existing indexes that use the operator class. Add `CASCADE` to drop such indexes along with the operator class.

## <a id="section6"></a>Compatibility 

There is no `DROP OPERATOR CLASS` statement in the SQL standard.

## <a id="section7"></a>See Also 

[ALTER OPERATOR CLASS](ALTER_OPERATOR_CLASS.html), [CREATE OPERATOR CLASS](CREATE_OPERATOR_CLASS.html), [DROP OPERATOR FAMILY](DROP_OPERATOR_FAMILY.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

