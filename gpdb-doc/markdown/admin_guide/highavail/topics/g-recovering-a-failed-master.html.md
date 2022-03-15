---
title: Recovering a Failed Master 
---

If the primary master fails, the Greenplum Database system is not accessible and WAL replication stops. Use [gpactivatestandby](../../../utility_guide/ref/gpactivatestandby.html) to activate the standby master. Upon activation of the standby master, Greenplum Database reconstructs the master host state at the time of the last successfully committed transaction.

These steps assume a standby master host is configured for the system. See [Enabling Master Mirroring](g-enabling-master-mirroring.html).

## <a id="ki181117"></a>To activate the standby master 

1.  Run the `gpactivatestandby` utility from the standby master host you are activating. For example:

    ```
    $ export PGPORT=5432
    $ gpactivatestandby -d /data/master/gpseg-1
    ```

    Where `-d` specifies the data directory of the master host you are activating.

    After you activate the standby, it becomes the *active* or *primary* master for your Greenplum Database array.

2.  After the utility completes, run `gpstate` with the `-b` option to display a summary of the system status:

    ```
    $ gpstate -b
    ```

    The master instance status should be `Active`. When a standby master is not configured, the command displays `No master standby configured` for the standby master status. If you configured a new standby master, its status is `Passive`.

3.  Optional: If you have not already done so while activating the prior standby master, you can run `gpinitstandby` on the active master host to configure a new standby master.

    **Important:** You must initialize a new standby master to continue providing master mirroring.

    For information about restoring the original master and standby master configuration, see [Restoring Master Mirroring After a Recovery](g-restoring-master-mirroring-after-a-recovery.html).


-   **[Restoring Master Mirroring After a Recovery](../../highavail/topics/g-restoring-master-mirroring-after-a-recovery.html)**  


**Parent topic:**[Enabling High Availability and Data Consistency Features](../../highavail/topics/g-enabling-high-availability-features.html)

