# gpexpand.status_detail 

The `gpexpand.status_detail` table contains information about the status of tables involved in a system expansion operation. You can query this table to determine the status of tables being expanded, or to view the start and end time for completed tables.

This table also stores related information about the table such as the oid and disk size. Overall status information for the expansion is stored in [gpexpand.status](gp_expansion_status.html).

In a normal expansion operation it is not necessary to modify the data stored in this table.

|column|type|references|description|
|------|----|----------|-----------|
|`dbname`|text| |Name of the database to which the table belongs.|
|`fq_name`|text| |Fully qualified name of the table.|
|`table_oid`|oid| |OID of the table.|
|`root_partition_oid`|oid| |For a partitioned table, the OID of the root partition. Otherwise, `None`.|
|`rank`|int| |Rank determines the order in which tables are expanded. The expansion utility will sort on rank and expand the lowest-ranking tables first.|
|`external_writable`|boolean| |Identifies whether or not the table is an external writable table. \(External writable tables require a different syntax to expand\).|
|`status`|text| |Status of expansion for this table. Valid values are:<br/><br/>NOT STARTED<br/><br/>IN PROGRESS<br/><br/>COMPLETED<br/><br/>NO LONGER EXISTS|
|`expansion_started`|timestamp without time zone| |Timestamp for the start of the expansion of this table. This field is only populated after a table is successfully expanded.|
|`expansion_finished`|timestamp without time zone| |Timestamp for the completion of expansion of this table.|
|`source_bytes`| | |The size of disk space associated with the source table. Due to table bloat in heap tables and differing numbers of segments after expansion, it is not expected that the final number of bytes will equal the source number. This information is tracked to help provide progress measurement to aid in duration estimation for the end-to-end expansion operation.|
|`rel_storage`|text|Storage type of a relation.|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

