# gp\_segment\_configuration 

The `gp_segment_configuration` table contains information about mirroring and segment configuration.

|column|type|references|description|
|------|----|----------|-----------|
|`dbid`|smallint| |The unique identifier of a segment \(or master\) instance.|
|`content`|smallint| |The content identifier for a segment instance. A primary segment instance and its corresponding mirror will always have the same content identifier.<br/>For a segment the value is from 0-*N-1*, where *N* is the number of primary segments in the system.<br/>For the master, the value is always -1.|
|`role`|char| |The role that a segment is currently running as. Values are `p` \(primary\) or `m`\(mirror\).|
|`preferred_role`|char| |The role that a segment was originally assigned at initialization time. Values are `p` \(primary\) or `m`\(mirror\).|
|`mode`|char| |The synchronization status of a segment with its mirror copy. Values are `s` \(synchronized\), `c` \(change logging\), or `r` \(resyncing\).|
|`status`|char| |The fault status of a segment. Values are `u` \(up\) or `d` \(down\).|
|`port`|integer| |The TCP port the database server listener process is using.|
|`hostname`|text| |The hostname of a segment host.|
|`address`|text| |The hostname used to access a particular segment on a segment host. This value may be the same as `hostname` in systems upgraded from 3.x or on systems that do not have per-interface hostnames configured.|
|`replication_port`|integer| |The TCP port the file block replication process is using to keep primary and mirror segments synchronized.|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

