# Enabling Segment Mirroring 

Mirror segments allow database queries to fail over to a backup segment if the primary segment is unavailable. By default, mirrors are configured on the same array of hosts as the primary segments. You may choose a completely different set of hosts for your mirror segments so they do not share machines with any of your primary segments.

**Important:** During the online data replication process, Greenplum Database should be in a quiescent state, workloads and other queries should not be running.

## To add segment mirrors to an existing system \(same hosts as primaries\) 

1.  Allocate the data storage area for mirror data on all segment hosts. The data storage area must be different from your primary segments' file system location.
2.  Use `gpssh-exkeys` to ensure that the segment hosts can SSH and SCP to each other without a password prompt.
3.  Run the `gpaddmirrors` utility to enable mirroring in your Greenplum Database system. For example, to add 10000 to your primary segment port numbers to calculate the mirror segment port numbers:

    ```
    $ gpaddmirrors -p 10000
    ```

    Where `-p` specifies the number to add to your primary segment port numbers. Mirrors are added with the default group mirroring configuration.


## To add segment mirrors to an existing system \(different hosts from primaries\) 

1.  Ensure the Greenplum Database software is installed on all hosts. See the *Greenplum Database Installation Guide* for detailed installation instructions.
2.  Allocate the data storage area for mirror data on all segment hosts.
3.  Use `gpssh-exkeys` to ensure the segment hosts can SSH and SCP to each other without a password prompt.
4.  Create a configuration file that lists the host names, ports, and data directories on which to create mirrors. To create a sample configuration file to use as a starting point, run:

    ```
    $ gpaddmirrors -o <filename>          
    ```

    The format of the mirror configuration file is:

    ```
    filespaceOrder=[<filespace1_fsname>[:<filespace2_fsname>:...] 
    <mirror>[<content>]=<content>:<address>:<port>:<mir_replication_port>:
    <pri_replication_port>:<fselocation>[:<fselocation>:...]
    ```

    For example, a configuration for two segment hosts and two segments per host, with no additional filespaces configured besides the default *pg\_system* filespace:

    ```
    filespaceOrder=
    mirror0=0:sdw1-1:52001:53001:54001:/gpdata/mir1/gp0
    mirror1=1:sdw1-2:52002:53002:54002:/gpdata/mir1/gp1
    mirror2=2:sdw2-1:52001:53001:54001:/gpdata/mir1/gp2
    mirror3=3:sdw2-2:52002:53002:54002:/gpdata/mir1/gp3
    
    ```

5.  Run the `gpaddmirrors` utility to enable mirroring in your Greenplum Database system:

    ```
    $ gpaddmirrors -i <mirror_config_file>
                   
    ```

    Where `-i` names the mirror configuration file you created.


**Parent topic:** [Enabling Mirroring in Greenplum Database](../../highavail/topics/g-enabling-mirroring-in-greenplum-database.html)

