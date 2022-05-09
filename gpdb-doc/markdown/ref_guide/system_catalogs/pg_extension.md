# pg_extension 

The system catalog table `pg_extension` stores information about installed extensions.

|column|type|references|description|
|------|----|----------|-----------|
|`extname`|name| |Name of the extension.|
|`extowner`|oid|pg\_authid.oid|Owner of the extension|
|`extnamespace`|oid|pg\_namespace.oid|Schema containing the extension exported objects.|
|`extrelocatable`|boolean| |True if the extension can be relocated to another schema.|
|`extversion`|text| |Version name for the extension.|
|`extconfig`|oid\[\]|pg\_class.oid|Array of `regclass` OIDs for the extension configuration tables, or `NULL` if none.|
|`extcondition`|text\[\]| |Array of `WHERE`-clause filter conditions for the extension configuration tables, or `NULL` if none.|

Unlike most catalogs with a namespace column, `extnamespace` does not imply that the extension belongs to that schema. Extension names are never schema-qualified. The `extnamespace` schema indicates the schema that contains most or all of the extension objects. If `extrelocatable` is `true`, then this schema must contain all schema-qualifiable objects that belong to the extension.

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

