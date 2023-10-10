# pg_constraint 

The `pg_constraint` system catalog table stores check, primary key, unique, foreign key, and exclusion constraints on tables. \(Column constraints are not treated specially. Every column constraint is equivalent to some table constraint.\) Not-null constraints are represented in the [pg\_attribute](pg_attribute.html) catalog table. Check constraints on domains are stored here, too.

|column|type|references|description|
|------|----|----------|-----------|
|`oid`|oid| |Row identifier|
|`conname`|name| |Constraint name \(not necessarily unique!\)|
|`connamespace`|oid|[pg\_namespace](pg_namespace.html).oid|The object identifier of the namespace \(schema\) that contains this constraint.|
|`contype`|char| |`c` = check constraint, `f` = foreign key constraint, `p` = primary key constraint, `u` = unique constraint, `x` = exclusion constraint.|
|`condeferrable`|boolean| |Is the constraint deferrable?|
|`condeferred`|boolean| |Is the constraint deferred by default?|
|`convalidated`|boolean| |Has the constraint been validated? Currently, can only be false for foreign keys and `CHECK` constraints.|
|`conrelid`|oid|[pg\_class](pg_class.html).oid|The table this constraint is on; 0 if not a table constraint.|
|`contypid`|oid|[pg\_type](pg_type.html).oid|The domain this constraint is on; 0 if not a domain constraint.|
|`conindid`|oid|[pg\_class](pg_class.html).oid|The index supporting this constraint, if it's a unique, primary key, foreign key, or exclusion constraint; else 0|
|`conparentid`|oid|pg\_constraint.oid|The corresponding constraint in the parent partitioned table, if this is a constraint in a partition; else 0|
|`confrelid`|oid|[pg\_class](pg_class.html).oid|If a foreign key, the referenced table; else 0.|
|`confupdtype`|char| |Foreign key update action code.|
|`confdeltype`|char| |Foreign key deletion action code.|
|`confmatchtype`|char| |Foreign key match type.|
|`conislocal`|boolean| |This constraint is defined locally for the relation. Note that a constraint can be locally defined and inherited simultaneously.|
|`coninhcount`|integer| |The number of direct inheritance ancestors this constraint has. A constraint with a nonzero number of ancestors cannot be dropped nor renamed.|
|`connoinherit`|boolean| |This constraint is defined locally for the relation. It is a non-inheritable constraint.|
|`conkey`|smallint[]|[pg\_attribute](pg_attribute.html).attnum|If a table constraint, list of columns which the constraint constrains.|
|`confkey`|smallint[]|[pg\_attribute](pg_attribute.html).attnum|If a foreign key, list of the referenced columns.|
|`conpfeqop`|oid\[]|[pg\_operator](pg_operator.html).oid|If a foreign key, list of the equality operators for PK = FK comparisons.|
|`conppeqop`|oid\[]|[pg\_operator](pg_operator.html).oid|If a foreign key, list of the equality operators for PK = PK comparisons.|
|`conffeqop`|oid\[]|[pg\_operator](pg_operator.html).oid|If a foreign key, list of the equality operators for FK = FK comparisons.|
|`conexclop`|oid\[]|[pg\_operator](pg_operator.html).oid|If an exclusion constraint, list of the per-column exclusion operators.|
|`conbin`|pg\_node\_tree| |If a check constraint, an internal representation of the expression. \(It is recommended to use `pg_get_constraintdef()` to extract the definition of a check constraint.\)|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

