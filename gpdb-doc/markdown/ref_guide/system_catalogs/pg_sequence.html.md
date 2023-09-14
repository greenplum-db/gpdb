# pg_sequence 

The system catalog table `pg_sequence` contains information about sequences. Some information about sequences, such as the name and the schema, is stored in the [pg_class](pg_class.html) system table.

|column|type|references|description|
|------|----|----------|-----------|
|`seqrelid`|oid|`pg_class.oid` |OID of the `pg_class` entry for this sequence|
|`seqtypid`|oid|`pg_type.oid`|Data type of the sequence|
|`seqstart`|big int| |Start value of the sequence|
|`seqincrement`|big int| |Increment value of the sequence|
|`seqmax`|big int| |Maximum value of the sequence|
|`seqmin`|big int| |Minimum value of the sequence|
|`seqcache`|big int| |Cache size of the sequence|
|`seqcycle`|boolean| |Whether the sequence cycles|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

