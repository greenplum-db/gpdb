---
title: Recovering a Failed Coordinator 
---

If the primary coordinator fails, the Greenplum Database system is not accessible and WAL replication stops. Use [gpactivatestandby](../../../utility_guide/ref/gpactivatestandby.html) to activate the standby coordinator. Upon activation of the standby coordinator, Greenplum Database reconstructs the coordinator host state at the time of the last successfully committed transaction.

These steps assume a standby coordinator host is configured for the system. See [Enabling Coordinator Mirroring](g-enabling-master-mirroring.html).

## <a id="ki181117"></a>To activate the standby coordinator 

1.  Run the `gpactivatestandby` utility from the standby coordinator host you are activating. For example:

    ```
    $ export PGPORT=5432
    $ gpactivatestandby -d /data/coordinator/gpseg-1
    ```

    Where `-d` specifies the data directory of the coordinator host you are activating.

    After you activate the standby, it becomes the *active* or *primary* coordinator for your Greenplum Database array.

2.  After the utility completes, run `gpstate` with the `-b` option to display a summary of the system status:

    ```
    $ gpstate -b
    ```

    The coordinator instance status should be `Active`. When a standby coordinator is not configured, the command displays `No coordinator standby configured` for the standby coordinator status. If you configured a new standby coordinator, its status is `Passive`.

3.  Optional: If you have not already done so while activating the prior standby coordinator, you can run `gpinitstandby` on the active coordinator host to configure a new standby coordinator.

    > **Important** You must initialize a new standby coordinator to continue providing coordinator mirroring.

    For information about restoring the original coordinator and standby coordinator configuration, see [Restoring Coordinator Mirroring After a Recovery](g-restoring-master-mirroring-after-a-recovery.html).


-   **[Restoring Coordinator Mirroring After a Recovery](../../highavail/topics/g-restoring-master-mirroring-after-a-recovery.html)**  


**Parent topic:** [Enabling High Availability and Data Consistency Features](../../highavail/topics/g-enabling-high-availability-features.html)

