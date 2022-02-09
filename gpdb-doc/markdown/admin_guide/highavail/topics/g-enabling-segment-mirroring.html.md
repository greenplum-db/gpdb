---
title: Enabling Segment Mirroring 
---

Mirror segments allow database queries to fail over to a backup segment if the primary segment is unavailable. By default, mirrors are configured on the same array of hosts as the primary segments. You may choose a completely different set of hosts for your mirror segments so they do not share machines with any of your primary segments.

**Important:** During the online data replication process, Greenplum Database should be in a quiescent state, workloads and other queries should not be running.

## <a id="ki169450"></a>To add segment mirrors to an existing system \(same hosts as primaries\) 

1.  Allocate the data storage area for mirror data on all segment hosts. The data storage area must be different from your primary segments' file system location.
2.  Use [gpssh-exkeys](../../../utility_guide/ref/gpssh-exkeys.html) to ensure that the segment hosts can SSH and SCP to each other without a password prompt.
3.  Run the [gpaddmirrors](../../../utility_guide/ref/gpaddmirrors.html) utility to enable mirroring in your Greenplum Database system. For example, to add 10000 to your primary segment port numbers to calculate the mirror segment port numbers:

    ```
    $ gpaddmirrors -p 10000
    ```

    Where `-p` specifies the number to add to your primary segment port numbers. Mirrors are added with the default group mirroring configuration.


## <a id="toadd"></a>To add segment mirrors to an existing system \(different hosts from primaries\) 

1.  Ensure the Greenplum Database software is installed on all hosts. See the *Greenplum Database Installation Guide* for detailed installation instructions.
2.  Allocate the data storage area for mirror data, and tablespaces if needed, on all segment hosts.
3.  Use `gpssh-exkeys` to ensure the segment hosts can SSH and SCP to each other without a password prompt.
4.  Create a configuration file that lists the host names, ports, and data directories on which to create mirrors. To create a sample configuration file to use as a starting point, run:

    ```
    $ gpaddmirrors -o <filename>          
    ```

    The format of the mirror configuration file is:

    ```
    <row_id>=<contentID>|<address>|<port>|<data_dir>
    ```

    Where `row_id` is the row in the file, contentID is the segment instance content ID, address is the host name or IP address of the segment host, port is the communication port, and `data_dir` is the segment instance data directory.

    For example, this is contents of a mirror configuration file for two segment hosts and two segment instances per host:

    ```
    0=2|sdw1-1|41000|/data/mirror1/gp2
    1=3|sdw1-2|41001|/data/mirror2/gp3
    2=0|sdw2-1|41000|/data/mirror1/gp0
    3=1|sdw2-2|41001|/data/mirror2/gp1
    ```

5.  Run the `gpaddmirrors` utility to enable mirroring in your Greenplum Database system:

    ```
    $ gpaddmirrors -i <mirror_config_file>
    ```

    The `-i` option specifies the mirror configuration file you created.


**Parent topic:**[Enabling Mirroring in Greenplum Database](../../highavail/topics/g-enabling-mirroring-in-greenplum-database.html)

