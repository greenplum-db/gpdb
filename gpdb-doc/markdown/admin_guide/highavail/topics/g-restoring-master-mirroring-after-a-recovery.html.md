---
title: Restoring Master Mirroring After a Recovery 
---

After you activate a standby master for recovery, the standby master becomes the primary master. You can continue running that instance as the primary master if it has the same capabilities and dependability as the original master host.

You must initialize a new standby master to continue providing master mirroring unless you have already done so while activating the prior standby master. Run [gpinitstandby](../../../utility_guide/ref/gpinitstandby.html) on the active master host to configure a new standby master. See [Enabling Master Mirroring](g-enabling-master-mirroring.html).

You can restore the primary and standby master instances on the original hosts. This process swaps the roles of the primary and standby master hosts, and it should be performed only if you strongly prefer to run the master instances on the same hosts they occupied prior to the recovery scenario.

**Important:** Restoring the primary and standby master instances to their original hosts is not an online operation. The master host must be stopped to perform the operation.

For information about the Greenplum Database utilities, see the *Greenplum Database Utility Guide*.

**Parent topic:**[Recovering a Failed Master](../../highavail/topics/g-recovering-a-failed-master.html)

## <a id="topic_us3_md4_npb"></a>To restore the master mirroring after a recovery 

1.  Ensure the original master host is in dependable running condition; ensure the cause of the original failure is fixed.
2.  On the original master host, move or remove the data directory, `gpseg-1`. This example moves the directory to `backup_gpseg-1`:

    ```
    $ mv /data/master/gpseg-1 /data/master/backup_gpseg-1
    ```

    You can remove the backup directory once the standby is successfully configured.

3.  Initialize a standby master on the original master host. For example, run this command from the current master host, smdw:

    ```
    $ gpinitstandby -s mdw
    ```

4.  After the initialization completes, check the status of standby master, mdw. Run [gpstate](../../../utility_guide/ref/gpstate.html) with the `-f` option to check the standby master status:

    ```
    $ gpstate -f
    ```

    The standby master status should be `passive`, and the WAL sender state should be `streaming`.


## <a id="topic_dr3_ld4_npb"></a>To restore the master and standby instances on original hosts \(optional\) 

**Note:** Before performing the steps in this section, be sure you have followed the steps to restore master mirroring after a recovery, as described in the [To restore the master mirroring after a recovery](#topic_us3_md4_npb)previous section.

1.  Stop the Greenplum Database master instance on the standby master. For example:

    ```
    $ gpstop -m
    ```

2.  Run the `gpactivatestandby` utility from the original master host, mdw, that is currently a standby master. For example:

    ```
    $ gpactivatestandby -d $COORDINATOR_DATA_DIRECTORY
    ```

    Where the `-d` option specifies the data directory of the host you are activating.

3.  After the utility completes, run `gpstate` with the `-b` option to display a summary of the system status:

    ```
    $ gpstate -b
    ```

    The master instance status should be `Active`. When a standby master is not configured, the command displays `No master standby configured` for the standby master state.

4.  On the standby master host, move or remove the data directory, `gpseg-1`. This example moves the directory:

    ```
    $ mv /data/master/gpseg-1 /data/master/backup_gpseg-1
    ```

    You can remove the backup directory once the standby is successfully configured.

5.  After the original master host runs the primary Greenplum Database master, you can initialize a standby master on the original standby master host. For example:

    ```
    $ gpinitstandby -s smdw
    ```

    After the command completes, you can run the `gpstate -f` command on the primary master host, to check the standby master status.


## <a id="topic_i1h_kd4_npb"></a>To check the status of the master mirroring process \(optional\) 

You can run the `gpstate` utility with the `-f` option to display details of the standby master host.

```
$ gpstate -f
```

The standby master status should be `passive`, and the WAL sender state should be `streaming`.

For information about the [gpstate](../../../utility_guide/ref/gpstate.html) utility, see the *Greenplum Database Utility Guide*.

