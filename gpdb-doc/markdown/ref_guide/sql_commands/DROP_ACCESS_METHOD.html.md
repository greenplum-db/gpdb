# DROP ACCESS METHOD 

Removes an access method

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
DROP ACCESS METHOD [IF EXISTS] <name> [CASCADE | RESTRICT]
```

## <a id="section3"></a>Description 

`DROP ACCESS METHOD` removes an existing access method. Only superusers can drop access methods.

## <a id="section4"></a>Parameters 

IF EXISTS
:   Do not throw an error if the access method does not exist. Greenplum Database issues a notice in this case.

name
:   The name of an existing access method.

CASCADE
:   Automatically drop objects that depend on the access method \(such as operator classes, operator families, and indexes\), and in turn all objects that depend on those objects.

RESTRICT
:   Refuse to drop the access method if any objects depend on it. This is the default.

## <a id="section5"></a>Examples 

Drop the access method `heptree`;

``` sql
DROP ACCESS METHOD heptree;
```

## <a id="section6"></a>Compatibility 

`DROP ACCESS METHOD` is a Greenplum Database extension.

## <a id="section7"></a>See Also 

[CREATE ACCESS METHOD](CREATE_ACCESS_METHOD.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

