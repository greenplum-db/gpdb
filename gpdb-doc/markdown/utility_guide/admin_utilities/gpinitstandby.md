# gpinitstandby 

Adds and/or initializes a standby master host for a Greenplum Database system.

## Synopsis 

```
gpinitstandby { -s <standby_hostname> [-P port] 
      [-F <list_of_filespaces>] | -r | -n } 
      [-a] [-q] [-D] [-l <logfile_directory>]

gpinitstandby -v 

gpinitstandby -?
```

## Description 

The `gpinitstandby` utility adds a backup, standby master host to your Greenplum Database system. If your system has an existing standby master host configured, use the `-r` option to remove it before adding the new standby master host.

Before running this utility, make sure that the Greenplum Database software is installed on the standby master host and that you have exchanged SSH keys between the hosts. It is recommended that the master port is set to the same port number on the master host and the backup master host.

This utility should be run on the currently active *primary* master host.See the *Greenplum Database Installation Guide* for instructions.

The utility performs the following steps:

-   Updates the Greenplum Database system catalog to remove the existing standby master host information \(if the `-r` option is supplied\)
-   Updates the Greenplum Database system catalog to add the new standby master host information
-   Edits the `pg_hba.conf` file of the Greenplum Database master to allow access from the newly added standby master.
-   Sets up the standby master instance on the alternate master host
-   Starts the synchronization process

A backup, standby master host serves as a 'warm standby' in the event of the primary master host becoming non-operational. The standby master is kept up to date by transaction log replication processes \(the `walsender` and `walreceiver`\), which run on the primary master and standby master hosts and keep the data between the primary and standby master hosts synchronized. If the primary master fails, the log replication process is shut down, and the standby master can be activated in its place by using the `gpactivatestandby` utility. Upon activation of the standby master, the replicated logs are used to reconstruct the state of the master host at the time of the last successfully committed transaction.

The activated standby master effectively becomes the Greenplum Database master, accepting client connections on the master port and performing normal master operations such as SQL command processing and resource management.

**Important:** If the `gpinitstandby` utility previously failed to initialize the standby master, you must delete the files in the standby master data directory before running `gpinitstandby` again. The standby master data directory is not cleaned up after an initialization failure because it contains log files that can help in determining the reason for the failure.

If an initialization failure occurs, a summary report file is generated in the standby host directory `/tmp`. The report file lists the directories on standby host that require clean up.

## Options 

-a \(do not prompt\)
:   Do not prompt the user for confirmation.

-D \(debug\)
:   Sets logging level to debug.

-F list\_of\_filespaces
:   A list of filespace names and the associated locations. Each filespace name and its location is separated by a colon. If there is more than one file space name, each pair \(name and location\) is separated by a comma. For example:

:   ```
filespace1_name:fs1_location,filespace2_name:fs2_location
```

:   If this option is not specified, `gpinitstandby` prompts the user for the filespace names and locations.

:   If the list is not formatted correctly or number of filespaces do not match the number of filespaces already created in the system, `gpinitstandby` returns an error.

-l logfile\_directory
:   The directory to write the log file. Defaults to `~/gpAdminLogs`.

-n \(restart standby master\)
:   Specify this option to start a Greenplum Database standby master that has been configured but has stopped for some reason.

-P port
:   This option specifies the port that is used by the Greenplum Database standby master. The default is the same port used by the active Greenplum Database master.

:   If the Greenplum Database standby master is on the same host as the active master, the ports must be different. If the ports are the same for the active and standby master and the host is the same, the utility returns an error.

-q \(no screen output\)
:   Run in quiet mode. Command output is not displayed on the screen, but is still written to the log file.

-r \(remove standby master\)
:   Removes the currently configured standby master host from your Greenplum Database system.

-s standby\_hostname
:   The host name of the standby master host.

-v \(show utility version\)
:   Displays the version, status, last updated date, and check sum of this utility.

-? \(help\)
:   Displays the online help.

## Examples 

Add a standby master host to your Greenplum Database system and start the synchronization process:

```
gpinitstandby -s host09
```

Start an existing standby master host and synchronize the data with the current primary master host:

```
gpinitstandby -n
```

**Note:** Do not specify the -n and -s options in the same command.

Add a standby master host to your Greenplum Database system specifying a different port:

```
gpinitstandby -s myhost -P 2222
```

If you specify the same host name as the active Greenplum Database master, the installed Greenplum Database software that is used as a standby master must be in a separate location from the active Greenplum Database master. Also, filespace locations that are used by the standby master must be different than the filespace locations used by the active Greenplum Database master.

Remove the existing standby master from your Greenplum system configuration:

```
gpinitstandby -r
```

## See Also 

[gpinitsystem](gpinitsystem.html), [gpaddmirrors](gpaddmirrors.html), [gpactivatestandby](gpactivatestandby.html)

