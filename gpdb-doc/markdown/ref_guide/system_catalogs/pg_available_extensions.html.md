# pg_available_extensions 

The `pg_available_extensions` view lists the extensions that are available for installation. The [*pg\_extension*](pg_extension.html) system catalog table shows the extensions currently installed.

The view is read only.

|column|type|description|
|------|----|-----------|
|`name`|name|Extension name.|
|`default_version`|text|Name of default version, or `NULL` if none is specified.|
|`installed_version`|text|Currently installed version of the extension, or `NULL` if not installed.|
|`comment`|text|Comment string from the extension control file.|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

