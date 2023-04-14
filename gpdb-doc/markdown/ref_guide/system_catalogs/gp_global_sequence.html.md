# gp_global_sequence 

The `gp_global_sequence` table contains the log sequence number position in the transaction log, which is used by the file replication process to determine the file blocks to replicate from a primary to a mirror segment.

|column|type|references|description|
|------|----|----------|-----------|
|`sequence_num`|bigint| |Log sequence number position in the transaction log|

