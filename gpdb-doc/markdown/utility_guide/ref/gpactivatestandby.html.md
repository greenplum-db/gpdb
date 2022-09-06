# gpactivatestandby 

Activates a standby master host and makes it the active master for the Greenplum Database system.

## <a id="section2"></a>Synopsis 

```
gpactivatestandby [-d <standby_master_datadir>] [-f] [-a] [-q] 
    [-l <logfile_directory>]

gpactivatestandby -v 

gpactivatestandby -? | -h | --help
```

## <a id="section3"></a>Description 

The `gpactivatestandby` utility activates a backup, standby master host and brings it into operation as the active master instance for a Greenplum Database system. The activated standby master effectively becomes the Greenplum Database master, accepting client connections on the master port.

When you initialize a standby master, the default is to use the same port as the active master. For information about the master port for the standby master, see [gpinitstandby](gpinitstandby.html).

You must run this utility from the master host you are activating, not the failed master host you are deactivating. Running this utility assumes you have a standby master host configured for the system \(see [gpinitstandby](gpinitstandby.html)\).

The utility will perform the following steps:

-   Stops the synchronization process \(`walreceiver`\) on the standby master
-   Updates the system catalog tables of the standby master using the logs
-   Activates the standby master to be the new active master for the system
-   Restarts the Greenplum Database system with the new master host

A backup, standby Greenplum master host serves as a 'warm standby' in the event of the primary Greenplum master host becoming non-operational. The standby master is kept up to date by transaction log replication processes \(the `walsender` and `walreceiver`\), which run on the primary master and standby master hosts and keep the data between the primary and standby master hosts synchronized.

If the primary master fails, the log replication process is shutdown, and the standby master can be activated in its place by using the `gpactivatestandby` utility. Upon activation of the standby master, the replicated logs are used to reconstruct the state of the Greenplum master host at the time of the last successfully committed transaction.

In order to use `gpactivatestandby` to activate a new primary master host, the master host that was previously serving as the primary master cannot be running. The utility checks for a `postmaster.pid` file in the data directory of the deactivated master host, and if it finds it there, it will assume the old master host is still active. In some cases, you may need to remove the `postmaster.pid` file from the deactivated master host data directory before running `gpactivatestandby` \(for example, if the deactivated master host process was terminated unexpectedly\).

After activating a standby master, run `ANALYZE` to update the database query statistics. For example:

```
psql <dbname> -c 'ANALYZE;'
```

After you activate the standby master as the primary master, the Greenplum Database system no longer has a standby master configured. You might want to specify another host to be the new standby with the [gpinitstandby](gpinitstandby.html) utility.

## <a id="section4"></a>Options 

-a \(do not prompt\)
:   Do not prompt the user for confirmation.

-d standby\_master\_datadir
:   The absolute path of the data directory for the master host you are activating.

:   If this option is not specified, `gpactivatestandby` uses the value of the `MASTER_DATA_DIRECTORY` environment variable setting on the master host you are activating. If this option is specified, it overrides any setting of `MASTER_DATA_DIRECTORY`.

:   If a directory cannot be determined, the utility returns an error.

-f \(force activation\)
:   Use this option to force activation of the backup master host. Use this option only if you are sure that the standby and primary master hosts are consistent.

-l logfile\_directory
:   The directory to write the log file. Defaults to `~/gpAdminLogs`.

-q \(no screen output\)
:   Run in quiet mode. Command output is not displayed on the screen, but is still written to the log file.

-v \(show utility version\)
:   Displays the version, status, last updated date, and check sum of this utility.

-? \| -h \| --help \(help\)
:   Displays the online help.

## <a id="section5"></a>Example 

Activate the standby master host and make it the active master instance for a Greenplum Database system \(run from backup master host you are activating\):

```
gpactivatestandby -d /gpdata
```

## <a id="section6"></a>See Also 

[gpinitsystem](gpinitsystem.html), [gpinitstandby](gpinitstandby.html)

