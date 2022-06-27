# Checking for Failed Segments 

With mirroring enabled, you may have failed segments in the system without interruption of service or any indication that a failure has occurred. You can verify the status of your system using the `gpstate` utility. `gpstate` provides the status of each individual component of a Greenplum Database system, including primary segments, mirror segments, master, and standby master.

## To check for failed segments 

1.  On the master, run the `gpstate` utility with the `-e` option to show segments with error conditions:

    ```
    $ gpstate -e
    ```

    Segments in *Change Tracking* mode indicate the corresponding mirror segment is down. When a segment is not in its *preferred role*, the segment does not operate in the role to which it was assigned at system initialization. This means the system is in a potentially unbalanced state, as some segment hosts may have more active segments than is optimal for top system performance.

    See [Recovering From Segment Failures](g-recovering-from-segment-failures.html) for instructions to fix this situation.

2.  To get detailed information about a failed segment, check the *gp\_segment\_configuration* catalog table. For example:

    ```
    $ psql -c "SELECT * FROM gp_segment_configuration WHERE status='d';"
    ```

3.  For failed segment instances, note the host, port, preferred role, and data directory. This information will help determine the host and segment instances to troubleshoot.
4.  To show information about mirror segment instances, run:

    ```
    $ gpstate -m
    ```


**Parent topic:** [Detecting a Failed Segment](../../highavail/topics/g-detecting-a-failed-segment.html)

