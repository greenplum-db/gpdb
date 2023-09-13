# gpcheckperf 

Verifies the baseline hardware performance of the specified hosts.

## <a id="section2"></a>Synopsis 

```
gpcheckperf -d <test_directory> [-d <test_directory> ...] 
    {-f <hostfile_gpcheckperf> | - h <hostname> [-h hostname ...]} 
    [-r ds] [-B <block_size>] [-S <file_size>] [--buffer-size <buffer_size>] [-D] [-v|-V]

gpcheckperf -d <temp_directory>
    {-f <hostfile_gpchecknet> | - h <hostname> [-h< hostname> ...]} 
    [ -r n|N|M [--duration <time>] [--netperf] ] [-D] [-v | -V]

gpcheckperf -?

gpcheckperf --version
```

## <a id="section3"></a>Description 

The `gpcheckperf` utility starts a session on the specified hosts and runs the following performance tests:

-   **Disk I/O Test \(dd test\)** — To test the sequential throughput performance of a logical disk or file system, the utility uses the **dd** command, which is a standard UNIX utility. It times how long it takes to write and read a large file to and from disk and calculates your disk I/O performance in megabytes \(MB\) per second. By default, the file size that is used for the test is calculated at two times the total random access memory \(RAM\) on the host. This ensures that the test is truly testing disk I/O and not using the memory cache.
-   **Memory Bandwidth Test \(stream\)** — To test memory bandwidth, the utility uses the STREAM benchmark program to measure sustainable memory bandwidth \(in MB/s\). This tests that your system is not limited in performance by the memory bandwidth of the system in relation to the computational performance of the CPU. In applications where the data set is large \(as in Greenplum Database\), low memory bandwidth is a major performance issue. If memory bandwidth is significantly lower than the theoretical bandwidth of the CPU, then it can cause the CPU to spend significant amounts of time waiting for data to arrive from system memory.
-   **Network Performance Test \(gpnetbench\*\)** — To test network performance \(and thereby the performance of the Greenplum Database interconnect\), the utility runs a network benchmark program that transfers a 5 second stream of data from the current host to each remote host included in the test. The data is transferred in parallel to each remote host and the minimum, maximum, average and median network transfer rates are reported in megabytes \(MB\) per second. If the summary transfer rate is slower than expected \(less than 100 MB/s\), you can run the network test serially using the `-r n` option to obtain per-host results. To run a full-matrix bandwidth test, you can specify `-r M` which will cause every host to send and receive data from every other host specified. This test is best used to validate if the switch fabric can tolerate a full-matrix workload.

To specify the hosts to test, use the `-f` option to specify a file containing a list of host names, or use the `-h` option to name single host names on the command-line. If running the network performance test, all entries in the host file must be for network interfaces within the same subnet. If your segment hosts have multiple network interfaces configured on different subnets, run the network test once for each subnet.

You must also specify at least one test directory \(with `-d`\). The user who runs `gpcheckperf` must have write access to the specified test directories on all remote hosts. For the disk I/O test, the test directories should correspond to your segment data directories \(primary and/or mirrors\). For the memory bandwidth and network tests, a temporary directory is required for the test program files.

Before using `gpcheckperf`, you must have a trusted host setup between the hosts involved in the performance test. You can use the utility `gpssh-exkeys` to update the known host files and exchange public keys between hosts if you have not done so already. Note that `gpcheckperf` calls to `gpssh` and `gpscp`, so these Greenplum utilities must also be in your `$PATH`.

## <a id="section4"></a>Options 

-B block\_size
:   Specifies the block size \(in KB or MB\) to use for disk I/O test. The default is 32KB, which is the same as the Greenplum Database page size. The maximum block size is 1 MB.

--buffer-size buffer_size 
:    Specifies the size of the send buffer in kilobytes. Default size is 32 kilobytes.

-d test\_directory
:   For the disk I/O test, specifies the file system directory locations to test. You must have write access to the test directory on all hosts involved in the performance test. You can use the `-d` option multiple times to specify multiple test directories \(for example, to test disk I/O of your primary and mirror data directories\).

-d temp\_directory
:   For the network and stream tests, specifies a single directory where the test program files will be copied for the duration of the test. You must have write access to this directory on all hosts involved in the test.

-D \(display per-host results\)
:   Reports performance results for each host for the disk I/O tests. The default is to report results for just the hosts with the minimum and maximum performance, as well as the total and average performance of all hosts.

--duration time
:   Specifies the duration of the network test in seconds \(s\), minutes \(m\), hours \(h\), or days \(d\). The default is 15 seconds.

-f hostfile\_gpcheckperf
:   For the disk I/O and stream tests, specifies the name of a file that contains one host name per host that will participate in the performance test. The host name is required, and you can optionally specify an alternate user name and/or SSH port number per host. The syntax of the host file is one host per line as follows:

:   ```
[<username>@]<hostname>[:<ssh_port>]
```

-f hostfile\_gpchecknet
:   For the network performance test, all entries in the host file must be for host addresses within the same subnet. If your segment hosts have multiple network interfaces configured on different subnets, run the network test once for each subnet. For example \(a host file containing segment host address names for interconnect subnet 1\):

:   ```
sdw1-1
sdw2-1
sdw3-1
```

-h hostname
:   Specifies a single host name \(or host address\) that will participate in the performance test. You can use the `-h` option multiple times to specify multiple host names.

--netperf
:   Specifies that the `netperf` binary should be used to perform the network test instead of the Greenplum network test. To use this option, you must download `netperf` from [https://github.com/HewlettPackard/netperf](https://github.com/HewlettPackard/netperf) and install it into `$GPHOME/bin/lib` on all Greenplum hosts \(master and segments\).

-r ds\{n\|N\|M\}
:   Specifies which performance tests to run. The default is `dsn`:
    -   Disk I/O test \(`d`\)
    -   Stream test \(`s`\)
    -   Network performance test in sequential \(`n`\), parallel \(`N`\), or full-matrix \(`M`\) mode. The optional `--duration` option specifies how long \(in seconds\) to run the network test. To use the parallel \(`N`\) mode, you must run the test on an even number of hosts.
        <br/><br/>If you would rather use `netperf` \([https://github.com/HewlettPackard/netperf](https://github.com/HewlettPackard/netperf)\) instead of the Greenplum network test, you can download it and install it into `$GPHOME/bin/lib` on all Greenplum hosts \(master and segments\). You would then specify the optional `--netperf` option to use the `netperf` binary instead of the default `gpnetbench*` utilities.


-S file\_size
:   Specifies the total file size to be used for the disk I/O test for all directories specified with `-d`. file\_size should equal two times total RAM on the host. If not specified, the default is calculated at two times the total RAM on the host where `gpcheckperf` is run. This ensures that the test is truly testing disk I/O and not using the memory cache. You can specify sizing in KB, MB, or GB.

-v \(verbose\) \| -V \(very verbose\)
:   Verbose mode shows progress and status messages of the performance tests as they are run. Very verbose mode shows all output messages generated by this utility.

--version
:   Displays the version of this utility.

-? \(help\)
:   Displays the online help.

## <a id="section5"></a>Examples 

Run the disk I/O and memory bandwidth tests on all the hosts in the file host\_file using the test directory of /data1 and /data2:

```
$ gpcheckperf -f hostfile_gpcheckperf -d /data1 -d /data2 -r ds
```

Run only the disk I/O test on the hosts named sdw1 and sdw2using the test directory of /data1. Show individual host results and run in verbose mode:

```
$ gpcheckperf -h sdw1 -h sdw2 -d /data1 -r d -D -v
```

Run the parallel network test using the test directory of /tmp, where hostfile\_gpcheck\_ic\* specifies all network interface host address names within the same interconnect subnet:

```
$ gpcheckperf -f hostfile_gpchecknet_ic1 -r N -d /tmp
$ gpcheckperf -f hostfile_gpchecknet_ic2 -r N -d /tmp
```

Run the same test as above, but use `netperf` instead of the Greenplum network test \(note that `netperf` must be installed in `$GPHOME/bin/lib` on all Greenplum hosts\):

```
$ gpcheckperf -f hostfile_gpchecknet_ic1 -r N --netperf -d /tmp
$ gpcheckperf -f hostfile_gpchecknet_ic2 -r N --netperf -d /tmp
```

## <a id="section6"></a>See Also 

[gpssh](gpssh.html), [gpscp](gpscp.html)

