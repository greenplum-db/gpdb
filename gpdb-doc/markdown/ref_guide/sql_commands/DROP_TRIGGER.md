# DROP TRIGGER 

Removes a trigger.

## Synopsis 

``` {#sql_command_synopsis}
DROP TRIGGER [IF EXISTS] <name> ON <table> [CASCADE | RESTRICT]
```

## Description 

`DROP TRIGGER` will remove an existing trigger definition. To execute this command, the current user must be the owner of the table for which the trigger is defined.

## Parameters 

IF EXISTS
:   Do not throw an error if the trigger does not exist. A notice is issued in this case.

name
:   The name of the trigger to remove.

table
:   The name \(optionally schema-qualified\) of the table for which the trigger is defined.

CASCADE
:   Automatically drop objects that depend on the trigger.

RESTRICT
:   Refuse to drop the trigger if any objects depend on it. This is the default.

## Examples 

Remove the trigger `sendmail` on table `expenses`;

```
DROP TRIGGER sendmail ON expenses;
```

## Compatibility 

The `DROP TRIGGER` statement in Greenplum Database is not compatible with the SQL standard. In the SQL standard, trigger names are not local to tables, so the command is simply `DROP TRIGGER name`.

## See Also 

[ALTER TRIGGER](ALTER_TRIGGER.html), [CREATE TRIGGER](CREATE_TRIGGER.html)

**Parent topic:** [SQL Command Reference](../sql_commands/sql_ref.html)

