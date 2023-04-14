# pg_stat_last_shoperation 

The pg\_stat\_last\_shoperation table contains metadata tracking information about global objects \(roles, tablespaces, etc.\).

|column|type|references|description|
|------|----|----------|-----------|
|classid|oid|pg\_class.oid|OID of the system catalog containing the object.|
|`objid`|oid|any OID column|OID of the object within its system catalog.|
|`staactionname`|name| |The action that was taken on the object.|
|`stasysid`|oid| | |
|`stausename`|name| |The name of the role that performed the operation on this object.|
|`stasubtype`|text| |The type of object operated on or the subclass of operation performed.|
|`statime`|timestamp with timezone| |The timestamp of the operation. This is the same timestamp that is written to the Greenplum Database server log files in case you need to look up more detailed information about the operation in the logs.|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

