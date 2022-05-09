# gp_distributed_xacts 

The `gp_distributed_xacts` view contains information about Greenplum Database distributed transactions. A distributed transaction is a transaction that involves modifying data on the segment instances. Greenplum's distributed transaction manager ensures that the segments stay in synch. This view allows you to see the currently active sessions and their associated distributed transactions.

|column|type|references|description|
|------|----|----------|-----------|
|`distributed_xid`|xid| |The transaction ID used by the distributed transaction across the Greenplum Database array.|
|`distributed_id`|text| |The distributed transaction identifier. It has 2 parts — a unique timestamp and the distributed transaction number.|
|`state`|text| |The current state of this session with regards to distributed transactions.|
|`gp_session_id`|int| |The ID number of the Greenplum Database session associated with this transaction.|
|`xmin_distributed _snapshot`|xid| |The minimum distributed transaction number found among all open transactions when this transaction was started. It is used for MVCC distributed snapshot purposes.|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

