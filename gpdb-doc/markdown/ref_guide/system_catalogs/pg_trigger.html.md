# pg_trigger 

The `pg_trigger` system catalog table stores triggers on tables.

> **Note** Greenplum Database does not support triggers.

|column|type|references|description|
|------|----|----------|-----------|
|`tgrelid`|oid|*pg\_class.oid*<br/><br/>Note that Greenplum Database does not enforce referential integrity.|The table this trigger is on.|
|`tgname`|name| |Trigger name \(must be unique among triggers of same table\).|
|`tgfoid`|oid|*pg\_proc.oid*<br/><br/>Note that Greenplum Database does not enforce referential integrity.|The function to be called.|
|`tgtype`|int2| |Bit mask identifying trigger conditions.|
|`tgenabled`|boolean| |True if trigger is enabled.|
|`tgisinternal`|boolean| |True if trigger is internally generated \(usually, to enforce the constraint identified by tgconstraint\).|
|`tgconstrrelid`|oid|*pg\_class.oid*<br/><br/>Note that Greenplum Database does not enforce referential integrity.|The table referenced by an referential integrity constraint.|
|`tgconstrindid`|oid|*pg\_class.oid*|The index supporting a unique, primary key, or referential integrity constraint.|
|`tgconstraint`|oid|*pg\_constraint.oid*|The `pg_constraint` entry associated with the trigger, if any.|
|`tgdeferrable`|boolean| |True if deferrable.|
|`tginitdeferred`|boolean| |True if initially deferred.|
|`tgnargs`|int2| |Number of argument strings passed to trigger function.|
|`tgattr`|int2vector| |Currently not used.|
|`tgargs`|bytea| |Argument strings to pass to trigger, each NULL-terminated.|
|`tgqual`|pg\_node\_tree| |Expression tree \(in `nodeToString()` representation\) for the trigger's `WHEN` condition, or null if none.|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

