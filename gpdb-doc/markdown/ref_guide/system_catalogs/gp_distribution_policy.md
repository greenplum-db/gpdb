# gp\_distribution\_policy 

The `gp_distribution_policy` table contains information about Greenplum Database tables and their policy for distributing table data across the segments. This table is populated only on the master. This table is not globally shared, meaning each database has its own copy of this table.

|column|type|references|description|
|------|----|----------|-----------|
|`localoid`|oid|pg\_class.oid|The table object identifier \(OID\).|
|`attrnums`|smallint\[\]|pg\_attribute.attnum|The column number\(s\) of the distribution column\(s\).|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

