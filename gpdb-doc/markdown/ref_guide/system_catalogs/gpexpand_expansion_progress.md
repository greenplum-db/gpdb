# gpexpand.expansion\_progress 

The `gpexpand.expansion_progress`view contains information about the status of a system expansion operation. The view provides calculations of the estimated rate of table redistribution and estimated time to completion.

Status for specific tables involved in the expansion is stored in [gpexpand.status\_detail](gp_expansion_tables.html).

|column|type|references|description|
|------|----|----------|-----------|
|`name`|text| |Name for the data field provided. Includes:Bytes Left

Bytes Done

Estimated Expansion Rate

Estimated Time to Completion

Tables Expanded

Tables Left

|
|`value`|text| |The value for the progress data. For example:`Estimated Expansion Rate - 9.75667095996092 MB/s`

|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

