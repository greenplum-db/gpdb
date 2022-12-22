# gp_distributed_log 

The `gp_distributed_log` view contains status information about distributed transactions and their associated local transactions. A distributed transaction is a transaction that involves modifying data on the segment instances. Greenplum's distributed transaction manager ensures that the segments stay in synch. This view allows you to see the status of distributed transactions.

|column|type|references|description|
|------|----|----------|-----------|
|`segment_id`|smallint|gp\_segment\_configuration.content|The content id of the segment. The coordinator is always -1 \(no content\).|
|`dbid`|smallint|gp\_segment\_configuration.dbid|The unique id of the segment instance.|
|`distributed_xid`|xid| |The global transaction id.|
|`distributed_id`|text| |A system assigned ID for a distributed transaction.|
|`status`|text| |The status of the distributed transaction \(Committed or Aborted\).|
|`local_transaction`|xid| |The local transaction ID.|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

