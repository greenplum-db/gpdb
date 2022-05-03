---
title: ALTER RULE 
---

Changes the definition of a rule.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
ALTER RULE name ON table\_name RENAME TO new\_name
```

## <a id="section3"></a>Description 

`ALTER RULE` changes properties of an existing rule. Currently, the only available action is to change the rule's name.

To use `ALTER RULE`, you must own the table or view that the rule applies to.

## <a id="section4"></a>Parameters 

name
:   The name of an existing rule to alter.

table\_name
:   The name \(optionally schema-qualified\) of the table or view that the rule applies to.

new\_name
:   The new name for the rule.

## <a id="section7"></a>Compatibility 

`ALTER RULE` is a Greenplum Database language extension, as is the entire query rewrite system.

## <a id="seea"></a>See Also 

[CREATE RULE](CREATE_RULE.html), [DROP RULE](DROP_RULE.html)

