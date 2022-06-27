# Recovering From Segment Failures 

Segment host failures usually cause multiple segment failures: all primary or mirror segments on the host are marked as down and nonoperational. If mirroring is not enabled and a segment goes down, the system automatically becomes nonoperational.

## To recover with mirroring enabled 

1.  Ensure you can connect to the segment host from the master host. For example:

    ```
    $ ping <failed_seg_host_address>
    ```

2.  Troubleshoot the problem that prevents the master host from connecting to the segment host. For example, the host machine may need to be restarted or replaced.
3.  After the host is online and you can connect to it, run the `gprecoverseg` utility from the master host to reactivate the failed segment instances. For example:

    ```
    $ gprecoverseg
    ```

4.  The recovery process brings up the failed segments and identifies the changed files that need to be synchronized. The process can take some time; wait for the process to complete. During this process, database write activity is suspended.
5.  After `gprecoverseg` completes, the system goes into *Resynchronizing* mode and begins copying the changed files. This process runs in the background while the system is online and accepting database requests.
6.  When the resynchronization process completes, the system state is *Synchronized*. Run the `gpstate` utility to verify the status of the resynchronization process:

    ```
    $ gpstate -m
    ```


## To return all segments to their preferred role 

When a primary segment goes down, the mirror activates and becomes the primary segment. After running `gprecoverseg`, the currently active segment remains the primary and the failed segment becomes the mirror. The segment instances are not returned to the preferred role that they were given at system initialization time. This means that the system could be in a potentially unbalanced state if segment hosts have more active segments than is optimal for top system performance. To check for unbalanced segments and rebalance the system, run:

```
$ gpstate -e
```

All segments must be online and fully synchronized to rebalance the system. Database sessions remain connected during rebalancing, but queries in progress are canceled and rolled back.

1.  Run `gpstate -m` to ensure all mirrors are *Synchronized*.

    ```
    $ gpstate -m
    ```

2.  If any mirrors are in *Resynchronizing* mode, wait for them to complete.
3.  Run gprecoverseg with the -r option to return the segments to their preferred roles.

    ```
    $ gprecoverseg -r
    ```

4.  After rebalancing, run `gpstate -e` to confirm all segments are in their preferred roles.

    ```
    $ gpstate -e
    ```


## To recover from a double fault 

In a double fault, both a primary segment and its mirror are down. This can occur if hardware failures on different segment hosts happen simultaneously. Greenplum Database is unavailable if a double fault occurs. To recover from a double fault:

1.  Restart Greenplum Database:

    ```
    $ gpstop -r
    ```

2.  After the system restarts, run `gprecoverseg`:

    ```
    $ gprecoverseg
    ```

3.  After `gprecoverseg` completes, use `gpstate` to check the status of your mirrors:

    ```
    $ gpstate -m
    ```

4.  If you still have segments in Change Tracking mode, run a full copy recovery:

    ```
    $ gprecoverseg -F
    ```


If a segment host is not recoverable and you have lost one or more segments, recreate your Greenplum Database system from backup files. See [Backing Up and Restoring Databases](../../managing/backup-main.html).

## To recover without mirroring enabled 

1.  Ensure you can connect to the segment host from the master host. For example:

    ```
    $ ping <failed_seg_host_address>
    ```

2.  Troubleshoot the problem that is preventing the master host from connecting to the segment host. For example, the host machine may need to be restarted.
3.  After the host is online, verify that you can connect to it and restart Greenplum Database. For example:

    ```
    $ gpstop -r
    ```

4.  Run the `gpstate` utility to verify that all segment instances are online:

    ```
    $ gpstate
    ```


-   **[When a segment host is not recoverable](../../highavail/topics/g-when-a-segment-host-is-not-recoverable.html)**  

-   **[About the Segment Recovery Process](../../highavail/topics/gprecover-steps.html)**  


**Parent topic:** [Recovering a Failed Segment](../../highavail/topics/g-recovering-a-failed-segment.html)

