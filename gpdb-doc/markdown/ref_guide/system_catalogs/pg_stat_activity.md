# pg\_stat\_activity 

The view `pg_stat_activity` shows one row per server process with details about the associated user session and query. The columns that report data on the current query are available unless the parameter `stats_command_string` has been turned off. Furthermore, these columns are only visible if the user examining the view is a superuser or the same as the user owning the process being reported on.

The maximum length of the query text string stored in the column `current_query` can be controlled with the server configuration parameter `pgstat_track_activity_query_size`.

|column|type|references|description|
|------|----|----------|-----------|
|`datid`|oid|pg\_database.oid|Database OID|
|`datname`|name| |Database name|
|`procpid`|integer| |Process ID of the server process|
|`sess_id`|integer| |Session ID|
|`usesysid`|oid|pg\_authid.oid|Role OID|
|`usename`|name| |Role name|
|`current_query`|text| |Current query that process is running|
|`waiting`|boolean| |True if waiting on a lock, false if not waiting|
|`query_start`|timestamptz| |Time query began execution|
|`backend_start`|timestamptz| |Time backend process was started|
|`client_addr`|inet| |Client address|
|`client_port`|integer| |Client port|
|`application_name`|text| |Client application name|
|`xact_start`|timestamptz| |Transaction start time|
|`waiting_reason`|text| |Reason the server process is waiting. The value can be:lock, replication, or resgroup<br/>|
|`rsgid`|oid|pg\_resgroup.oid|Resource group OID or `0`<br/>See [Note](#rsg_note).<br/>|
|`rsgname`|text|pg\_resgroup.rsgname|Resource group name or `unknown`.<br/>See [Note](#rsg_note).<br/>|
|`rsgqueueduration`|interval| |For a queued query, the total time the query has been queued.|

**Note:** When resource groups are enabled. Only query dispatcher \(QD\) processes will have a `rsgid` and `rsgname`. Other server processes such as a query executer \(QE\) process or session connection processes will have a `rsgid` value of `0` and a `rsgname` value of `unknown`. QE processes are managed by the same resource group as the dispatching QD process.

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

