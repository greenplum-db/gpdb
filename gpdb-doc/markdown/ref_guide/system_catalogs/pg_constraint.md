# pg\_constraint 

The `pg_constraint` system catalog table stores check, primary key, unique, and foreign key constraints on tables. Column constraints are not treated specially. Every column constraint is equivalent to some table constraint. Not-null constraints are represented in the [pg\_attribute](pg_attribute.html) catalog table. Check constraints on domains are stored here, too.

|column|type|references|description|
|------|----|----------|-----------|
|`conname`|name| |Constraint name \(not necessarily unique!\)|
|`connamespace`|oid|pg\_namespace.oid|The OID of the namespace \(schema\) that contains this constraint.|
|`contype`|char| |`c` = check constraint, `f` = foreign key constraint, `p` = primary key constraint, `u` = unique constraint.|
|`condeferrable`|boolean| |Is the constraint deferrable?|
|`condeferred`|boolean| |Is the constraint deferred by default?|
|`conrelid`|oid|pg\_class.oid|The table this constraint is on; 0 if not a table constraint.|
|`contypid`|oid|pg\_type.oid|The domain this constraint is on; 0 if not a domain constraint.|
|`confrelid`|oid|pg\_class.oid|If a foreign key, the referenced table; else 0.|
|`confupdtype`|char| |Foreign key update action code.|
|`confdeltype`|char| |Foreign key deletion action code.|
|`confmatchtype`|char| |Foreign key match type.|
|`conkey`|int2\[\]|pg\_attribute.attnum|If a table constraint, list of columns which the constraint constrains.|
|`confkey`|int2\[\]|pg\_attribute.attnum|If a foreign key, list of the referenced columns.|
|`conbin`|text| |If a check constraint, an internal representation of the expression.|
|`consrc`|text| |If a check constraint, a human-readable representation of the expression. This is not updated when referenced objects change; for example, it won't track renaming of columns. Rather than relying on this field, it is best to use `pg_get_constraintdef()` to extract the definition of a check constraint.|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

