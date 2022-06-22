# pg\_trigger 

The `pg_trigger` system catalog table stores triggers on tables.

**Note:** Greenplum Database does not support triggers.

|column|type|references|description|
|------|----|----------|-----------|
|`tgrelid`|oid|*pg\_class.oid*<br/>Note that Greenplum Database does not enforce referential integrity.<br/>|The table this trigger is on.|
|`tgname`|name| |Trigger name \(must be unique among triggers of same table\).|
|`tgfoid`|oid|*pg\_proc.oid*<br/>Note that Greenplum Database does not enforce referential integrity.<br/>
|The function to be called.|
|`tgtype`|int2| |Bit mask identifying trigger conditions.|
|`tgenabled`|boolean| |True if trigger is enabled.|
|`tgisconstraint`|boolean| |True if trigger implements a referential integrity constraint.|
|`tgconstrname`|name| |Referential integrity constraint name.|
|`tgconstrrelid`|oid|*pg\_class.oid*<br/>Note that Greenplum Database does not enforce referential integrity.<br/>|The table referenced by an referential integrity constraint.|
|`tgdeferrable`|boolean| |True if deferrable.|
|`tginitdeferred`|boolean| |True if initially deferred.|
|`tgnargs`|int2| |Number of argument strings passed to trigger function.|
|`tgattr`|int2vector| |Currently not used.|
|`tgargs`|bytea| |Argument strings to pass to trigger, each NULL-terminated.|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

