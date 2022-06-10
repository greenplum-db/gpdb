# gpexpand 

Expands an existing Greenplum Database across new hosts in the array.

## Synopsis 

```
gpexpand [{-f|--hosts-file} <hosts_file>]
        | {-i|--input} <input_file> [-B <batch_size>] [-V|--novacuum] 
        | [{-d | --duration} <hh:mm:ss> | {-e|--end} '<YYYY-MM-DD hh:mm:ss>'] 
          [-a|-analyze] 
          [-n  <parallel_processes>]
        | {-r|--rollback}
        | {-c|--clean}
        [-D <database_name>] [-v|--verbose] [-s|--silent]
        [{-t|--tardir} <directory >]
        [-S|--simple-progress ]

gpexpand -? | -h | --help 

gpexpand --version
```

## Prerequisites 

-   You are logged in as the Greenplum Database superuser \(`gpadmin`\).
-   The new segment hosts have been installed and configured as per the existing segment hosts. This involves:
    -   Configuring the hardware and OS
    -   Installing the Greenplum software
    -   Creating the `gpadmin` user account
    -   Exchanging SSH keys.
-   Enough disk space on your segment hosts to temporarily hold a copy of your largest table.
-   When redistributing data, Greenplum Database must be running in production mode. Greenplum Database cannot be restricted mode or in master mode. The `gpstart` options `-R` or `-m` cannot be specified to start Greenplum Database.

## Description 

The `gpexpand` utility performs system expansion in two phases: segment initialization and then table redistribution.

In the initialization phase, `gpexpand` runs with an input file that specifies data directories, dbid values, and other characteristics of the new segments. You can create the input file manually, or by following the prompts in an interactive interview.

If you choose to create the input file using the interactive interview, you can optionally specify a file containing a list of expansion hosts. If your platform or command shell limits the length of the list of hostnames that you can type when prompted in the interview, specifying the hosts with `-f` may be mandatory.

In addition to initializing the segments, the initialization phase performs these actions:

-   Creates an expansion schema to store the status of the expansion operation, including detailed status for tables.
-   Changes the distribution policy for all tables to `DISTRIBUTED RANDOMLY`. The original distribution policies are later restored in the redistribution phase.

**Note:** Data redistribution should be performed during low-use hours. Redistribution can divided into batches over an extended period.

To begin the redistribution phase, run `gpexpand` with either the `-d` \(duration\) or `-e` \(end time\) options, or with no options. If you specify an end time or duration, then the utility redistributes tables in the expansion schema until the specified end time or duration is reached. If you specify no options, then the utility redistribution phase continues until all tables in the expansion schema are reorganized. Each table is reorganized using `ALTER TABLE` commands to rebalance the tables across new segments, and to set tables to their original distribution policy. If `gpexpand` completes the reorganization of all tables, it displays a success message and ends.

**Note:** This utility uses secure shell \(SSH\) connections between systems to perform its tasks. In large Greenplum Database deployments, cloud deployments, or deployments with a large number of segments per host, this utility may exceed the host's maximum threshold for unauthenticated connections. Consider updating the SSH `MaxStartups` and `MaxSessions` configuration parameters to increase this threshold. For more information about SSH configuration options, refer to the SSH documentation for your Linux distribution.

## Options 

-a \| --analyze
:   Run `ANALYZE` to update the table statistics after expansion. The default is to not run `ANALYZE`.

-B batch\_size
:   Batch size of remote commands to send to a given host before making a one-second pause. Default is `16`. Valid values are 1-128.

:   The `gpexpand` utility issues a number of setup commands that may exceed the host's maximum threshold for unauthenticated connections as defined by `MaxStartups` in the SSH daemon configuration. The one-second pause allows authentications to be completed before `gpexpand` issues any more commands.

:   The default value does not normally need to be changed. However, it may be necessary to reduce the maximum number of commands if `gpexpand` fails with connection errors such as `'ssh_exchange_identification: Connection closed by remote host.'`

-c \| --clean
:   Remove the expansion schema.

-d \| --duration hh:mm:ss
:   Duration of the expansion session from beginning to end.

-D database\_name
:   Specifies the database in which to create the expansion schema and tables. If this option is not given, the setting for the environment variable `PGDATABASE` is used. The database templates template1 and template0 cannot be used.

-e \| --end 'YYYY-MM-DD hh:mm:ss'
:   Ending date and time for the expansion session.

-f \| --hosts-file filename
:   Specifies the name of a file that contains a list of new hosts for system expansion. Each line of the file must contain a single host name.

:   This file can contain hostnames with or without network interfaces specified. The `gpexpand` utility handles either case, adding interface numbers to end of the hostname if the original nodes are configured with multiple network interfaces.

    **Note:** The Greenplum Database segment host naming convention is `sdwN` where `sdw` is a prefix and `N` is an integer. For example, `sdw1`, `sdw2` and so on. For hosts with multiple interfaces, the convention is to append a dash \(`-`\) and number to the host name. For example, `sdw1-1` and `sdw1-2` are the two interface names for host `sdw1`.

-i \| --input input\_file
:   Specifies the name of the expansion configuration file, which contains one line for each segment to be added in the format of:

:   hostname:address:port:fselocation:dbid:content:preferred\_role:replication\_port

:   If your system has filespaces, `gpexpand` will expect a filespace configuration file \(input\_file\_name`.fs`\) to exist in the same directory as your expansion configuration file. The filespace configuration file is in the format of:

:   ```
filespaceOrder=<filespace1_name>:<filespace2_name>: ...
dbid:</path/for/filespace1>:</path/for/filespace2>: ...
dbid:</path/for/filespace1>:</path/for/filespace2>: ...
...
```

-n parallel\_processes
:   The number of tables to redistribute simultaneously. Valid values are 1 - 96.

:   Each table redistribution process requires two database connections: one to alter the table, and another to update the table's status in the expansion schema. Before increasing `-n`, check the current value of the server configuration parameter `max_connections` and make sure the maximum connection limit is not exceeded.

-r \| --rollback
:   Roll back a failed expansion setup operation. If the rollback command fails, attempt again using the `-D` option to specify the database that contains the expansion schema for the operation that you want to roll back.

-s \| --silent
:   Runs in silent mode. Does not prompt for confirmation to proceed on warnings.

-S \| --simple-progress
:   If specified, the `gpexpand` utility records only the minimum progress information in the Greenplum Database table *gpexpand.expansion\_progress*. The utility does not record the relation size information and status information in the table *gpexpand.status\_detail*.

:   Specifying this option can improve performance by reducing the amount of progress information written to the `gpexpand` tables.

\[-t \| --tardir\] directory
:   The fully qualified path to a directory on segment hosts were the `gpexpand` utility copies a temporary tar file. The file contains Greenplum Database files that are used to create segment instances. The default directory is the user home directory.

-v \| --verbose
:   Verbose debugging output. With this option, the utility will output all DDL and DML used to expand the database.

--version
:   Display the utility's version number and exit.

-V \| --novacuum
:   Do not vacuum catalog tables before creating schema copy.

-? \| -h \| --help
:   Displays the online help.

## Examples 

Run `gpexpand` with an input file to initialize new segments and create the expansion schema in the default database:

```
$ gpexpand -i input_file
```

Run `gpexpand` for sixty hours maximum duration to redistribute tables to new segments:

```
$ gpexpand -d 60:00:00
```

## See Also 

[gpssh-exkeys](gpssh-exkeys.html), [Expanding a Greenplum System](../../admin_guide/expand/expand-main.html) in the *Greenplum Database Administrator Guide*

