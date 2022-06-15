# gpexpand.status\_detail 

The `gpexpand.status_detail` table contains information about the status of tables involved in a system expansion operation. You can query this table to determine the status of tables being expanded, or to view the start and end time for completed tables.

This table also stores related information about the table such as the oid, disk size, and normal distribution policy and key. Overall status information for the expansion is stored in [gpexpand.status](gp_expansion_status.html).

In a normal expansion operation it is not necessary to modify the data stored in this table. .

|column|type|references|description|
|------|----|----------|-----------|
|`dbname`|text| |Name of the database to which the table belongs.|
|`fq_name`|text| |Fully qualified name of the table.|
|`schema_oid`|oid| |OID for the schema of the database to which the table belongs.|
|`table_oid`|oid| |OID of the table.|
|`distribution_policy`|smallint\(\)| |Array of column IDs for the distribution key of the table.|
|`distribution_policy _names`|text| |Column names for the hash distribution key.|
|`distribution_policy _coloids`|text| |Column IDs for the distribution keys of the table.|
|`storage_options`|text| |Not enabled in this release. Do not update this field.|
|`rank`|int| |Rank determines the order in which tables are expanded. The expansion utility will sort on rank and expand the lowest-ranking tables first.|
|`status`|text| |Status of expansion for this table. Valid values are:NOT STARTED

IN PROGRESS

COMPLETED

NO LONGER EXISTS

|
|`last updated`|timestamp with time zone| |Timestamp of the last change in status for this table.|
|`expansion started`|timestamp with time zone| |Timestamp for the start of the expansion of this table. This field is only populated after a table is successfully expanded.|
|`expansion finished`|timestamp with time zone| |Timestamp for the completion of expansion of this table.|
|`source bytes`| | |The size of disk space associated with the source table. Due to table bloat in heap tables and differing numbers of segments after expansion, it is not expected that the final number of bytes will equal the source number. This information is tracked to help provide progress measurement to aid in duration estimation for the end-to-end expansion operation.|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

