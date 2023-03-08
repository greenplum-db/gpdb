# DROP MATERIALIZED VIEW 

Removes a materialized view.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
DROP MATERIALIZED VIEW [ IF EXISTS ] <name> [, ...] [ CASCADE | RESTRICT ]
```

## <a id="section3"></a>Description 

`DROP MATERIALIZED VIEW` drops an existing materialized view. To run this command, you must be the owner of the materialized view.

## <a id="section4"></a>Parameters 

IF EXISTS
:   Do not throw an error if the materialized view does not exist. A notice is issued in this case.

name
:   The name \(optionally schema-qualified\) of a materialized view to be dropped.

CASCADE
:   Automatically drop objects that depend on the materialized view \(such as other materialized views, or regular views\), and in turn all objects that depend on those objects.

RESTRICT
:   Refuse to drop the materialized view if any objects depend on it. This is the default.

## <a id="section6"></a>Examples 

This command removes the materialized view called `order_summary`.

```
DROP MATERIALIZED VIEW order_summary;
```

## <a id="section7"></a>Compatibility 

`DROP MATERIALIZED VIEW` is a Greenplum Database extension of the SQL standard.

## <a id="section8"></a>See Also 

[ALTER MATERIALIZED VIEW](ALTER_MATERIALIZED_VIEW.html), [CREATE MATERIALIZED VIEW](CREATE_MATERIALIZED_VIEW.html), [REFRESH MATERIALIZED VIEW](REFRESH_MATERIALIZED_VIEW.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

