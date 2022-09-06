---
title: ALTER TRIGGER 
---

Changes the definition of a trigger.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
ALTER TRIGGER <name> ON <table> RENAME TO <newname>
```

## <a id="section3"></a>Description 

`ALTER TRIGGER` changes properties of an existing trigger. The `RENAME` clause changes the name of the given trigger without otherwise changing the trigger definition. You must own the table on which the trigger acts to be allowed to change its properties.

## <a id="section4"></a>Parameters 

name
:   The name of an existing trigger to alter.

table
:   The name of the table on which this trigger acts.

newname
:   The new name for the trigger.

## <a id="section5"></a>Notes 

The ability to temporarily activate or deactivate a trigger is provided by [ALTER TABLE](ALTER_TABLE.html), not by `ALTER TRIGGER`, because `ALTER TRIGGER` has no convenient way to express the option of activating or deactivating all of a table's triggers at once.

Note that Greenplum Database has limited support of triggers in this release. See [CREATE TRIGGER](CREATE_TRIGGER.html) for more information.

## <a id="section6"></a>Examples 

To rename an existing trigger:

```
ALTER TRIGGER emp_stamp ON emp RENAME TO emp_track_chgs;
```

## <a id="section7"></a>Compatibility 

`ALTER TRIGGER` is a Greenplum Database extension of the SQL standard.

## <a id="section8"></a>See Also 

[ALTER TABLE](ALTER_TABLE.html), [CREATE TRIGGER](CREATE_TRIGGER.html), [DROP TRIGGER](DROP_TRIGGER.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

