---
title: System Monitoring and Maintenance 
---

Best practices for regular maintenance that will ensure Greenplum Database high availability and optimal performance.

-   **[Updating Statistics with ANALYZE](analyze.html)**  

-   **[Managing Bloat in a Database](bloat.html)**  

-   **[Monitoring Greenplum Database Log Files](logfiles.html)**  


**Parent topic:** [Greenplum Database Best Practices](intro.html)

## <a id="topic_izw_bwb_s4"></a>Monitoring 

Greenplum Database includes utilities that are useful for monitoring the system.

The `gp_toolkit` schema contains several views that can be accessed using SQL commands to query system catalogs, log files, and operating environment for system status information.

The `gp_stats_missing` view shows tables that do not have statistics and require `ANALYZE` to be run.

For additional information on `gpstate` and `gpcheckperf` refer to the *Greenplum Database Utility Guide*. For information about the `gp_toolkit` schema, see the *Greenplum Database Reference Guide*.

### <a id="gpstate"></a>gpstate 

The `gpstate` utility program displays the status of the Greenplum system, including which segments are down, coordinator and segment configuration information \(hosts, data directories, etc.\), the ports used by the system, and mapping of primary segments to their corresponding mirror segments.

Run `gpstate -Q` to get a list of segments that are marked "down" in the coordinator system catalog.

To get detailed status information for the Greenplum system, run `gpstate -s`.

### <a id="gpcheckperf"></a>gpcheckperf 

The `gpcheckperf` utility tests baseline hardware performance for a list of hosts. The results can help identify hardware issues. It performs the following checks:

-   disk I/O test – measures I/O performance by writing and reading a large file using the `dd` operating system command. It reports read and write rates in megabytes per second.
-   memory bandwidth test – measures sustainable memory bandwidth in megabytes per second using the STREAM benchmark.
-   network performance test – runs the `gpnetbench` network benchmark program \(optionally `netperf)` to test network performance. The test is run in one of three modes: parallel pair test \(`-r N`\), serial pair test \(`-r n`\), or full-matrix test \(`-r M)`. The minimum, maximum, average, and median transfer rates are reported in megabytes per second.

To obtain valid numbers from `gpcheckperf`, the database system must be stopped. The numbers from `gpcheckperf` can be inaccurate even if the system is up and running with no query activity.

`gpcheckperf` requires a trusted host setup between the hosts involved in the performance test. It calls `gpssh` and `gpsync`, so these utilities must also be in your `PATH`. Specify the hosts to check individually \(`-h *host1* -h *host2* ...`\) or with `-f *hosts_file*`, where `*hosts_file*` is a text file containing a list of the hosts to check. If you have more than one subnet, create a separate host file for each subnet so that you can test the subnets separately.

By default, `gpcheckperf` runs the disk I/O test, the memory test, and a serial pair network performance test. With the disk I/O test, you must use the `-d` option to specify the file systems you want to test. The following command tests disk I/O and memory bandwidth on hosts listed in the `subnet_1_hosts` file:

```
$ gpcheckperf -f subnet_1_hosts -d /data1 -d /data2 -r ds
```

The `-r` option selects the tests to run: disk I/O \(`d`\), memory bandwidth \(`s`\), network parallel pair \(`N`\), network serial pair test \(`n`\), network full-matrix test \(`M`\). Only one network mode can be selected per execution. See the *Greenplum Database Reference Guide* for the detailed `gpcheckperf` reference.

### <a id="monos"></a>Monitoring with Operating System Utilities 

The following Linux/UNIX utilities can be used to assess host performance:

-   `iostat` allows you to monitor disk activity on segment hosts.
-   `top` displays a dynamic view of operating system processes.
-   `vmstat` displays memory usage statistics.

You can use `gpssh` to run utilities on multiple hosts.

### <a id="bp1"></a>Best Practices 

-   Implement the "Recommended Monitoring and Maintenance Tasks" in the *Greenplum Database Administrator Guide*.
-   Run `gpcheckperf` at install time and periodically thereafter, saving the output to compare system performance over time.
-   Use all the tools at your disposal to understand how your system behaves under different loads.
-   Examine any unusual event to determine the cause.
-   Monitor query activity on the system by running explain plans periodically to ensure the queries are running optimally.
-   Review plans to determine whether index are being used and partition elimination is occurring as expected.

### <a id="addinfo"></a>Additional Information 

-   `gpcheckperf` reference in the *Greenplum Database Utility Guide*.
-   "Recommended Monitoring and Maintenance Tasks" in the *Greenplum Database Administrator Guide*.
-   [Sustainable Memory Bandwidth in Current High Performance Computers](http://www.cs.virginia.edu/%7Emccalpin/papers/bandwidth/bandwidth.html). John D. McCalpin. Oct 12, 1995.
-   [www.netperf.org](http://www.netperf.org/netperf) to use `netperf`, netperf must be installed on each host you test. See `gpcheckperf` reference for more information.

