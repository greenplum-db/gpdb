# gpmfr 

Manages the Greenplum Database backup images that are stored on a local Data Domain system and a remote Data Domain system that is used for disaster recovery. Managed file replication is used for disaster recovery by the Data Domain Boost software option to transfer a backup image from one Data Domain system to another.

## Synopsis 

```
gpmfr --delete {LATEST | OLDEST | <timestamp>}[--remote]
   [--master-port=<master_port>] [--skip-ping]
   [--ddboost-storage-unit=<unit-ID>]
   [-a] [-v | --verbose]

gpmfr {--replicate | --recover} {LATEST | OLDEST |  <timestamp>}
   --max-streams <max_IO_streams> [--master-port= <master_port>] [--skip-ping]
   [--ddboost-storage-unit=<unit-ID>]
   [-a] [-q | --quiet] [-v | --verbose]

gpmfr {--list {LATEST | OLDEST | <timestamp>} }
   [--ddboost-storage-unit=<unit-ID>]
   [--master-port=<master_port>] [--remote] [--skip-ping]
   [-v | --verbose]

gpmfr --list-files {LATEST | OLDEST | <timestamp>}
   [--ddboost-storage-unit=<unit-ID>]
   [--master-port=<master_port>] [--remote] [--skip-ping]
   [-v | --verbose]

gpmfr --show-streams [--skip-ping] [-v | --verbose]

gpmfr -h | --help

gpmfr --version
```

## Prerequisites 

The Data Domain systems that are used as local and remote backup systems for managed file replication must have Data Domain Boost and Replicator enabled.

The Greenplum Database master host must be able to connect to both the local Data Domain system and the remote Data Domain system.

The login credentials for the local and remote Data Domain systems must be configured on the Greenplum master host with the `gpcrondump` utility. See "Backing Up and Restoring Databases" in the *Greenplum Database Administrator Guide* for information about setting up Data Domain systems for use with Greenplum Database.

See the *Greenplum Database Release Notes* for information about the supported version of Data Domain Boost.

## Description 

The `gpmfr` utility provides these capabilities:

-   Lists the backup data sets that are on the local or the remote Data Domain system.
-   Replicates a backup data set that is on the local Data Domain system to the remote system.
-   Recovers a backup data set that is on the remote Data Domain system to the local system.
-   Deletes a backup data set that is on the local or the remote Data Domain system.

The Greenplum Database backup sets are identified by timestamps \(yyyymmddhhmmss\).

`gpmfr` attempts to schedule the replication task for the files in backup data set. It ensures that the limit on the maximum number of I/O streams used for replication is never exceeded. The I/O streams limit is set with the `--max-streams` option that accompanies the `--replicate` or `--recover` option.

When cancelling a replication operation, `gpmfr` kills all active replication processes and cleans up all the files on replication Data Domain system.

## Options 

-a
:   Do not prompt the user for confirmation. Progress information is displayed on the output. Specify the option `-q` or `--quiet` to write progress information to the log file.

--ddboost-storage-unit=unit-ID
:   Optional. Specify a valid storage unit ID for the Data Domain system that is used for the `gpmfr` operation. A replicate or recover operation uses the same storage unit ID on both local and remote Data Domain systems. If the storage unit on the destination Data Domain system \(where the backup is being copied\) is created if it does not exist.

:   If this option is not specified, the utility uses the storage unit specified when configuring the DD Boost credentials or the default ID `GPDB`.

--delete \{LATEST \| OLDEST \| timestamp\}
:   Deletes a Greenplum Database backup set from the local Data Domain system. Specify the option `--remote` to delete the backup set from the remote Data Domain system.

:   `LATEST` specifies deleting the latest backup set \(first in chronological order\).

:   `OLDEST` specifies deleting the backup set that is oldest in chronological order.

:   `timestamp` specifies deleting the Greenplum Database backup set identified by the `timestamp`.

--list
:   Lists the Greenplum Database backup sets that are on the local Data Domain system. The backup sets are identified by timestamps \(`yyyymmddhhmmss`\).

:   Specify the option `--remote` to list the Greenplum Database backup sets that are on the remote Data Domain system.

--list-files \{LATEST \| OLDEST \| timestamp\}
:   Lists the files in a Greenplum Database backup that is on the local Data Domain system. Specify the option `--remote` to list the files in the backup set that is on the remote Data Domain system.

:   `LATEST` specifies listing the files in the latest backup set \(first in chronological order\).

:   `OLDEST` specifies listing the files in the backup set that is oldest in chronological order.

:   `timestamp` specifies listing the file in the backup set identified by the `timestamp`.

--master-port=master\_port
:   Specifies the Greenplum Database master port number. To validate backup sets, the utility retrieves information from the Greenplum Database instance that uses the port number. If the option is not specified, the default value is 5432.

:   If `gpmfr` does not find a Greenplum Database, validation is skipped and a warning is displayed.

--max-streams max\_IO\_streams
:   Specifies the maximum number of Data Domain I/O streams that can be used when copying the backup set between the local and remote Data Domain systems.

-q \| --quiet
:   Runs in quiet mode. File transfer progress information is not displayed on the output, it is written to the log file. If this option is not specified, progress information is only displayed on screen, it is not written to the log file.

--recover \{LATEST \| OLDEST \| timestamp\}
:   Recovers a Greenplum Database backup set that is available on the remote Data Domain system to the local system.

:   `LATEST` specifies recovering the most recent backup set \(first in chronological order\).

:   `OLDEST` specifies recovering the backup set that is oldest in chronological order.

:   `timestamp` specifies recovering the backup set identified by the `timestamp`.

:   If a backup set with the same `timestamp` exists on local Data Domain system, the utility prompts you to confirm replacing the backup.

:   A progress bar indicating transfer status of the backup set is shown on shown at the output.

--replicate \{LATEST \| OLDEST \| timestamp\}
:   Replicates a Greenplum Database backup set that is on the local Data Domain system to the remote system.

:   `LATEST` specifies replicating the most recent backup set \(first in chronological order\).

:   `OLDEST` specifies replicating the backup set that is oldest in chronological order.

:   `timestamp` specifies replicating the backup set identified by the `timestamp`.

:   If a backup set with the same `timestamp` exists on remote Data Domain system, the utility prompts you to confirm replacing the backup.

:   A progress bar indicating transfer status of the backup set is shown at the output.

    A backup set must be completely backed up to the local Domain system before it can be replicated to the remote Data Domain system.

--remote
:   Perform the operation on the remote Data Domain system that is used for disaster recovery.

:   For example, `gpmfr --list` lists the backup sets that are on the local Data Domain system that is used to back up Greenplum Database. `gpmfr --list --remote` lists the backup sets that are on the remote system.

--show-streams
:   Displays the replication I/O stream soft limit and the number of I/O streams that are in use.

--skip-ping
:   Specify this option to skip the ping of a Data Domain system. `gpmfr` uses ping to ensure that the Data Domain system is reachable. If the Data Domain host is configured to block ICMP ping probes, specify this option to skip the ping of the Data Domain system.

-h \| --help
:   Displays the online help.

-v \| --verbose
:   Specifies verbose logging mode. Additional log information is written to the log file during command execution.

--version
:   Displays the version of this utility.

## Example 

The following example replicates the latest backup set on the local Data Domain sever to the remote server. The maximum number of I/O streams that can be used for the replication is 30.

```
gpmfr --replicate LATEST --max-streams 30
```

## See Also 

[gpcrondump](gpcrondump.html), [gpdbrestore](gpdbrestore.html)

