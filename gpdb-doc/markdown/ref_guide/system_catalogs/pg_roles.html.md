# pg_roles 

The view `pg_roles` provides access to information about database roles. This is simply a publicly readable view of [pg\_authid](pg_authid.html) that blanks out the password field. This view explicitly exposes the OID column of the underlying table, since that is needed to do joins to other catalogs.

|column|type|references|description|
|------|----|----------|-----------|
|`rolname`|name| |Role name|
|`rolsuper`|bool| |Role has superuser privileges|
|`rolinherit`|bool| |Role automatically inherits privileges of roles it is a member of|
|`rolcreaterole`|bool| |Role may create more roles|
|`rolcreatedb`|bool| |Role may create databases|
|`rolcatupdate`|bool| |Role may update system catalogs directly. \(Even a superuser may not do this unless this column is true.\)|
|`rolcanlogin`|bool| |Role may log in. That is, this role can be given as the initial session authorization identifier|
|`rolconnlimit`|int4| |For roles that can log in, this sets maximum number of concurrent connections this role can make. -1 means no limit|
|`rolpassword`|text| |Not the password \(always reads as \*\*\*\*\*\*\*\*\)|
|`rolvaliduntil`|timestamptz| |Password expiry time \(only used for password authentication\); NULL if no expiration|
|`rolconfig`|text\[\]| |Role-specific defaults for run-time configuration variables|
|`rolresqueue`|oid|pg\_resqueue.oid|Object ID of the resource queue this role is assigned to.|
|`oid`|oid|pg\_authid.oid|Object ID of role|
|`rolcreaterextgpfd`|bool| |Role may create readable external tables that use the gpfdist protocol.|
|`rolcreaterexthttp`|bool| |Role may create readable external tables that use the http protocol.|
|`rolcreatewextgpfd`|bool| |Role may create writable external tables that use the gpfdist protocol.|
|`rolresgroup`|oid|pg\_resgroup.oid|Object ID of the resource group to which this role is assigned.|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

