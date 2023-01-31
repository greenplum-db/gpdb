---
title: Understanding Segment Recovery 
---

This topic provides background information about concepts and principles of segment recovery. If you have down segments and need immediate help recovering them, see the instructions in [Recovering from Segment Failures](g-recovering-from-segment-failures.html). For information on how Greenplum Database detects that segments are down and an explanation of the Fault Tolerance Server \(FTS\) that manages down segment tracking, see [How Greenplum Database Detects a Failed Segment](g-detecting-a-failed-segment.html).

This topic is divided into the following sections:

-   [Segment Recovery Basics](#recovery_basics)
-   [Segment Recovery: Flow of Events](#flow_of_events)
-   [Simple Failover and Recovery Example](#simple_example)
-   [Incremental versus Full Recovery](#incremental_vs_full)

**Parent topic:** [Enabling High Availability and Data Consistency Features](../../highavail/topics/g-enabling-high-availability-features.html)

## <a id="recovery_basics"></a>Segment Recovery Basics 

If the master cannot connect to a segment instance, it marks that segment as down in the Greenplum Database `gp_segment_configuration` table. The segment instance remains offline until an administrator takes steps to bring the segment back online. The process for recovering a down segment instance or host depends on the cause of the failure and on whether or not mirroring is enabled. A segment instance can be marked as down for a number of reasons:

-   A segment host is unavailable; for example, due to network or hardware failures.
-   A segment instance is not running; for example, there is no `postgres` database listener process.
-   The data directory of the segment instance is corrupt or missing; for example, data is not accessible, the file system is corrupt, or there is a disk failure.

In order to bring the down segment instance back into operation again, you must correct the problem that made it fail in the first place, and then – if you have mirroring enabled – you can attempt to recover the segment instance from its mirror using the `gprecoverseg` utility.

## <a id="flow_of_events"></a>Segment Recovery: Flow of Events 

**When a Primary Segment Goes Down**

The following summarizes the flow of events that follow a **primary** segment going down:

1.  A primary segment goes down.
2.  The Fault Tolerance Server \(FTS\) detects this and marks the segment as down in the `gp_segment_configuration` table.
3.  The mirror segment is promoted to primary and starts functioning as primary. The previous primary is demoted to mirror.
4.  The user fixes the underlying problem.
5.  The user runs `gprecoverseg` to bring back the \(formerly primary\) mirror segment.
6.  The WAL synchronization process ensures that the mirror segment data is synchronized with the primary segment data. Users can check the state of this synching with `gpstate -e`.
7.  Greenplum Database marks the segments as up \(`u`\) in the `gp_segment_configuration` table.
8.  If segments are not in their preferred roles, user runs `gprecoverseg -r` to restore them to their preferred roles.

**When a Mirror Segment Goes Down**

The following summarizes the flow of events that follow a **mirror** segment going down:

1.  A mirror segment goes down.
2.  The Fault Tolerance Server \(FTS\) detects this and marks the segment as down in the `gp_segment_configuration` table.
3.  The user fixes the underlying problem.
4.  The user runs `gprecoverseg` to bring back the \(formerly mirror\) mirror segment.
5.  The synching process occurs: the mirror comes into sync with its primary via WAL synching. You can check the state of this synching with `gpstate -e`.

## <a id="rebalancing"></a>Rebalancing After Recovery 

After a segment instance has been recovered, the segments may not be in their preferred roles, which can cause processing to be skewed. The `gp_segment_configuration` table has the columns `role` \(current role\) and `preferred_role` \(original role at the beginning\). When a segment's `role` and `preferred_role` do not match the system may not be balanced. To rebalance the cluster and bring all the segments into their preferred roles, run the `gprecoverseg -r`command.

## <a id="simple_example"></a>Simple Failover and Recovery Example 

Consider a single primary-mirror segment instance pair where the primary segment has failed over to the mirror. The following table shows the segment instance preferred role, role, mode, and status from the `gp_segment_configuration` table before beginning recovery of the failed primary segment.

You can also run `gpstate -e` to display any issues with a primary or mirror segment instances.

| Segment Type |`preferred_role`|`role`|`mode`|`status`|
|--------------|----------------|------|------|--------|
|Primary|`p`\(primary\)|`m`\(mirror\)|`n`\(Not In Sync\)|`d`\(down\)|
|Mirror|`m`\(mirror\)|`p`\(primary\)|`n`\(Not In Sync\)|`u`\(up\)|

The primary segment is down and segment instances are not in their preferred roles. The mirror segment is up and its role is now primary. However, it is not synchronized with its mirror \(the former primary segment\) because that segment is down. You must potentially fix either issues with the host the down segment is running on, issues with the segment instance itself, or both. You then use `gprecoverseg` to prepare failed segment instances for recovery and initiate synchronization between the primary and mirror instances.

After `gprecoverseg` has completed, the segments are in the states shown in the following table where the primary-mirror segment pair is up with the primary and mirror roles reversed from their preferred roles.

> **Note** There might be a lag between when `gprecoverseg` completes and when the segment status is set to `u` \(up\).

| Segment Type |`preferred_role`|`role`|`mode`|`status`|
|--------------|----------------|------|------|--------|
|Primary|`p`\(primary\)|`m`\(mirror\)|`s`\(Synchronized\)|`u`\(up\)|
|Mirror|`m`\(mirror\)`p`\(primary\)|`s`\(Synchronized\)|`u`\(up\)

|

The `gprecoverseg -r` command rebalances the system by returning the segment roles to their preferred roles.

| Segment Type |`preferred_role`|`role`|`mode`|`status`|
|--------------|----------------|------|------|--------|
|Primary|`p`\(primary\)|`p`\(primary\)|`s`\(Synchronized\)|`u`\(up\)|
|Mirror|`m`\(mirror\)|`m`\(mirror\)|`s`\(Synchronized\)|`u`\(up\)|

## <a id="incremental_vs_full"></a>Incremental versus Full Recovery 

Greenplum database can perform two types of recovery: incremental or full. The default is incremental.

By default, `gprecoverseg` performs an incremental recovery, placing the mirror into *Synchronizing* mode, which starts to replay the recorded changes from the primary onto the mirror. If the incremental recovery cannot be completed, the recovery fails and you should run `gprecoverseg` again with the `-F` option, to perform full recovery. This causes the primary to copy all of its data to the mirror.

> **Note** After a failed incremental recovery attempt you must perform a full recovery.

Whenever possible, you should perform an incremental recovery rather than a full recovery, as incremental recovery is substantially faster.

For a more detailed explanation of the differences between incremental and full recovery, see the article ["VMware Greenplum 6's gprecoverseg explained"](https://community.pivotal.io/s/article/5004y00001YA9fI1617805667833?language=en_US) in the VMware Support Hub.

