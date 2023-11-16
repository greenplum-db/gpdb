# DROP EVENT TRIGGER 

Removes an event trigger.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
DROP EVENT TRIGGER [IF EXISTS] <name> [CASCADE | RESTRICT]
```

## <a id="section3"></a>Description 

`DROP EVENT TRIGGER` removes an existing event trigger. To run this command, the current user must be the owner of the event trigger.

## <a id="section4"></a>Parameters 

IF EXISTS
:   Do not throw an error if the event trigger does not exist. Greenplum Database issues a notice in this case.

name
:   The name of the event trigger to remove.

CASCADE
:   Automatically drop objects that depend on the trigger, and in turn all objects that depend on those objects.

RESTRICT
:   Refuse to drop the trigger if any objects depend on it. This is the default.

## <a id="section5"></a>Examples 

Remove the event trigger `mtrig`;

```
DROP EVENT TRIGGER mtrig;
```

## <a id="section6"></a>Compatibility 

There is no `DROP EVENT TRIGGER` statement in the SQL standard.

## <a id="section7"></a>See Also 

[ALTER EVENT TRIGGER](ALTER_EVENT_TRIGGER.html), [CREATE EVENT TRIGGER](CREATE_EVENT_TRIGGER.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

