# gpactivatestandby 

Activates a standby coordinator host and makes it the active coordinator for the Greenplum Database system.

## <a id="section2"></a>Synopsis 

```
gpactivatestandby [-d <standby_coordinator_datadir>] [-f] [-a] [-q] 
    [-l <logfile_directory>]

gpactivatestandby -v 

gpactivatestandby -? | -h | --help
```

## <a id="section3"></a>Description 

The `gpactivatestandby` utility activates a backup, standby coordinator host and brings it into operation as the active coordinator instance for a Greenplum Database system. The activated standby coordinator effectively becomes the Greenplum Database coordinator, accepting client connections on the coordinator port.

When you initialize a standby coordinator, the default is to use the same port as the active coordinator. For information about the coordinator port for the standby coordinator, see [gpinitstandby](gpinitstandby.html).

You must run this utility from the coordinator host you are activating, not the failed coordinator host you are deactivating. Running this utility assumes you have a standby coordinator host configured for the system \(see [gpinitstandby](gpinitstandby.html)\).

The utility will perform the following steps:

-   Stops the synchronization process \(`walreceiver`\) on the standby coordinator
-   Updates the system catalog tables of the standby coordinator using the logs
-   Activates the standby coordinator to be the new active coordinator for the system
-   Restarts the Greenplum Database system with the new coordinator host

A backup, standby Greenplum coordinator host serves as a 'warm standby' in the event of the primary Greenplum coordinator host becoming non-operational. The standby coordinator is kept up to date by transaction log replication processes \(the `walsender` and `walreceiver`\), which run on the primary coordinator and standby coordinator hosts and keep the data between the primary and standby coordinator hosts synchronized.

If the primary coordinator fails, the log replication process is shutdown, and the standby coordinator can be activated in its place by using the `gpactivatestandby` utility. Upon activation of the standby coordinator, the replicated logs are used to reconstruct the state of the Greenplum coordinator host at the time of the last successfully committed transaction.

In order to use `gpactivatestandby` to activate a new primary coordinator host, the coordinator host that was previously serving as the primary coordinator cannot be running. The utility checks for a `postmaster.pid` file in the data directory of the deactivated coordinator host, and if it finds it there, it will assume the old coordinator host is still active. In some cases, you may need to remove the `postmaster.pid` file from the deactivated coordinator host data directory before running `gpactivatestandby` \(for example, if the deactivated coordinator host process was terminated unexpectedly\).

After activating a standby coordinator, run `ANALYZE` to update the database query statistics. For example:

```
psql <dbname> -c 'ANALYZE;'
```

After you activate the standby coordinator as the primary coordinator, the Greenplum Database system no longer has a standby coordinator configured. You might want to specify another host to be the new standby with the [gpinitstandby](gpinitstandby.html) utility.

## <a id="section4"></a>Options 

-a \(do not prompt\)
:   Do not prompt the user for confirmation.

-d standby\_coordinator\_datadir
:   The absolute path of the data directory for the coordinator host you are activating.

:   If this option is not specified, `gpactivatestandby` uses the value of the `COORDINATOR_DATA_DIRECTORY` environment variable setting on the coordinator host you are activating. If this option is specified, it overrides any setting of `COORDINATOR_DATA_DIRECTORY`.

:   If a directory cannot be determined, the utility returns an error.

-f \(force activation\)
:   Use this option to force activation of the backup coordinator host. Use this option only if you are sure that the standby and primary coordinator hosts are consistent.

-l logfile\_directory
:   The directory to write the log file. Defaults to `~/gpAdminLogs`.

-q \(no screen output\)
:   Run in quiet mode. Command output is not displayed on the screen, but is still written to the log file.

-v \(show utility version\)
:   Displays the version, status, last updated date, and check sum of this utility.

-? \| -h \| --help \(help\)
:   Displays the online help.

## <a id="section5"></a>Example 

Activate the standby coordinator host and make it the active coordinator instance for a Greenplum Database system \(run from backup coordinator host you are activating\):

```
gpactivatestandby -d /gpdata
```

## <a id="section6"></a>See Also 

[gpinitsystem](gpinitsystem.html), [gpinitstandby](gpinitstandby.html)

