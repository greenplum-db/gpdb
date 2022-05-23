# pg_shdescription 

The `pg_shdescription` system catalog table stores optional descriptions \(comments\) for shared database objects. Descriptions can be manipulated with the `COMMENT` command and viewed with `psql`'s `\d` meta-commands. See also [pg\_description](pg_description.html), which performs a similar function for descriptions involving objects within a single database. Unlike most system catalogs, `pg_shdescription` is shared across all databases of a Greenplum system: there is only one copy of `pg_shdescription` per system, not one per database.

|column|type|references|description|
|------|----|----------|-----------|
|`objoid`|oid|any OID column|The OID of the object this description pertains to.|
|`classoid`|oid|pg\_class.oid|The OID of the system catalog this object appears in|
|`description`|text| |Arbitrary text that serves as the description of this object.|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

