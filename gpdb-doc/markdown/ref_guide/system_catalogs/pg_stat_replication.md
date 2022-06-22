# pg\_stat\_replication 

The `pg_stat_replication` view contains metadata of the `walsender` process that is used for Greenplum Database master mirroring.

|column|type|references|description|
|------|----|----------|-----------|
|`procpid`|integer| |Process ID of WAL sender backend process.|
|`usesysid`|integer| |User system ID that runs the WAL sender backend process|
|`usename`|name| |User name that runs WAL sender backend process.|
|`application_name`|oid| |Client application name.|
|`client_addr`|name| |Client IP address.|
|`client_port`|integer| |Client port number.|
|`backend_start`|timestamp| |Operation start timestamp.|
|`state`|text| |WAL sender state.<br/>The value can be:<br/>`startup`<br/>`backup`<br/>`catchup`<br/>`streaming`<br/>|
|`sent_location`|text| |WAL sender xlog record sent location.|
|`write_location`|text| |WAL receiver xlog record write location.|
|`flush_location`|text| |WAL receiver xlog record flush location.|
|`replay_location`|text| |Standby xlog record replay location.|
|`sync_priority`|text| |Priorty. the value is `1`.|
|`sync_state`|text| |WAL sender syncronization state. The value is `sync`.|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

