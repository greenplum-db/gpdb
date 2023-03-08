# DROP FOREIGN DATA WRAPPER 

Removes a foreign-data wrapper.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
DROP FOREIGN DATA WRAPPER [ IF EXISTS ] <name> [ CASCADE | RESTRICT ]
```

## <a id="section3"></a>Description 

`DROP FOREIGN DATA WRAPPER` removes an existing foreign-data wrapper from the current database. A foreign-data wrapper may be removed only by its owner.

## <a id="section4"></a>Parameters 

IF EXISTS
:   Do not throw an error if the foreign-data wrapper does not exist. Greenplum Database issues a notice in this case.

name
:   The name of an existing foreign-data wrapper.

CASCADE
:   Automatically drop objects that depend on the foreign-data wrapper \(such as foreign tables and servers\), and in turn all objects that depend on those objects.

RESTRICT
:   Refuse to drop the foreign-data wrapper if any object depends on it. This is the default.

## <a id="section6"></a>Examples 

Drop the foreign-data wrapper named `dbi`:

```
DROP FOREIGN DATA WRAPPER dbi;
```

## <a id="section7"></a>Compatibility 

`DROP FOREIGN DATA WRAPPER` conforms to ISO/IEC 9075-9 \(SQL/MED\). The `IF EXISTS` clause is a Greenplum Database extension.

## <a id="section8"></a>See Also 

[CREATE FOREIGN DATA WRAPPER](CREATE_FOREIGN_DATA_WRAPPER.html), [ALTER FOREIGN DATA WRAPPER](ALTER_FOREIGN_DATA_WRAPPER.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

