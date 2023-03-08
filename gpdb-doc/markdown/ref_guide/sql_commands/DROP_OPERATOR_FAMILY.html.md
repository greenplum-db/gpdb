# DROP OPERATOR FAMILY 

Removes an operator family.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
DROP OPERATOR FAMILY [IF EXISTS] <name> USING <index_method> [CASCADE | RESTRICT]
```

## <a id="section3"></a>Description 

`DROP OPERATOR FAMILY` drops an existing operator family. To run this command you must be the owner of the operator family.

`DROP OPERATOR FAMILY` includes dropping any operator classes contained in the family, but it does not drop any of the operators or functions referenced by the family. If there are any indexes depending on operator classes within the family, you will need to specify `CASCADE` for the drop to complete.

## <a id="section4"></a>Parameters 

IF EXISTS
:   Do not throw an error if the operator family does not exist. A notice is issued in this case.

name
:   The name \(optionally schema-qualified\) of an existing operator family.

index\_method
:   The name of the index access method the operator family is for.

CASCADE
:   Automatically drop objects that depend on the operator family, and in turn all objects that depend on those objects.

RESTRICT
:   Refuse to drop the operator family if any objects depend on it. This is the default.

## <a id="section5"></a>Examples 

Remove the B-tree operator family `float_ops`:

```
DROP OPERATOR FAMILY float_ops USING btree;
```

This command will not succeed if there are any existing indexes that use the operator family. Add `CASCADE` to drop such indexes along with the operator family.

## <a id="section6"></a>Compatibility 

There is no `DROP OPERATOR FAMILY` statement in the SQL standard.

## <a id="section7"></a>See Also 

[ALTER OPERATOR FAMILY](ALTER_OPERATOR_FAMILY.html), [CREATE OPERATOR FAMILY](CREATE_OPERATOR_FAMILY.html), [ALTER OPERATOR CLASS](ALTER_OPERATOR_CLASS.html), [CREATE OPERATOR CLASS](CREATE_OPERATOR_CLASS.html), [DROP OPERATOR CLASS](DROP_OPERATOR_CLASS.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

