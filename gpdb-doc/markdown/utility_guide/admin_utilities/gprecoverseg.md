# gprecoverseg 

Recovers a primary or mirror segment instance that has been marked as down \(if mirroring is enabled\).

## Synopsis 

```
gprecoverseg [[-p <new_recover_host>[,...]] | -i <recover_config_file>] [-d <master_data_directory>] 
             [-B <parallel_processes>] [-F [-s]] [-a] [-q] 
	      [--hba-hostnames <boolean>] 
             [--no-progress] [-l <logfile_directory>]

gprecoverseg -r 

gprecoverseg -o <output_recover_config_file> | -S <output_filespace_config_file>
             [-p <new_recover_host>[,...]]

gprecoverseg -? 

gprecoverseg --version
```

## Description 

In a system with mirrors enabled, the `gprecoverseg` utility reactivates a failed segment instance and identifies the changed database files that require resynchronization. Once `gprecoverseg` completes this process, the system goes into *resynchronizing* mode until the recovered segment is brought up to date. The system is online and fully operational during resynchronization.

During an incremental recovery \(the `-F` option is not specified\), if `gprecoverseg` detects a segment instance with mirroring disabled in a system with mirrors enabled, the utility reports that mirroring is disabled for the segment, does not attempt to recover that segment instance, and continues the recovery process.

A segment instance can fail for several reasons, such as a host failure, network failure, or disk failure. When a segment instance fails, its status is marked as *down* in the Greenplum Database system catalog, and its mirror is activated in *change tracking* mode. In order to bring the failed segment instance back into operation again, you must first correct the problem that made it fail in the first place, and then recover the segment instance in Greenplum Database using `gprecoverseg`.

**Note:** If incremental recovery was not successful and the down segment instance data is not corrupted, contact VMware Support.

Segment recovery using `gprecoverseg` requires that you have an active mirror to recover from. For systems that do not have mirroring enabled, or in the event of a double fault \(a primary and mirror pair both down at the same time\) — you must take manual steps to recover the failed segment instances and then perform a system restart to bring the segments back online. For example, this command restarts a system.

```
gpstop -r
```

By default, a failed segment is recovered in place, meaning that the system brings the segment back online on the same host and data directory location on which it was originally configured. In this case, use the following format for the recovery configuration file \(using `-i`\).

```
<failed_host_address>|<port>|<data_directory> 
```

In some cases, this may not be possible \(for example, if a host was physically damaged and cannot be recovered\). In this situation, `gprecoverseg` allows you to recover failed segments to a completely new host \(using `-p`\), on an alternative data directory location on your remaining live segment hosts \(using `-s`\), or by supplying a recovery configuration file \(using `-i`\) in the following format. The word **SPACE** indicates the location of a required space. Do not add additional spaces.

```
<failed_host_address>|<port>|<data_directory>SPACE
<recovery_host_address>|<port>|<data_directory>

```

See the `-i` option below for details and examples of a recovery configuration file.

The `gp_segment_configuration` system catalog table can help you determine your current segment configuration so that you can plan your mirror recovery configuration. For example, run the following query:

```
=# SELECT dbid, content, address, port, datadir 
   FROM gp_segment_configuration
   ORDER BY dbid;
```

The new recovery segment host must be pre-installed with the Greenplum Database software and configured exactly the same as the existing segment hosts. A spare data directory location must exist on all currently configured segment hosts and have enough disk space to accommodate the failed segments.

The recovery process marks the segment as up again in the Greenplum Database system catalog, and then initiates the resynchronization process to bring the transactional state of the segment up-to-date with the latest changes. The system is online and available during resynchronization.

## Options 

-a \(do not prompt\)
:   Do not prompt the user for confirmation.

-B parallel\_processes
:   The number of segments to recover in parallel. If not specified, the utility will start up to 16 parallel processes depending on how many segment instances it needs to recover.

-d master\_data\_directory
:   Optional. The master host data directory. If not specified, the value set for `$MASTER_DATA_DIRECTORY` will be used.

-F \(full recovery\)
:   Optional. Perform a full copy of the active segment instance in order to recover the failed segment. The default is to only copy over the incremental changes that occurred while the segment was down.

    **Warning:** A full recovery deletes the data directory of the down segment instance before copying the data from the active \(current primary\) segment instance. Before performing a full recovery, ensure that the segment failure did not cause data corruption and that any host segment disk issues have been fixed.

    Also, for a full recovery, the utility does not restore custom files that are stored in the segment instance data directory even if the custom files are also in the active segment instance. You must restore the custom files manually. For example, when using the `gpfdists` protocol \(`gpfdist` with SSL encryption\) to manage external data, client certificate files are required in the segment instance `$PGDATA/gpfdists` directory. These files are not restored. For information about configuring `gpfdists`, see [Encrypting gpfdist Connections](../../security-guide/topics/Encryption.html).

-i recover\_config\_file
:   Specifies the name of a file with the details about failed segments to recover. Each line in the file is in the following format. The word **SPACE** indicates the location of a required space. Do not add additional spaces.

    ```
    filespaceOrder=[filespace1_fsname[, filespace2_fsname[, ...]]
    <failed_host_address>:<port>:<data_directory>SPACE 
    <recovery_host_address>:<port>:<replication_port>:<data_directory>
    [:<fselocation>:...]
    ```

    **Comments**

    Lines beginning with `#` are treated as comments and ignored.

    **Filespace Order**

    The first comment line that is not a comment specifies filespace ordering. This line starts with `filespaceOrder=` and is followed by list of filespace names delimited by a colon. For example:

    ```
    filespaceOrder=raid1:raid2
    ```

    The default `pg_system` filespace should not appear in this list. The list should be left empty on a system with no filespaces other than the default `pg_system` filespace. For example:

    ```
    filespaceOrder=
    ```

    **Segments to Recover**

    Each line after the first specifies a segment to recover. This line can have one of two formats. In the event of in-place recovery, enter one group of colon delimited fields in the line. For example:

    ```
    failedAddress:failedPort:failedDataDirectory
    ```

    For recovery to a new location, enter two groups of fields separated by a space in the line. The required space is indicated by **SPACE**. Do not add additional spaces.

    ```
    failedAddress:failedPort:failedDataDirectorySPACEnewAddress:
    newPort:newReplicationPort:newDataDirectory
    ```

    On a system with additional filespaces, the second group of fields is expected to be followed with a list of the corresponding filespace locations separated by additional colons. For example, on a system with two additional filespaces, enter two additional directories in the second group, as follows. The required space is indicated by **SPACE**. Do not add additional spaces.

    ```
    failedAddress:failedPort:failedDataDirectorySPACEnewAddress:
    newPort:newReplicationPort:newDataDirectory:location1:location2
    ```

    **Examples**

    **In-place recovery of a single mirror**

    ```
    filespaceOrder=
    sdw1-1:50001:/data1/mirror/gpseg16
    ```

    **Recovery of a single mirror to a new host**

    ```
    filespaceOrder=
    sdw1-1:50001:/data1/mirror/gpseg16SPACEsdw4-1:
    50001:51001:/data1/recover1/gpseg16
    ```

    **Recovery of a single mirror to a new host on a system with an extra filespace**

    ```
    filespaceOrder=
    fs1sdw1-1:50001:/data1/mirror/gpseg16SPACEsdw4-1:
    50001:51001:/data1/recover1/gpseg16:/data1/fs1/gpseg16
    ```

    **Obtaining a Sample File**

    You can use the `-o` option to output a sample recovery configuration file to use as a starting point.

-l logfile\_directory
:   The directory to write the log file. Defaults to `~/gpAdminLogs`.

-o output\_recover\_config\_file
:   Specifies a file name and location to output a sample recovery configuration file. The output file lists the currently invalid segments and their default recovery location in the format that is required by the `-i` option. Use together with the `-p` option to output a sample file for recovering on a different host. This file can be edited to supply alternate recovery locations if needed.

-p new\_recover\_host\[,...\]
:   Specifies a spare host outside of the currently configured Greenplum Database array on which to recover invalid segments.

:   **Important:** The hostname of the new node needs to match the hostname being recovered.

:   The spare host must have the Greenplum Database software installed and configured, and have the same hardware and OS configuration as the current segment hosts \(same hostname, OS version, locales, `gpadmin` user account, data directory locations created, ssh keys exchanged, number of network interfaces, network interface naming convention, and so on\). Specifically, the Greenplum Database binaries must be installed, the spare host must be able to connect password-less with all segments including the Greenplum master, and any other Greenplum Database specific OS configuration parameters must be applied.

:   **Note:** In the case of multiple failed segment hosts, you can specify the hosts to recover with a comma-separated list. However, it is strongly recommended to recover one host at a time. If you must recover more than one host at a time, then it is critical to ensure that a double fault scenario does not occur, in which both the segment primary and corresponding mirror are offline.

-q \(no screen output\)
:   Run in quiet mode. Command output is not displayed on the screen, but is still written to the log file.

-r \(rebalance segments\)
:   After a segment recovery, segment instances may not be returned to the preferred role that they were given at system initialization time. This can leave the system in a potentially unbalanced state, as some segment hosts may have more active segments than is optimal for top system performance. This option rebalances primary and mirror segments by returning them to their preferred roles. All segments must be valid and synchronized before running `gprecoverseg -r`. If there are any in progress queries, they will be cancelled and rolled back.

-s filespace\_config\_file
:   Specifies the name of a configuration file that contains file system locations on the currently configured segment hosts where you can recover failed segment instances. The filespace configuration file is in the format of:

:   ```
pg_system=<default_fselocation>
<filespace1_name>=<filespace1_fselocation>
<filespace2_name>=<filespace2_fselocation>
...
```

:   If your system does not have additional filespaces configured, this file will only have one location \(for the default filespace, `pg_system`\). These file system locations must exist on all segment hosts in the array and have sufficient disk space to accommodate recovered segments.

:   Note: The `-s` and `-S` options are only used when you recover to existing hosts in the cluster. You cannot use these options when you recover to a new host. To recover to a new host, use the `-i` and `-o` options.

-S output\_filespace\_config\_file
:   Specifies a file name and location to output a sample filespace configuration file in the format that is required by the `-s` option. This file should be edited to supply the correct alternate filespace locations.

-v \(verbose\)
:   Sets logging output to verbose.

--version \(version\)
:   Displays the version of this utility.

-? \(help\)
:   Displays the online help.

## Examples 

Recover any failed segment instances in place:

```
$ gprecoverseg
```

Rebalance your Greenplum Database system after a recovery by resetting all segments to their preferred role. First check that all segments are up and synchronized.

```
$ gpstate -m
$ gprecoverseg -r
```

Recover any failed segment instances to a newly configured spare segment host:

```
$ gprecoverseg -i <recover_config_file>
```

Output the default recovery configuration file:

```
$ gprecoverseg -o /home/gpadmin/recover_config_file
```

## See Also 

[gpstart](gpstart.html), [gpstop](gpstop.html)

