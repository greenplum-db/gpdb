---
title: Validating Your Systems 
---

Validate your hardware and network performance.

Greenplum provides a management utility called [gpcheckperf](../utility_guide/ref/gpcheckperf.html), which can be used to identify hardware and system-level issues on the machines in your Greenplum Database array. [gpcheckperf](../utility_guide/ref/gpcheckperf.html) starts a session on the specified hosts and runs the following performance tests:

-   Network Performance \(`gpnetbench*`\)
-   Disk I/O Performance \(`dd` test\)
-   Memory Bandwidth \(`stream` test\)

Before using `gpcheckperf`, you must have a trusted host setup between the hosts involved in the performance test. You can use the utility [gpssh-exkeys](../utility_guide/ref/gpssh-exkeys.html) to update the known host files and exchange public keys between hosts if you have not done so already. Note that `gpcheckperf` calls to [gpssh](../utility_guide/ref/gpssh.html) and [gpscp](../utility_guide/ref/gpscp.html), so these Greenplum utilities must be in your `$PATH`.

-   **[Validating Network Performance](validate.html)**  

-   **[Validating Disk I/O and Memory Bandwidth](validate.html)**  


**Parent topic:**[Installing and Upgrading Greenplum](install_guide.html)

## <a id="topic4"></a>Validating Network Performance 

To test network performance, run [gpcheckperf](../utility_guide/ref/gpcheckperf.html) with one of the network test run options: parallel pair test \(`-r N`\), serial pair test \(`-r n`\), or full matrix test \(`-r M`\). The utility runs a network benchmark program that transfers a 5 second stream of data from the current host to each remote host included in the test. By default, the data is transferred in parallel to each remote host and the minimum, maximum, average and median network transfer rates are reported in megabytes \(MB\) per second. If the summary transfer rate is slower than expected \(less than 100 MB/s\), you can run the network test serially using the `-r n` option to obtain per-host results. To run a full-matrix bandwidth test, you can specify `-r M` which will cause every host to send and receive data from every other host specified. This test is best used to validate if the switch fabric can tolerate a full-matrix workload.

Most systems in a Greenplum Database array are configured with multiple network interface cards \(NICs\), each NIC on its own subnet. When testing network performance, it is important to test each subnet individually. For example, considering the following network configuration of two NICs per host:

|Greenplum Host|Subnet1 NICs|Subnet2 NICs|
|--------------|------------|------------|
|Segment 1|sdw1-1|sdw1-2|
|Segment 2|sdw2-1|sdw2-2|
|Segment 3|sdw3-1|sdw3-2|

You would create four distinct host files for use with the [gpcheckperf](../utility_guide/ref/gpcheckperf.html) network test:

|hostfile\_gpchecknet\_ic1|hostfile\_gpchecknet\_ic2|
|-------------------------|-------------------------|
|sdw1-1|sdw1-2|
|sdw2-1|sdw2-2|
|sdw3-1|sdw3-2|

You would then run [gpcheckperf](../utility_guide/ref/gpcheckperf.html) once per subnet. For example \(if testing an *even* number of hosts, run in parallel pairs test mode\):

```
$ gpcheckperf -f hostfile_gpchecknet_ic1 -r N -d /tmp > subnet1.out
$ gpcheckperf -f hostfile_gpchecknet_ic2 -r N -d /tmp > subnet2.out
```

If you have an *odd* number of hosts to test, you can run in serial test mode \(`-r n`\).

**Parent topic:**[Validating Your Systems](validate.html)

## <a id="topic5"></a>Validating Disk I/O and Memory Bandwidth 

To test disk and memory bandwidth performance, run [gpcheckperf](../utility_guide/ref/gpcheckperf.html) with the disk and stream test run options \(`-r ds`\). The disk test uses the `dd` command \(a standard UNIX utility\) to test the sequential throughput performance of a logical disk or file system. The memory test uses the STREAM benchmark program to measure sustainable memory bandwidth. Results are reported in MB per second \(MB/s\).

### <a id="jj161569"></a>To run the disk and stream tests 

1.  Log in on the coordinator host as the `gpadmin` user.
2.  Source the `greenplum_path.sh` path file from your Greenplum installation. For example:

    ```
    $ source /usr/local/greenplum-db/greenplum_path.sh
    ```

3.  Create a host file named `hostfile_gpcheckperf` that has one host name per segment host. Do not include the coordinator host. For example:

    ```
    sdw1
    sdw2
    sdw3
    sdw4
    ```

4.  Run the `gpcheckperf` utility using the `hostfile_gpcheckperf` file you just created. Use the `-d` option to specify the file systems you want to test on each host \(you must have write access to these directories\). You will want to test all primary and mirror segment data directory locations. For example:

    ```
    $ gpcheckperf -f hostfile_gpcheckperf -r ds -D \
      -d /data1/primary -d  /data2/primary \
      -d /data1/mirror -d  /data2/mirror
    ```

5.  The utility may take a while to perform the tests as it is copying very large files between the hosts. When it is finished you will see the summary results for the Disk Write, Disk Read, and Stream tests.

**Parent topic:**[Validating Your Systems](validate.html)

