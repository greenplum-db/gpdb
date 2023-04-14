# DROP VIEW 

Removes a view.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
DROP VIEW [IF EXISTS] <name> [, ...] [CASCADE | RESTRICT]
```

## <a id="section3"></a>Description 

`DROP VIEW` will remove an existing view. Only the owner of a view can remove it.

## <a id="section4"></a>Parameters 

IF EXISTS
:   Do not throw an error if the view does not exist. A notice is issued in this case.

name
:   The name \(optionally schema-qualified\) of the view to remove.

CASCADE
:   Automatically drop objects that depend on the view \(such as other views\).

RESTRICT
:   Refuse to drop the view if any objects depend on it. This is the default.

## <a id="section5"></a>Examples 

Remove the view `topten`:

```
DROP VIEW topten;
```

## <a id="section6"></a>Compatibility 

`DROP VIEW` is fully conforming with the SQL standard, except that the standard only allows one view to be dropped per command. Also, the `IF EXISTS` option is a Greenplum Database extension.

## <a id="section7"></a>See Also 

[CREATE VIEW](CREATE_VIEW.html), [ALTER VIEW](ALTER_VIEW.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

