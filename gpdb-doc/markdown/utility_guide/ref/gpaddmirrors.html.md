# gpaddmirrors 

Adds mirror segments to a Greenplum Database system that was initially configured without mirroring.

## <a id="section2"></a>Synopsis 

```
gpaddmirrors [-p <port_offset>] [-m <datadir_config_file> [-a]] [-s] 
   [-d <coordinator_data_directory>] [-b <segment_batch_size>] [-B <batch_size>] [-l <logfile_directory>]
   [-v] [--hba-hostnames <boolean>] 

gpaddmirrors -i <mirror_config_file> [-a] [-d <coordinator_data_directory>]
   [-b <segment_batch_size>] [-B <batch_size>] [-l <logfile_directory>] [-v]

gpaddmirrors -o output_sample_mirror_config> [-s] [-m <datadir_config_file>]

gpaddmirrors -? 

gpaddmirrors --version
```

## <a id="section3"></a>Description 

The `gpaddmirrors` utility configures mirror segment instances for an existing Greenplum Database system that was initially configured with primary segment instances only. The utility will create the mirror instances and begin the online replication process between the primary and mirror segment instances. Once all mirrors are synchronized with their primaries, your Greenplum Database system is fully data redundant.

> **Important** During the online replication process, Greenplum Database should be in a quiescent state, workloads and other queries should not be running.

By default, the utility will prompt you for the file system location\(s\) where it will create the mirror segment data directories. If you do not want to be prompted, you can pass in a file containing the file system locations using the `-m` option.

The mirror locations and ports must be different than your primary segment data locations and ports.

The utility creates a unique data directory for each mirror segment instance in the specified location using the predefined naming convention. There must be the same number of file system locations declared for mirror segment instances as for primary segment instances. It is OK to specify the same directory name multiple times if you want your mirror data directories created in the same location, or you can enter a different data location for each mirror. Enter the absolute path. For example:

```
Enter mirror segment data directory location 1 of 2 > /gpdb/mirror
Enter mirror segment data directory location 2 of 2 > /gpdb/mirror
```

OR

```
Enter mirror segment data directory location 1 of 2 > /gpdb/m1
Enter mirror segment data directory location 2 of 2 > /gpdb/m2
```

Alternatively, you can run the `gpaddmirrors` utility and supply a detailed configuration file using the `-i` option. This is useful if you want your mirror segments on a completely different set of hosts than your primary segments. The format of the mirror configuration file is:

```
<contentID>|<address>|<port>|<data_dir>
```

Where `<contentID>` is the segment instance content ID, `<address>` is the host name or IP address of the segment host, `<port>` is the communication port, and `<data_dir>` is the segment instance data directory.

For example:

```
0|sdw1-1|60000|/gpdata/m1/gp0
1|sdw1-1|60001|/gpdata/m2/gp1
```

The `gp_segment_configuration` system catalog table can help you determine your current primary segment configuration so that you can plan your mirror segment configuration. For example, run the following query:

```
=# SELECT dbid, content, address as host_address, port, datadir 
   FROM gp_segment_configuration
   ORDER BY dbid;
```

If you are creating mirrors on alternate mirror hosts, the new mirror segment hosts must be pre-installed with the Greenplum Database software and configured exactly the same as the existing primary segment hosts.

You must make sure that the user who runs `gpaddmirrors` \(the `gpadmin` user\) has permissions to write to the data directory locations specified. You may want to create these directories on the segment hosts and `chown` them to the appropriate user before running `gpaddmirrors`.

> **Note** This utility uses secure shell \(SSH\) connections between systems to perform its tasks. In large Greenplum Database deployments, cloud deployments, or deployments with a large number of segments per host, this utility may exceed the host's maximum threshold for unauthenticated connections. Consider updating the SSH `MaxStartups` configuration parameter to increase this threshold. For more information about SSH configuration options, refer to the SSH documentation for your Linux distribution.

## <a id="section4"></a>Options 

-a \(do not prompt\)
:   Run in quiet mode - do not prompt for information. Must supply a configuration file with either `-m` or `-i` if this option is used.

-b segment\_batch\_size
:   The maximum number of segments per host to operate on in parallel. Valid values are `1` to `128`. If not specified, the utility will start recovering up to 64 segments in parallel on each host.

-B batch\_size
:   The number of hosts to work on in parallel. If not specified, the utility will start working on up to 16 hosts in parallel. Valid values are `1` to `64`.

-d coordinator\_data\_directory
:   The coordinator data directory. If not specified, the value set for `$COORDINATOR_DATA_DIRECTORY` will be used.

--hba-hostnames boolean
:   Optional. Controls whether this utility uses IP addresses or host names in the `pg_hba.conf` file when updating this file with addresses that can connect to Greenplum Database. When set to 0 -- the default value -- this utility uses IP addresses when updating this file. When set to 1, this utility uses host names when updating this file. For consistency, use the same value that was specified for `HBA_HOSTNAMES` when the Greenplum Database system was initialized. For information about how Greenplum Database resolves host names in the `pg_hba.conf` file, see [Configuring Client Authentication](../../admin_guide/client_auth.html).

-i mirror\_config\_file
:   A configuration file containing one line for each mirror segment you want to create. You must have one mirror segment instance listed for each primary segment in the system. The format of this file is as follows \(as per attributes in the [gp\_segment\_configuration](../../ref_guide/system_catalogs/gp_segment_configuration.html) catalog table\):

:   ```
<contentID>|<address>|<port>|<data_dir>
```

:   Where `<contentID>` is the segment instance content ID, `<address>` is the hostname or IP address of the segment host, `<port>` is the communication port, and `<data_dir>` is the segment instance data directory. For information about using a hostname or IP address, see [Specifying Hosts using Hostnames or IP Addresses](#host_ip). Also, see [Using Host Systems with Multiple NICs](#multi_nic).

-l logfile\_directory
:   The directory to write the log file. Defaults to `~/gpAdminLogs`.

-m datadir\_config\_file
:   A configuration file containing a list of file system locations where the mirror data directories will be created. If not supplied, the utility prompts you for locations. Each line in the file specifies a mirror data directory location. For example:

:   ```
/gpdata/m1
/gpdata/m2
/gpdata/m3
/gpdata/m4
```

-o output\_sample\_mirror\_config
:   If you are not sure how to lay out the mirror configuration file used by the `-i` option, you can run `gpaddmirrors` with this option to generate a sample mirror configuration file based on your primary segment configuration. The utility will prompt you for your mirror segment data directory locations \(unless you provide these in a file using `-m`\). You can then edit this file to change the host names to alternate mirror hosts if necessary.

-p port\_offset
:   Optional. This number is used to calculate the database ports used for mirror segments. The default offset is 1000. Mirror port assignments are calculated as follows:
    ```
    primary_port + offset = mirror_database_port
    ```

:   For example, if a primary segment has port 50001, then its mirror will use a database port of 51001, by default.

-s \(spread mirrors\)
:   Spreads the mirror segments across the available hosts. The default is to group a set of mirror segments together on an alternate host from their primary segment set. Mirror spreading will place each mirror on a different host within the Greenplum Database array. Spreading is only allowed if there is a sufficient number of hosts in the array \(number of hosts is greater than the number of segment instances per host\).

-v \(verbose\)
:   Sets logging output to verbose.

--version \(show utility version\)
:   Displays the version of this utility.

-? \(help\)
:   Displays the online help.

## <a id="host_ip"></a>Specifying Hosts using Hostnames or IP Addresses 

When specifying a mirroring configuration using the `gpaddmirrors` option `-i`, you can specify either a hostname or an IP address for the <address\> value.

-   If you specify a hostname, the resolution of the hostname to an IP address should be done locally for security. For example, you should use entries in a local `/etc/hosts` file to map the hostname to an IP address. The resolution of a hostname to an IP address should not be performed by an external service such as a public DNS server. You must stop the Greenplum system before you change the mapping of a hostname to a different IP address.
-   If you specify an IP address, the address should not be changed after the initial configuration. When segment mirroring is enabled, replication from the primary to the mirror segment will fail if the IP address changes from the configured value. For this reason, you should use a hostname when enabling mirroring using the `-i` option unless you have a specific requirement to use IP addresses.

When enabling a mirroring configuration that adds hosts to the Greenplum system, `gpaddmirrors` populates the [gp\_segment\_configuration](../../ref_guide/system_catalogs/gp_segment_configuration.html) catalog table with the mirror segment instance information. Greenplum Database uses the address value of the `gp_segment_configuration` catalog table when looking up host systems for Greenplum interconnect \(internal\) communication between the coordinator and segment instances and between segment instances, and for other internal communication.

## <a id="multi_nic"></a>Using Host Systems with Multiple NICs 

If hosts systems are configured with multiple NICs, you can initialize a Greenplum Database system to use each NIC as a Greenplum host system. You must ensure that the host systems are configured with sufficient resources to support all the segment instances being added to the host. Also, if you enable segment mirroring, you must ensure that the Greenplum system configuration supports failover if a host system fails. For information about Greenplum Database mirroring schemes, see [Segment Mirroring Configurations](../../best_practices/ha.html#topic_ngz_qf4_tt).

For example, this is a segment instance configuration for a simple Greenplum system. The segment host `gp7c` is configured with two NICs, `gp7c-1` and `gp7c-2`, where the Greenplum Database system uses `gp7c-1` for the coordinator segment and `gp7c-2` for segment instances.

```
select content, role, port, hostname, address from gp_segment_configuration ;

 content | role | port  | hostname | address
---------+------+-------+----------+----------
      -1 | p    |  5432 | gp7c     | gp7c-1
       0 | p    | 40000 | gp7c     | gp7c-2
       0 | m    | 50000 | gp7s     | gp7s
       1 | p    | 40000 | gp7s     | gp7s
       1 | m    | 50000 | gp7c     | gp7c-2
(5 rows) 
```

## <a id="section5"></a>Examples 

Add mirroring to an existing Greenplum Database system using the same set of hosts as your primary data. Calculate the mirror database ports by adding 100 to the current primary segment port numbers:

```
$ gpaddmirrors -p 100
```

Generate a sample mirror configuration file with the `-o` option to use with `gpaddmirrors -i`:

```
$ gpaddmirrors -o /home/gpadmin/sample_mirror_config
```

Add mirroring to an existing Greenplum Database system using a different set of hosts from your primary data:

```
$ gpaddmirrors -i mirror_config_file
```

Where `mirror_config_file` looks something like this:

```
0|sdw1-1|52001|/gpdata/m1/gp0
1|sdw1-2|52002|/gpdata/m2/gp1
2|sdw2-1|52001|/gpdata/m1/gp2
3|sdw2-2|52002|/gpdata/m2/gp3
```

## <a id="section6"></a>See Also 

[gpinitsystem](gpinitsystem.html), [gpinitstandby](gpinitstandby.html), [gpactivatestandby](gpactivatestandby.html)

