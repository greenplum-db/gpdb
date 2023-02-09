# pg_stat_activity 

The view `pg_stat_activity` shows one row per server process with details about the associated user session and query. The columns that report data on the current query are available unless the parameter `stats_command_string` has been turned off. Furthermore, these columns are only visible if the user examining the view is a superuser or the same as the user owning the process being reported on.

The maximum length of the query text string stored in the column `query` can be controlled with the server configuration parameter `track_activity_query_size`.

|column|type|references|description|
|------|----|----------|-----------|
|`datid`|oid|pg\_database.oid|Database OID|
|`datname`|name| |Database name|
|`pid`|integer| |Process ID of this backend|
|`sess_id`|integer| |Session ID|
|`usesysid`|oid|pg\_authid.oid|OID of the user logged into this backend|
|`usename`|name| |Name of the user logged into this backend|
|`application_name`|text| |Name of the application that is connected to this backend|
|`client_addr`|inet| |IP address of the client connected to this backend. If this field is null, it indicates either that the client is connected via a Unix socket on the server machine or that this is an internal process such as autovacuum.|
|`client_hostname`|text| |Host name of the connected client, as reported by a reverse DNS lookup of `client_addr`. This field will only be non-null for IP connections, and only when log\_hostname is enabled.|
|`client_port`|integer| |TCP port number that the client is using for communication with this backend, or -1 if a Unix socket is used|
|`backend_start`|timestamptz| |Time backend process was started|
|`xact_start`|timestamptz| |Transaction start time|
|`query_start`|timestamptz| |Time query began execution|
|`state_change`|timestampz| |Time when the `state` was last changed|
|`waiting`|boolean| |True if waiting on a lock, false if not waiting|
|`state`|text| |Current overall state of this backend. Possible values are:<br/><br/>-   `active`: The backend is running a query.<br/><br/>-   `idle`: The backend is waiting for a new client command.<br/><br/>-   `idle in transaction`: The backend is in a transaction, but is not currently running a query.<br/><br/>-   `idle in transaction (aborted)`: This state is similar to idle in transaction, except one of the statements in the transaction caused an error.<br/><br/>-   `fastpath function call`: The backend is running a fast-path function.<br/><br/>-   `disabled`: This state is reported if `track_activities` is deactivated in this backend.|
|`query`|text| |Text of this backend's most recent query. If `state` is active this field shows the currently running query. In all other states, it shows the last query that was run.|
|`waiting_reason`|text| |Reason the server process is waiting. The value can be: lock, replication, or resgroup|
|`rsgid`|oid|pg\_resgroup.oid|Resource group OID or `0`.<br/><br/>See [Note](#rsg_note).|
|`rsgname`|text|pg\_resgroup.rsgname|Resource group name or `unknown`.<br/><br/>See [Note](#rsg_note).|
|`rsgqueueduration`|interval| |For a queued query, the total time the query has been queued.|

> **Note**
> When resource groups are enabled. Only query dispatcher (QD) processes will have a `rsgid` and `rsgname`. Other server processes such as a query executer (QE) process or session connection processes will have a `rsgid` value of `0` and a `rsgname` value of `unknown`. QE processes are managed by the same resource group as the dispatching QD process.

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

