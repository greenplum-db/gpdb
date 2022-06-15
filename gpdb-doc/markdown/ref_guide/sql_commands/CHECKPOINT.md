# CHECKPOINT 

Forces a transaction log checkpoint.

## Synopsis 

``` {#sql_command_synopsis}
CHECKPOINT
```

## Description 

Write-Ahead Logging \(WAL\) puts a checkpoint in the transaction log every so often. The automatic checkpoint interval is set per Greenplum Database segment instance by the server configuration parameters checkpoint\_segments and checkpoint\_timeout. The `CHECKPOINT` command forces an immediate checkpoint when the command is issued, without waiting for a scheduled checkpoint.

A checkpoint is a point in the transaction log sequence at which all data files have been updated to reflect the information in the log. All data files will be flushed to disk.

Only superusers may call `CHECKPOINT`. The command is not intended for use during normal operation.

## Compatibility 

The `CHECKPOINT` command is a Greenplum Database language extension.

**Parent topic:** [SQL Command Reference](../sql_commands/sql_ref.html)

