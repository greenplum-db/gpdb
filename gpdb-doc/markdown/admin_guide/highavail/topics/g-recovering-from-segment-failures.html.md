---
title: Recovering from Segment Failures 
---

This topic walks you through what to do when one or more segments or hosts are down and you want to recover the down segments. The recovery path you follow depends primarily which of these 3 scenarios fits your circumstances:

-   you want to recover in-place to the current host

-   you want to recover to a different host, within the cluster

-   you want to recover to a new host, outside of the cluster


The steps you follow within these scenarios can vary, depending on:

-   whether you want to do an incremental or a full recovery

-   whether you want to recover all segments or just a subset of segments


> **Note** Incremental recovery is only possible when recovering segments to the current host \(in-place recovery\).

This topic is divided into the following sections:

-   [Prerequisites](#prepare_for_recovery)
-   [Recovery Scenarios](#recovery_scenarios)
-   [Post-Recovery Tasks](#post_recovery)

**Parent topic:** [Enabling High Availability and Data Consistency Features](../../highavail/topics/g-enabling-high-availability-features.html)

## <a id="prepare_for_recovery"></a>Prerequisites 

-   Mirroring is enabled for all segments.
-   You've already identified which segments have failed. If necessary, see the topic [Checking for Failed Segments](g-checking-for-failed-segments.html).
-   The coordinator host can connect to the segment host.
-   All networking or hardware issues that caused the segment to fail have been resolved.

## <a id="recovery_scenarios"></a>Recovery Scenarios 

This section documents the steps for the 3 distinct segment recovery scenarios. Follow the link to instructions that walk you through each scenario.

-   [Recover In-Place to Current Host](#same_host)
    -   [Incremental Recovery](#incremental)
    -   [Full Recovery](#full)
-   [Recover to A Different Host within the Cluster](#different_host)
-   [Recover to A New Host, Outside of the Cluster](#new_host)

### <a id="same_host"></a>Recover In-Place to Current Host 

When recovering in-place to the current host, you may choose between incremental recovery \(the default\) and full recovery.

#### <a id="incremental"></a>Incremental Recovery 

Follow these steps for incremental recovery:

1.  To recover all segments, run `gprecoverseg` with no options:

    ```
    gprecoverseg
    ```

2.  To recover a subset of segments:
    1.  Manually create a `recover_config_file` file in a location of your choice, where each segment to recover has its own line with format `failedAddress|failedPort|failedDataDirectory`

        For multiple segments, create a new line for each segment you want to recover, specifying the address, port number and data directory for each down segment. For example:

        ```
        failedAddress1|failedPort1|failedDataDirectory1
        failedAddress2|failedPort2|failedDataDirectory2
        failedAddress3|failedPort3|failedDataDirectory3
        ```

    2.  Alternatively, generate a sample recovery file using the following command; you may edit the resulting file if necessary:

        ```
        $ gprecoverseg -o /home/gpadmin/recover_config_file
        ```

    3.  Pass the `recover_config_file` to the `gprecoverseg -i` command:

        ```
        $ gprecoverseg -i /home/gpadmin/recover_config_file  
        ```

3.  Perform the post-recovery tasks summarized in the section [Post-Recovery Tasks](#post_recovery).

#### <a id="full"></a>Full Recovery 

1.  To recover all segments, run `gprecoverseg -F`:

    ```
    gprecoverseg -F
    ```

2.  To recover specific segments:
    1.  Manually create a `recover_config_file` file in a location of your choice, where each segment to recover has its own line with following format:

        ```
        failedAddress1|failedPort1|failedDataDirectory1<SPACE>failedAddress2|failedPort2|failedDataDirectory2
        ```

        Note the literal **SPACE** separating the lines.

    2.  Alternatively, generate a sample recovery file using the following command and edit the resulting file to match your desired recovery configuration:

        ```
        $ gprecoverseg -o /home/gpadmin/recover_config_file
        ```

    3.  Run the following command, passing in the config file generated in the previous step:

        ```
        $ gprecoverseg -i recover_config_file
        ```

3.  Perform the post-recovery tasks summarized in the section [Post-Recovery Tasks](#post_recovery).

### <a id="different_host"></a>Recover to A Different Host within the Cluster 

> **Note** Only full recovery is possible when recovering to a different host in the cluster.

Follow these steps to recover all segments or just a subset of segments to a different host in the cluster:

1.  Manually create a `recover_config_file` file in a location of your choice, where each segment to recover has its own line with following format:

    ```
    failedAddress|failedPort|failedDataDirectory<SPACE>newAddress|newPort|newDataDirectory
    ```

    Note the literal **SPACE** separating the details of the down segment from the details of where the segment will be recovered to.

    Alternatively, generate a sample recovery file using the following command and edit the resulting file to match your desired recovery configuration:

    ```
    $ gprecoverseg -o -p /home/gpadmin/recover_config_file
    ```

2.  Run the following command, passing in the config file generated in the previous step:

    ```
    $ gprecoverseg -i recover_config_file
    ```

3.  Perform the post-recovery tasks summarized in the section [Post-Recovery Tasks](#post_recovery).

### <a id="new_host"></a>Recover to A New Host, Outside of the Cluster 

Follow these steps if you are planning to do a hardware refresh on the host the segments are running on.

> **Note** Only full recovery is possible when recovering to a new host.

#### <a id="new_host_requirements"></a>Requirements for New Host 

The new host must:

-   have the same Greenplum Database software installed and configured as the failed host

-   have the same hardware and OS configuration as the failed host \(same hostname, OS version, OS configuration parameters applied, locales, gpadmin user account, data directory locations created, ssh keys exchanged, number of network interfaces, network interface naming convention, and so on\)

-   have sufficient disk space to accommodate the segments

-   be able to connect password-less with all other existing segments and Greenplum coordinator.


#### <a id="topic_yyj_4gb_yqb"></a>Steps to Recover to a New Host 

1.  Bring up the new host
2.  Run the following command to recover all segments to the new host:

    ```
    gprecoverseg -p <new_host_name>
    ```

    You may also specify more than one host. However, be sure you do not trigger a double-fault scenario when recovering to two hosts at a time.

    ```
    gprecoverseg -p <new_host_name1>,<new_host_name2>
    ```

    > **Note** In the case of multiple failed segment hosts, you can specify the hosts to recover to with a comma-separated list. However, it is strongly recommended to recover to one host at a time. If you must recover to more than one host at a time, then it is critical to ensure that a double fault scenario does not occur, in which both the segment primary and corresponding mirror are offline.

3.  Perform the post-recovery tasks summarized in the section [Post-Recovery Tasks](#post_recovery).

### <a id="post_recovery"></a>Post-Recovery Tasks 

Follow these steps once `gprecoverseg` has completed:

1.  Validate segement status and preferred roles:

    ```
    select * from gp_segment_configuration
    ```

2.  Monitor mirror synchronization progress:

    ```
    gpstate -e
    ```

3.  If necessary, run the following command to return segments to their preferred roles:

    ```
    gprecoverseg -r
    ```


