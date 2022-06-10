# gpmovemirrors 

Moves mirror segment instances to new locations.

## Synopsis 

```
gpmovemirrors -i <move_config_file> [-d <master_data_directory>] 
          [-l <logfile_directory>] 
          [-B <parallel_processes>] [-v]

gpmovemirrors -? 

gpmovemirrors --version
```

## Description 

The `gpmovemirrors` utility moves mirror segment instances to new locations. You may want to move mirrors to new locations to optimize distribution or data storage.

Before moving segments, the utility verifies that they are mirrors, and that their corresponding primary segments are up and are in synchronizing or resynchronizing mode.

By default, the utility will prompt you for the file system location\(s\) where it will move the mirror segment data directories.

You must make sure that the user who runs `gpmovemirrors` \(the `gpadmin` user\) has permissions to write to the data directory locations specified. You may want to create these directories on the segment hosts and `chown` them to the appropriate user before running `gpmovemirrors`.

## Options 

-B parallel\_processes
:   The number of mirror segments to move in parallel. If not specified, the utility will start up to 4 parallel processes depending on how many mirror segment instances it needs to move.

-d master\_data\_directory
:   The master data directory. If not specified, the value set for `$MASTER_DATA_DIRECTORY` will be used.

-i move\_config\_file
:   A configuration file containing information about which mirror segments to move, and where to move them.

:   You must have one mirror segment listed for each primary segment in the system. Each line inside the configuration file has the following format \(as per attributes in the `gp_segment_configuration`, `pg_filespace`, and `pg_filespace_entry` catalog tables\):

:   ```
filespaceOrder=[<filespace1_fsname>[:<filespace2_fsname>:...]
<old_address>:<port>:fselocation \ 
[new_address:<port>:replication_port:fselocation[:<fselocation>:...]]
```

:   Note that you only need to specify a name for `filespaceOrder` if your system has multiple filespaces configured. If your system does not have additional filespaces configured besides the default `pg_system` filespace, this file will only have one location \(for the default data directory filespace, `pg_system`\). `pg_system` does not need to be listed in the `filespaceOrder` line. It will always be the first fselocation listed after replication\_port.

-l logfile\_directory
:   The directory to write the log file. Defaults to `~/gpAdminLogs`.

-v \(verbose\)
:   Sets logging output to verbose.

--version \(show utility version\)
:   Displays the version of this utility.

-? \(help\)
:   Displays the online help.

## Examples 

Moves mirrors from an existing Greenplum Database system to a different set of hosts:

```
$ gpmovemirrors -i move_config_file
```

Where the `move_config_file` looks something like this \(if you do not have additional filespaces configured besides the default `pg_system` filespace\):

```
filespaceOrder=filespacea
sdw1-1:61001:/data/mirrors/database/dbfast22/gp1 
sdw2-1:61001:43001:/data/mirrors/database/dbfast222/gp1:
/data/mirrors/database/dbfast222fs1
```

