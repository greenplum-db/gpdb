---
title: Preparing and Adding Hosts 
---

Verify your new host systems are ready for integration into the existing Greenplum system.

To prepare new host systems for expansion, install the Greenplum Database software binaries, exchange the required SSH keys, and run performance tests.

Run performance tests first on the new hosts and then all hosts. Run the tests on all hosts with the system offline so user activity does not distort results.

Generally, you should run performance tests when an administrator modifies host networking or other special conditions in the system. For example, if you will run the expanded system on two network clusters, run tests on each cluster.

**Note:** Preparing host systems for use by a Greenplum Database system assumes that the new hosts' operating system has been properly configured to match the existing hosts, described in [Configuring Your Systems](../../install_guide/prep_os.html).

**Parent topic:**[Expanding a Greenplum System](../expand/expand-main.html)

## <a id="topic18"></a>Adding New Hosts to the Trusted Host Environment 

New hosts must exchange SSH keys with the existing hosts to enable Greenplum administrative utilities to connect to all segments without a password prompt. Perform the key exchange process twice with the [gpssh-exkeys](../../utility_guide/ref/gpssh-exkeys.html) utility.

First perform the process as `root`, for administration convenience, and then as the user `gpadmin`, for management utilities. Perform the following tasks in order:

1.  [To exchange SSH keys as root](#no160715)
2.  [To create the gpadmin user](#no160595)
3.  [To exchange SSH keys as the gpadmin user](#sshexch_gpadmin)

**Note:** The Greenplum Database segment host naming convention is `sdwN` where `sdw` is a prefix and `N` is an integer \( `sdw1`, `sdw2` and so on\). For hosts with multiple interfaces, the convention is to append a dash \(`-`\) and number to the host name. For example, `sdw1-1` and `sdw1-2` are the two interface names for host `sdw1`.

### <a id="no160715"></a>To exchange SSH keys as root 

1.  Create a host file with the existing host names in your array and a separate host file with the new expansion host names. For existing hosts, you can use the same host file used to set up SSH keys in the system. In the files, list all hosts \(master, backup master, and segment hosts\) with one name per line and no extra lines or spaces. Exchange SSH keys using the configured host names for a given host if you use a multi-NIC configuration. In this example, `mdw` is configured with a single NIC, and `sdw1`, `sdw2`, and `sdw3` are configured with 4 NICs:

    ```
    mdw
    sdw1-1
    sdw1-2
    sdw1-3
    sdw1-4
    sdw2-1
    sdw2-2
    sdw2-3
    sdw2-4
    sdw3-1
    sdw3-2
    sdw3-3
    sdw3-4
    ```

2.  Log in as `root` on the master host, and source the `greenplum_path.sh` file from your Greenplum installation.

    ```
    $ su - 
    # source /usr/local/greenplum-db/greenplum_path.sh
    ```

3.  Run the `gpssh-exkeys` utility referencing the host list files. For example:

    ```
    # gpssh-exkeys -e /home/gpadmin/<existing_hosts_file> -x 
    /home/gpadmin/<new_hosts_file>
    ```

4.  `gpssh-exkeys` checks the remote hosts and performs the key exchange between all hosts. Enter the `root` user password when prompted. For example:

    ```
    ***Enter password for root@<hostname>: <root_password>
    ```


### <a id="no160595"></a>To create the gpadmin user 

1.  Use [gpssh](../../utility_guide/ref/gpssh.html) to create the `gpadmin` user on all the new segment hosts \(if it does not exist already\). Use the list of new hosts you created for the key exchange. For example:

    ```
    # gpssh -f <new_hosts_file> '/usr/sbin/useradd gpadmin -d 
    /home/gpadmin -s /bin/bash'
    ```

2.  Set a password for the new `gpadmin` user. On Linux, you can do this on all segment hosts simultaneously using `gpssh`. For example:

    ```
    # gpssh -f <new_hosts_file> 'echo <gpadmin_password> | passwd 
    gpadmin --stdin'
    ```

3.  Verify the `gpadmin` user has been created by looking for its home directory:

    ```
    # gpssh -f <new_hosts_file> ls -l /home
    ```


### <a id="sshexch_gpadmin"></a>To exchange SSH keys as the gpadmin user 

1.  Log in as `gpadmin` and run the `gpssh-exkeys` utility referencing the host list files. For example:

    ```
    # gpssh-exkeys -e /home/gpadmin/<existing_hosts_file> -x 
    /home/gpadmin/<new_hosts_file>
    ```

2.  `gpssh-exkeys` will check the remote hosts and perform the key exchange between all hosts. Enter the `gpadmin` user password when prompted. For example:

    ```
    ***Enter password for gpadmin@<hostname>: <gpadmin_password>
    ```


## <a id="topic20"></a>Validating Disk I/O and Memory Bandwidth 

Use the [gpcheckperf](../../utility_guide/ref/gpcheckperf.html) utility to test disk I/O and memory bandwidth.

### <a id="no159247"></a>To run gpcheckperf 

1.  Run the `gpcheckperf` utility using the host file for new hosts. Use the `-d` option to specify the file systems you want to test on each host. You must have write access to these directories. For example:

    ```
    $ gpcheckperf -f <new_hosts_file> -d /data1 -d /data2 -v 
    ```

2.  The utility may take a long time to perform the tests because it is copying very large files between the hosts. When it is finished, you will see the summary results for the Disk Write, Disk Read, and Stream tests.

For a network divided into subnets, repeat this procedure with a separate host file for each subnet.

## <a id="topic21"></a>Integrating New Hardware into the System 

Before initializing the system with the new segments, shut down the system with `gpstop` to prevent user activity from skewing performance test results. Then, repeat the performance tests using host files that include *all* hosts, existing and new.

