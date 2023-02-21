# pg_stat_replication 

The `pg_stat_replication` view contains metadata of the `walsender` process that is used for Greenplum Database coordinator mirroring.

The [gp\_stat\_replication](gp_stat_replication.html) view contains `walsender` replication information for coordinator and segment mirroring.

|column|type|references|description|
|------|----|----------|-----------|
|`pid`|integer| |Process ID of WAL sender backend process.|
|`usesysid`|integer| |User system ID that runs the WAL sender backend process|
|`usename`|name| |User name that runs WAL sender backend process.|
|`application_name`|oid| |Client application name.|
|`client_addr`|name| |Client IP address.|
|`client_hostname`|text| |The host name of the client machine.|
|`client_port`|integer| |Client port number.|
|`backend_start`|timestamp| |Operation start timestamp.|
|`backend_xmin`|xid| |The current backend's `xmin` horizon.|
|`state`|text| |WAL sender state. The value can be:<br/><br/>`startup`<br/><br/>`backup`<br/><br/>`catchup`<br/><br/>`streaming`|
|`sent_location`|text| |WAL sender xlog record sent location.|
|`write_location`|text| |WAL receiver xlog record write location.|
|`flush_location`|text| |WAL receiver xlog record flush location.|
|`replay_location`|text| |Standby xlog record replay location.|
|`sync_priority`|text| |Priority. the value is `1`.|
|`sync_state`|text| |WAL sender synchronization state. The value is `sync`.|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

