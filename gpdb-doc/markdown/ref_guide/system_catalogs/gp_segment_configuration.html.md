---
title: gp\_segment\_configuration 
---

The `gp_segment_configuration` table contains information about mirroring and segment instance configuration.

|column|type|references|description|
|------|----|----------|-----------|
|`dbid`|smallint| |Unique identifier of a segment \(or master\) instance.|
|`content`|smallint| |The content identifier for a segment instance. A primary segment instance and its corresponding mirror will always have the same content identifier.For a segment the value is from 0 to *N*-1, where *N* is the number of primary segments in the system.

For the master, the value is always -1.

|
|`role`|char| |The role that a segment is currently running as. Values are `p` \(primary\) or `m`\(mirror\).|
|`preferred_role`|char| |The role that a segment was originally assigned at initialization time. Values are `p` \(primary\) or `m` \(mirror\).|
|`mode`|char| |The synchronization status of a segment instance with its mirror copy. Values are `s` \(Synchronized\) or `n` \(Not In Sync\). **Note:** This column always shows `n` for the master segment and `s` for the standby master segment, but these values do not describe the synchronization state for the master segment. Use [gp\_stat\_replication](gp_stat_replication.html) to determine the synchronization state between the master and standby master.

|
|`status`|char| |The fault status of a segment instance. Values are `u` \(up\) or `d` \(down\).|
|`port`|integer| |The TCP port the database server listener process is using.|
|`hostname`|text| |The hostname of a segment host.|
|`address`|text| |The hostname used to access a particular segment instance on a segment host. This value may be the same as `hostname` on systems that do not have per-interface hostnames configured.|
|`datadir`|text| |Segment instance data directory.|

**Parent topic:**[System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

