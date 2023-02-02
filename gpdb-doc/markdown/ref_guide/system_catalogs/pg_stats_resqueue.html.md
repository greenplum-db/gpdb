# pg_stat_resqueues 

> **Note** The `pg_stat_resqueues` view is valid only when resource queue-based resource management is active.

The `pg_stat_resqueues` view allows administrators to view metrics about a resource queue's workload over time. To allow statistics to be collected for this view, you must enable the `stats_queue_level` server configuration parameter on the Greenplum Database coordinator instance. Enabling the collection of these metrics does incur a small performance penalty, as each statement submitted through a resource queue must be logged in the system catalog tables.

|column|type|references|description|
|------|----|----------|-----------|
|`queueid`|oid| |The OID of the resource queue.|
|`queuename`|name| |The name of the resource queue.|
|`n_queries_exec`|bigint| |Number of queries submitted for execution from this resource queue.|
|`n_queries_wait`|bigint| |Number of queries submitted to this resource queue that had to wait before they could run.|
|`elapsed_exec`|bigint| |Total elapsed execution time for statements submitted through this resource queue.|
|`elapsed_wait`|bigint| |Total elapsed time that statements submitted through this resource queue had to wait before they were run.|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

