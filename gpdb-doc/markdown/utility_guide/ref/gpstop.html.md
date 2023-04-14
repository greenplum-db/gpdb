# gpstop 

Stops or restarts a Greenplum Database system.

## <a id="section2"></a>Synopsis 

```
gpstop [-d <coordinator_data_directory>] [-B parallel_processes>] 
       [-M smart | fast | immediate] [-t <timeout_seconds>] [-r] [-y] [-a] 
       [-l <logfile_directory>] [-v | -q]

gpstop -m [-d <coordinator_data_directory>] [-y] [-l <logfile_directory>] [-v | -q]

gpstop -u [-d <coordinator_data_directory>] [-l <logfile_directory>] [-v | -q]
 
gpstop --host <host_namea> [-d <coordinator_data_directory>] [-l <logfile_directory>]
       [-t <timeout_seconds>] [-a] [-v | -q]

gpstop --version 

gpstop -? | -h | --help
```

## <a id="section3"></a>Description 

The `gpstop` utility is used to stop the database servers that comprise a Greenplum Database system. When you stop a Greenplum Database system, you are actually stopping several `postgres` database server processes at once \(the coordinator and all of the segment instances\). The `gpstop` utility handles the shutdown of the individual instances. Each instance is shutdown in parallel.

The default shutdown mode \(`-M smart`\) waits for current client connections to finish before completing the shutdown. If any connections remain open after the timeout period, or if you interrupt with CTRL-C, `gpstop` lists the open connections and prompts whether to continue waiting for connections to finish, or to perform a fast or immediate shutdown. The default timeout period is 120 seconds and can be changed with the `-t timeout\_seconds` option.

Specify the `-M fast` shutdown mode to roll back all in-progress transactions and terminate any connections before shutting down.

With the `-u` option, the utility uploads changes made to the coordinator `pg_hba.conf` file or to *runtime* configuration parameters in the coordinator `postgresql.conf` file without interruption of service. Note that any active sessions will not pick up the changes until they reconnect to the database.

## <a id="section4"></a>Options 

-a
:   Do not prompt the user for confirmation.

-B parallel\_processes
:   The number of segments to stop in parallel. If not specified, the utility will start up to 64 parallel processes depending on how many segment instances it needs to stop.

-d coordinator\_data\_directory
:   Optional. The coordinator host data directory. If not specified, the value set for `$COORDINATOR_DATA_DIRECTORY` will be used.

--host host\_name
:   The utility shuts down the Greenplum Database segment instances on the specified host to allow maintenance on the host. Each primary segment instance on the host is shut down and the associated mirror segment instance is promoted to a primary segment if the mirror segment is on another host. Mirror segment instances on the host are shut down.

:   The segment instances are not shut down and the utility returns an error in these cases:

    -   Segment mirroring is not enabled for the system.
    -   The coordinator or standby coordinator is on the host.
    -   Both a primary segment instance and its mirror are on the host.

:   This option cannot be specified with the `-m`, `-r`, `-u`, or `-y` options.

    > **Note** The `gprecoverseg` utility restores segment instances. Run `gprecoverseg` commands to start the segments as mirrors and then to return the segments to their preferred role \(primary segments\).

-l logfile\_directory
:   The directory to write the log file. Defaults to `~/gpAdminLogs`.

-m
:   Optional. Shuts down a Greenplum coordinator instance that was started in maintenance mode.

-M fast
:   Fast shut down. Any transactions in progress are interrupted and rolled back.

-M immediate
:   Immediate shut down. Any transactions in progress are cancelled.

:   This mode kills all `postgres` processes without allowing the database server to complete transaction processing or clean up any temporary or in-process work files.

-M smart
:   Smart shut down. This is the default shutdown mode. `gpstop` waits for active user connections to disconnect and then proceeds with the shutdown. If any user connections remain open after the timeout period \(or if you interrupt by pressing CTRL-C\) `gpstop` lists the open user connections and prompts whether to continue waiting for connections to finish, or to perform a fast or immediate shutdown.

-q
:   Run in quiet mode. Command output is not displayed on the screen, but is still written to the log file.

-r
:   Restart after shutdown is complete.

-t timeout\_seconds
:   Specifies a timeout threshold \(in seconds\) to wait for a segment instance to shutdown. If a segment instance does not shutdown in the specified number of seconds, `gpstop` displays a message indicating that one or more segments are still in the process of shutting down and that you cannot restart Greenplum Database until the segment instance\(s\) are stopped. This option is useful in situations where `gpstop` is run and there are very large transactions that need to rollback. These large transactions can take over a minute to rollback and surpass the default timeout period of 120 seconds.

-u
:   This option reloads the `pg_hba.conf` files of the coordinator and segments and the runtime parameters of the `postgresql.conf` files but does not shutdown the Greenplum Database array. Use this option to make new configuration settings active after editing `postgresql.conf` or `pg_hba.conf`. Note that this only applies to configuration parameters that are designated as *runtime* parameters.

-v
:   Displays detailed status, progress and error messages output by the utility.

-y
:   Do not stop the standby coordinator process. The default is to stop the standby coordinator.

-? \| -h \| --help
:   Displays the online help.

--version
:   Displays the version of this utility.

## <a id="section5"></a>Examples 

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

Stop a coordinator instance that was started in maintenance mode:

```
gpstop -m
```

Reload the `postgresql.conf` and `pg_hba.conf` files after making configuration changes but do not shutdown the Greenplum Database array:

```
gpstop -u
```

## <a id="section6"></a>See Also 

[gpstart](gpstart.html)

