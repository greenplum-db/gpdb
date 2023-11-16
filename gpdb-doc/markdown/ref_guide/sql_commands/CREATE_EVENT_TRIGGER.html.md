# CREATE EVENT TRIGGER 

Defines a new event trigger.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
CREATE EVENT TRIGGER <name>
    ON <event>
    [ WHEN <filter_variable> IN (<filter_value> [, ... ]) [ AND ... ] ]
    EXECUTE { FUNCTION | PROCEDURE } <function_name>()
```

## <a id="section3"></a>Description 

`CREATE EVENT TRIGGER` creates a new event trigger. Whenever the designated event occurs and the `WHEN` condition associated with the trigger, if any, is satisfied, Greenplum Database runs the trigger function. For a general introduction to event triggers, see [Using Event Triggers](../../admin_guide/event_triggers.html). The user who creates an event trigger becomes its owner.

## <a id="section4"></a>Parameters 

name
:   The name to assign the new trigger. This name must be unique in the database.

event
:   The name of the event that triggers a call to the given function. See [Using Event Triggers](../../admin_guide/event_triggers.html) for more information about event names.

filter_variable
:   The name of a variable used to filter events. This makes it possible to restrict the firing of the trigger to a subset of the cases in which it is supported. Currently the only supported filter_variable is `TAG`.

filter_value
:   A list of values for the associated filter_variable for which the trigger should fire. For `TAG`, this means a list of command tags (e.g., `'DROP FUNCTION'`).

function_name
:   A user-supplied function that is declared as taking no argument and returning type event_trigger.

:   In the syntax of `CREATE EVENT TRIGGER`, the keywords `FUNCTION` and `PROCEDURE` are equivalent, but the referenced function must always be a function, not a procedure. The use of the keyword `PROCEDURE` here is historical and deprecated.

## <a id="section5"></a>Notes

Only superusers can create event triggers.

## <a id="section7"></a>Compatibility 

There is no `CREATE EVENT TRIGGER` statement in the SQL standard.

## <a id="section8"></a>See Also 

[ALTER EVENT TRIGGER](ALTER_EVENT_TRIGGER.html), [DROP EVENT TRIGGER](DROP_EVENT_TRIGGER.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

