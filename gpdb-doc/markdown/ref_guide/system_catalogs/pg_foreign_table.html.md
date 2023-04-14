# pg_foreign_table 

The system catalog table `pg_foreign_table` contains auxiliary information about foreign tables. A foreign table is primarily represented by a `pg_class` entry, just like a regular table. Its `pg_foreign_table` entry contains the information that is pertinent only to foreign tables and not any other kind of relation.

|column|type|references|description|
|------|----|----------|-----------|
|`ftrelid`|oid|pg\_class.oid|OID of the `pg_class` entry for this foreign table.|
|`ftserver`|oid|pg\_foreign\_server.oid|OID of the foreign server for this foreign table.|
|`ftoptions`|text\[\]| |Foreign table options, as "keyword=value" strings.|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

