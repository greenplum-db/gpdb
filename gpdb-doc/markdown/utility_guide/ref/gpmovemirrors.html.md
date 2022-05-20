# gpmovemirrors 

Moves mirror segment instances to new locations.

## <a id="section2"></a>Synopsis 

```
gpmovemirrors -i <move_config_file> [-d <coordinator_data_directory>] 
          [-l <logfile_directory>] [-b <segment_batch_size>]
          [-B <batch_size>] [-v] [--hba-hostnames <boolean>] 

gpmovemirrors -? 

gpmovemirrors --version
```

## <a id="section3"></a>Description 

The `gpmovemirrors` utility moves mirror segment instances to new locations. You may want to move mirrors to new locations to optimize distribution or data storage.

By default, the utility will prompt you for the file system location\(s\) where it will move the mirror segment data directories.

You must make sure that the user who runs `gpmovemirrors` \(the `gpadmin` user\) has permissions to write to the data directory locations specified. You may want to create these directories on the segment hosts and `chown` them to the appropriate user before running `gpmovemirrors`.

## <a id="section4"></a>Options 

-b segment\_batch\_size
:   The maximum number of segments per host to operate on in parallel. Valid values are `1` to `128`. If not specified, the utility will start recovering up to 64 segments in parallel on each host.

-B batch\_size
:   The number of hosts to work on in parallel. If not specified, the utility will start working on up to 16 hosts in parallel. Valid values are `1` to `64`.

-d coordinator\_data\_directory
:   The coordinator data directory. If not specified, the value set for `$COORDINATOR_DATA_DIRECTORY` will be used.

--hba-hostnames boolean
:   Optional. Controls whether this utility uses IP addresses or host names in the `pg_hba.conf` file when updating this file with addresses that can connect to Greenplum Database. When set to 0 -- the default value -- this utility uses IP addresses when updating this file. When set to 1, this utility uses host names when updating this file. For consistency, use the same value that was specified for `HBA_HOSTNAMES` when the Greenplum Database system was initialized. For information about how Greenplum Database resolves host names in the `pg_hba.conf` file, see [Configuring Client Authentication](../../admin_guide/client_auth.html).

-i move\_config\_file
:   A configuration file containing information about which mirror segments to move, and where to move them.

:   You must have one mirror segment listed for each primary segment in the system. Each line inside the configuration file has the following format \(as per attributes in the `gp_segment_configuration` catalog table\):

:   ```
<old_address>|<port>|<data_dir> <new_address>|<port>|<data_dir>
```

:   Where `<old_address>` and `<new_address>` are the host names or IP addresses of the segment hosts, `<port>` is the communication port, and `<data_dir>` is the segment instance data directory.

-l logfile\_directory
:   The directory to write the log file. Defaults to `~/gpAdminLogs`.

-v \(verbose\)
:   Sets logging output to verbose.

--version \(show utility version\)
:   Displays the version of this utility.

-? \(help\)
:   Displays the online help.

## <a id="section5"></a>Examples 

Moves mirrors from an existing Greenplum Database system to a different set of hosts:

```
$ gpmovemirrors -i move_config_file
```

Where the `move_config_file` looks something like this:

```
sdw2|50000|/data2/mirror/gpseg0 sdw3|50000|/data/mirror/gpseg0
sdw2|50001|/data2/mirror/gpseg1 sdw4|50001|/data/mirror/gpseg1
sdw3|50002|/data2/mirror/gpseg2 sdw1|50002|/data/mirror/gpseg2

```

