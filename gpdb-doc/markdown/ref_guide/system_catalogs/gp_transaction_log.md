# gp\_transaction\_log 

The `gp_transaction_log` view contains status information about transactions local to a particular segment. This view allows you to see the status of local transactions.

|column|type|references|description|
|------|----|----------|-----------|
|`segment_id`|smallint|gp\_segment\_ configuration.content|The content id if the segment. The master is always -1 \(no content\).|
|`dbid`|smallint|gp\_segment\_ configuration.dbid|The unique id of the segment instance.|
|`transaction`|xid| |The local transaction ID.|
|`status`|text| |The status of the local transaction \(Committed or Aborted\).|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

