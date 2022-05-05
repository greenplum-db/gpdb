---
title: gp\_stat\_replication 
---

The `gp_stat_replication` view contains replication statistics of the `walsender` process that is used for Greenplum Database Write-Ahead Logging \(WAL\) replication when master or segment mirroring is enabled.

|column|type|references|description|
|------|----|----------|-----------|
|`gp_segment_id`|integer| |Unique identifier of a segment \(or master\) instance.|
|`pid`|integer| |Process ID of the `walsender` backend process.|
|`usesysid`|oid| |User system ID that runs the `walsender` backend process.|
|`usename`|name| |User name that runs the `walsender` backend process.|
|`application_name`|text| |Client application name.|
|`client_addr`|inet| |Client IP address.|
|`client_hostname`|text| |Client host name.|
|`client_port`|integer| |Client port number.|
|`backend_start`|timestamp| |Operation start timestamp.|
|`backend_xmin`|xid| |The current backend's `xmin` horizon.|
|`state`|text| |`walsender` state. The value can be:`startup`

`backup`

`catchup`

`streaming`

|
|`sent_location`|text| |`walsender` xlog record sent location.|
|`write_location`|text| |`walreceiver` xlog record write location.|
|`flush_location`|text| |`walreceiver` xlog record flush location.|
|`replay_location`|text| |Master standby or segment mirror xlog record replay location.|
|`sync_priority`|integer| |Priority. The value is `1`.|
|`sync_state`|text| |`walsender`synchronization state. The value is `sync`.|
|`sync_error`|text| |`walsender` synchronization error. `none` if no error.|

**Parent topic: **[System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

