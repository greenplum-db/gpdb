# pg_enum 

The `pg_enum` table contains entries matching enum types to their associated values and labels. The internal representation of a given enum value is actually the OID of its associated row in `pg_enum`. The OIDs for a particular enum type are guaranteed to be ordered in the way the type should sort, but there is no guarantee about the ordering of OIDs of unrelated enum types.

|Column|Type|References|Description|
|------|----|----------|-----------|
|`enumtypid`|oid|pgtype.oid|The OID of the `pg_type` entry owning this enum value|
|`enumsortorder`|float4| |The sort position of this enum value within its enum type|
|`enumlabel`|name| |The textual label for this enum value|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

