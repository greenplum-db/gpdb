# pg_max_external_files 

The `pg_max_external_files` view shows the maximum number of external table files allowed per segment host when using the external table `file` protocol.

|column|type|references|description|
|------|----|----------|-----------|
|`hostname`|name| |The host name used to access a particular segment instance on a segment host.|
|`maxfiles`|bigint| |Number of primary segment instances on the host.|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

