# DROP RULE 

Removes a rewrite rule.

## Synopsis 

``` {#sql_command_synopsis}
DROP RULE [IF EXISTS] <name> ON <relation> [CASCADE | RESTRICT]
```

## Description 

`DROP RULE` drops a rewrite rule from a table or view.

## Parameters 

IF EXISTS
:   Do not throw an error if the rule does not exist. A notice is issued in this case.

name
:   The name of the rule to remove.

relation
:   The name \(optionally schema-qualified\) of the table or view that the rule applies to.

CASCADE
:   Automatically drop objects that depend on the rule.

RESTRICT
:   Refuse to drop the rule if any objects depend on it. This is the default.

## Examples 

Remove the rewrite rule `sales_2006` on the table `sales`:

```
DROP RULE sales_2006 ON sales;
```

## Compatibility 

There is no `DROP RULE` statement in the SQL standard.

## See Also 

[CREATE RULE](CREATE_RULE.html)

**Parent topic:** [SQL Command Reference](../sql_commands/sql_ref.html)

