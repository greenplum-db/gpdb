# ALTER EVENT TRIGGER 

Changes the definition of an event trigger.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
ALTER EVENT TRIGGER <name> DISABLE
ALTER EVENT TRIGGER <name> ENABLE [ REPLICA | ALWAYS ]
ALTER EVENT TRIGGER <name> OWNER TO { <new_owner> | CURRENT_USER | SESSION_USER }
ALTER EVENT TRIGGER <name> RENAME TO <new_name>
```

## <a id="section3"></a>Description 

`ALTER EVENT TRIGGER` changes properties of an existing event trigger.

You must be superuser to alter an event trigger.

## <a id="section4"></a>Parameters 

name
:   The name of an existing trigger to alter.

new_owner
:   The user name of the new owner of the event trigger.

new_name
:   The new name for the event trigger.

DISABLE/ENABLE [ REPLICA | ALWAYS ]
:   These forms configure the firing of event triggers. A deactivated trigger is still known to the system, but is not run when its triggering event occurs.

## <a id="section7"></a>Compatibility 

There is no `ALTER EVENT TRIGGER` statement in the SQL standard.

## <a id="section8"></a>See Also 

[CREATE EVENT TRIGGER](CREATE_EVENT_TRIGGER.html), [DROP EVENT TRIGGER](DROP_EVENT_TRIGGER.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

