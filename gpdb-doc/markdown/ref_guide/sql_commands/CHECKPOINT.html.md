# CHECKPOINT 

Forces a transaction log checkpoint.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
CHECKPOINT
```

## <a id="section3"></a>Description 

A checkpoint is a point in the transaction log sequence at which all data files have been updated to reflect the information in the log. All data files will be flushed to disk.

The `CHECKPOINT` command forces an immediate checkpoint when the command is issued, without waiting for a regular checkpoint scheduled by the system. `CHECKPOINT` is not intended for use during normal operation.

If run during recovery, the `CHECKPOINT` command will force a restartpoint rather than writing a new checkpoint.

Only superusers may call `CHECKPOINT`.

## <a id="section4"></a>Compatibility 

The `CHECKPOINT` command is a Greenplum Database extension.

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

