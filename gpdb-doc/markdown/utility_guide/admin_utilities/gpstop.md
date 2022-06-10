# gpstop 

Stops or restarts a Greenplum Database system.

## Synopsis 

```
gpstop [-d <master_data_directory>] [-B <parallel_processes>] 
       [-M smart | fast | immediate] [-t <timeout_seconds>] [-r] [-y] [-a] 
       [-l <logfile_directory>] [-v | -q]

gpstop -m [-d <master_data_directory>] [-y] [-l <logfile_directory>] [-v | -q]

gpstop -u [-d <master_data_directory>] [-l <logfile_directory>] [-v | -q]
 
gpstop --host <host_name> [-d <master_data_directory>] [-l <logfile_directory>]
       [-t <timeout_seconds>] [-a] [-v | -q]

gpstop --version 

gpstop -? | -h | --help
```

## Description 

The `gpstop` utility is used to stop the database servers that comprise a Greenplum Database system. When you stop a Greenplum Database system, you are actually stopping several `postgres` database server processes at once \(the master and all of the segment instances\). The `gpstop` utility handles the shutdown of the individual instances. Each instance is shutdown in parallel.

By default, you are not allowed to shut down Greenplum Database if there are any client connections to the database. Use the `-M fast` option to roll back all in progress transactions and terminate any connections before shutting down. If there are any transactions in progress, the default behavior is to wait for them to commit before shutting down.

With the `-u` option, the utility uploads changes made to the master `pg_hba.conf` file or to *runtime* configuration parameters in the master `postgresql.conf` file without interruption of service. Note that any active sessions will not pickup the changes until they reconnect to the database.

## Options 

-a
:   Do not prompt the user for confirmation.

-B parallel\_processes
:   The number of segments to stop in parallel. If not specified, the utility will start up to 64 parallel processes depending on how many segment instances it needs to stop.

-d master\_data\_directory
:   Optional. The master host data directory. If not specified, the value set for `$MASTER_DATA_DIRECTORY` will be used.

--host host\_name
:   The utility shuts down the Greenplum Database segment instances on the specified host to allow maintenance on the host. Each primary segment instance on the host is shut down and the associated mirror segment instance is promoted to a primary segment if the mirror segment is on another host. Mirror segment instances on the host are shut down.

:   The segment instances are not shut down and the utility returns an error in these cases:

    -   Segment mirroring is not enabled for the system.
    -   The master or standby master is on the host.
    -   Both a primary segment instance and its mirror are on the host.

:   This option cannot be specified with the `-m`, `-r`, `-u`, or `-y` options.

    **Note:** The `gprecoverseg` utility restores segment instances. Run `gprecoverseg` commands to start the segments as mirrors and then to return the segments to their preferred role \(primary segments\).

-l logfile\_directory
:   The directory to write the log file. Defaults to `~/gpAdminLogs`.

-m
:   Optional. Shuts down a Greenplum master instance that was started in maintenance mode.

-M fast
:   Fast shut down. Any transactions in progress are interrupted and rolled back.

-M immediate
:   Immediate shut down. Any transactions in progress are aborted.

:   This mode kills all `postgres` processes without allowing the database server to complete transaction processing or clean up any temporary or in-process work files.

-M smart
:   Smart shut down. If there are active connections, this command fails with a warning. This is the default shutdown mode.

-q
:   Run in quiet mode. Command output is not displayed on the screen, but is still written to the log file.

-r
:   Restart after shutdown is complete.

-t timeout\_seconds
:   Specifies a timeout threshold \(in seconds\) to wait for a segment instance to shutdown. If a segment instance does not shutdown in the specified number of seconds, `gpstop` displays a message indicating that one or more segments are still in the process of shutting down and that you cannot restart Greenplum Database until the segment instance\(s\) are stopped. This option is useful in situations where `gpstop` is executed and there are very large transactions that need to rollback. These large transactions can take over a minute to rollback and surpass the default timeout period of 600 seconds.

-u
:   This option reloads the `pg_hba.conf` files of the master and segments and the runtime parameters of the `postgresql.conf` files but does not shutdown the Greenplum Database array. Use this option to make new configuration settings active after editing `postgresql.conf` or `pg_hba.conf`. Note that this only applies to configuration parameters that are designated as *runtime* parameters.

-v
:   Displays detailed status, progress and error messages output by the utility.

-y
:   Do not stop the standby master process. The default is to stop the standby master.

-? \| -h \| --help
:   Displays the online help.

--version
:   Displays the version of this utility.

## Examples 

Stop a Greenplum Database system in smart mode:

```
gpstop
```

Stop a Greenplum Database system in fast mode:

```
gpstop -M fast
```

Stop all segment instances and then restart the system:

```
gpstop -r
```

Stop a master instance that was started in maintenance mode:

```
gpstop -m
```

Reload the `postgresql.conf` and `pg_hba.conf` files after making configuration changes but do not shutdown the Greenplum Database array:

```
gpstop -u
```

## See Also 

[gpstart](gpstart.html)

