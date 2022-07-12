# gpfilespace 

Creates a filespace using a configuration file that defines per-segment file system locations. Filespaces describe the physical file system resources to be used by a tablespace.

**Note:** `gpfilespace` is deprecated and will be removed in the Greenplum Database 6 release.

## Synopsis 

```
gpfilespace [<connection_option> ...] [-l <logfile_directory>] 
              [-o <output_file_name>]

gpfilespace [<connection_option> ...] [-l <logfile_directory>] 
              [-c <fs_config_file>]

gpfilespace --movetempfilespace \{<filespace\_name\> \| default\}

gpfilespace --movetransfilespace \{<filespace\_name\> \| default\}

gpfilespace --showtempfilespace 

gpfilespace --showtransfilespace 

gpfilespace -v 

gpfilespace -?
```

## Description 

A tablespace requires a file system location to store its database files. In Greenplum Database, the master and each segment \(primary and mirror\) needs its own distinct storage location. This collection of file system locations for all components in a Greenplum system is referred to as a filespace. Once a filespace is defined, it can be used by one or more tablespaces.

When used with the `-o` option, the `gpfilespace` utility looks up your system configuration information in the Greenplum Database catalog tables and prompts you for the appropriate file system locations needed to create the filespace. It then outputs a configuration file that can be used to create a filespace. If a file name is not specified, a `gpfilespace_config_`\# file will be created in the current directory by default.

Once you have a configuration file, you can run `gpfilespace` with the `-c` option to create the filespace in Greenplum Database.

You will need to create a filespace before you can use the `gpfilespace --movetempfilespace` option to move your temporary files to the new location.

Use either `gpfilespace --showtempfilespace` or `--showtransfilespace` options to show the name of the filespace currently associated with temporary or transaction files.

**Note:** If segments are down due to a power or NIC failure, you may see inconsistencies during filespace creation. You may not be able to bring up the Greenplum Database.

## Options 

-c \| --config fs\_config\_file
:   A configuration file containing:

    -   An initial line denoting the new filespace name. For example:

        filespace:myfs

    -   One line each for the master, the primary segments, and the mirror segments. A line describes a file system location that a particular segment database instance should use as its data directory location to store database files associated with a tablespace. Each line is in the format of:

        ```
        <hostname>:<dbid>:/<filesystem_dir>/<seg_datadir_name>
        ```


-l \| --logdir logfile\_directory
:   The directory to write the log file. Defaults to `~/gpAdminLogs`.

-o \| --output output\_file\_name
:   The directory location and file name to output the generated filespace configuration file. You will be prompted to enter a name for the filespace, a master file system location, the primary segment file system locations, and the mirror segment file system locations. For example, if your configuration has 2 primary and 2 mirror segments per host, you will be prompted for a total of 5 locations \(including the master\). The file system locations must exist on all hosts in your system prior to running the `gpfilespace` utility. The utility will designate segment-specific data directories within the location\(s\) you specify, so it is possible to use the same location for multiple segments. However, primaries and mirrors cannot use the same location. After the utility creates the configuration file, you can manually edit the file to make any required changes to the filespace layout before creating the filespace in Greenplum Database.

--movetempfilespace \{filespace\_name \| default\}
:   Moves temporary files to a new filespace or to the default location.

--movetransfilespace \{filespace\_name \| default\}
:   Moves transaction files to a new filespace or to the default location.

    **Warning:** Do not move transaction files to a non-default location. Moving transaction files might cause data loss.

--showtempfilespace
:   Show the name of the filespace currently associated with temporary files. This option checks that all primary and mirror segments, master and master standby are using the same filespace or temporary files. You will receive a warning message and an email if any inconsistencies exist.

--showtransfilespace
:   Show the name of the filespace currently associated with transaction files. This option checks that all primary and mirror segments, master and master standby are using the same filespace or transaction files. You will receive a warning message and an email if any inconsistencies exist.

-v \| --version \(show utility version\)
:   Displays the version of this utility.

-? \| --help \(help\)
:   Displays the utility usage and syntax.

**Connection Options**

-h host \| --host host
:   The host name of the machine on which the Greenplum master database server is running. If not specified, reads from the environment variable `PGHOST` or defaults to localhost.

-p port \| --port port
:   The TCP port on which the Greenplum master database server is listening for connections. If not specified, reads from the environment variable `PGPORT` or defaults to 5432.

-U username \| --username superuser\_name
:   The database superuser role name to connect as. If not specified, reads from the environment variable `PGUSER` or defaults to the current system user name. Only database superusers are allowed to create filespaces.

-W \| --password
:   Force a password prompt.

## Examples 

Create a filespace configuration file. You will be prompted to enter a name for the filespace, a master file system location, the primary segment file system locations, and the mirror segment file system locations. For example, if your configuration has 2 primary and 2 mirror segments per host, you will be prompted for a total of 5 locations \(including the master\). The file system locations must exist on all hosts in your system prior to running the `gpfilespace` utility:

```
$ **gpfilespace -o .
**Enter a name for this filespace
> fastdisk

Checking your configuration:

Your system has 2 hosts with 2 primary and 2 mirror segments 
per host.

Configuring hosts: [sdw1, sdw2]

Please specify 2 locations for the primary segments, one per line:
primary location 1> /gp_pri_filespc
primary location 2> /gp_pri_filespc

Please specify 2 locations for the mirror segments, one per line:
mirror location 1> /gp_mir_filespc
mirror location 2> /gp_mir_filespc

Enter a file system location for the master:
master location> /gp_master_filespc
```

Example filespace configuration file:

```
filespace:fastdisk
mdw:1:/gp_master_filespc/gp-1
sdw1:2:/gp_pri_filespc/gp0
sdw1:3:/gp_mir_filespc/gp1
sdw2:4:/gp_mir_filespc/gp0
sdw2:5:/gp_pri_filespc/gp1
```

Execute the configuration file to create the filespace in Greenplum Database:

```
$ gpfilespace -c gpfilespace_config_1
```

## See Also 

[CREATE TABLESPACE](/oss/ref_guide/sql_commands/CREATE_TABLESPACE.html) in the *Greenplum Database Reference Guide*

