# pg_inherits 

The `pg_inherits` system catalog table records information about table inheritance hierarchies. There is one entry for each direct child table in the database. \(Indirect inheritance can be determined by following chains of entries.\) In Greenplum Database, inheritance relationships are created by both the `INHERITS` clause \(standalone inheritance\) and the `PARTITION BY` clause \(partitioned child table inheritance\) of `CREATE TABLE`.

|column|type|references|description|
|------|----|----------|-----------|
|`inhrelid`|oid|pg\_class.oid|The OID of the child table.|
|`inhparent`|oid|pg\_class.oid|The OID of the parent table.|
|`inhseqno`|int4| |If there is more than one direct parent for a child table \(multiple inheritance\), this number tells the order in which the inherited columns are to be arranged. The count starts at 1.|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

