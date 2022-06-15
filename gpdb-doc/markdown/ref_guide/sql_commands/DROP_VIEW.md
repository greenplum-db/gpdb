# DROP VIEW 

Removes a view.

## Synopsis 

``` {#sql_command_synopsis}
DROP VIEW [IF EXISTS] <name> [, ...] [CASCADE | RESTRICT]
```

## Description 

`DROP VIEW` will remove an existing view. Only the owner of a view can remove it.

## Parameters 

IF EXISTS
:   Do not throw an error if the view does not exist. A notice is issued in this case.

name
:   The name \(optionally schema-qualified\) of the view to remove.

CASCADE
:   Automatically drop objects that depend on the view \(such as other views\).

RESTRICT
:   Refuse to drop the view if any objects depend on it. This is the default.

## Examples 

Remove the view `topten`;

```
DROP VIEW topten;
```

## Compatibility 

`DROP VIEW` is fully conforming with the SQL standard, except that the standard only allows one view to be dropped per command. Also, the `IF EXISTS` option is a Greenplum Database extension.

## See Also 

[CREATE VIEW](CREATE_VIEW.html)

**Parent topic:** [SQL Command Reference](../sql_commands/sql_ref.html)

