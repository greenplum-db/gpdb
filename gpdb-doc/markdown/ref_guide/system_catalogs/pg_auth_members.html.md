# pg_auth_members 

The `pg_auth_members` system catalog table shows the membership relations between roles. Any non-circular set of relationships is allowed. Because roles are system-wide, `pg_auth_members` is shared across all databases of a Greenplum Database system.

|column|type|references|description|
|------|----|----------|-----------|
|`roleid`|oid|pg\_authid.oid|ID of the parent-level \(group\) role|
|`member`|oid|pg\_authid.oid|ID of a member role|
|`grantor`|oid|pg\_authid.oid|ID of the role that granted this membership|
|`admin_option`|boolean| |True if role member may grant membership to others|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

