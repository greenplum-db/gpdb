# Restoring Master Mirroring After a Recovery 

After you activate a standby master for recovery, the standby master becomes the primary master. You can continue running that instance as the primary master if it has the same capabilities and dependability as the original master host.

You must initialize a new standby master to continue providing master mirroring unless you have already done so while activating the prior standby master. Run `gpinitstandby` on the active master host to configure a new standby master.

You may restore the primary and standby master instances on the original hosts. This process swaps the roles of the primary and standby master hosts, and it should be performed only if you strongly prefer to run the master instances on the same hosts they occupied prior to the recovery scenario.

For information about the Greenplum Database utilities, see the *Greenplum Database Utility Guide*.

## To restore the master and standby instances on original hosts \(optional\) 

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

4.  After the initialization completes, check the status of standby master, mdw, run `gpstate` with the `-f` option to check the status:

    ```
    $ gpstate -f
    ```

    The status should be *In Synch*.

5.  Stop Greenplum Database master instance on the standby master. For example:

    ```
    $ gpstop -m
    ```

6.  Run the `gpactivatestandby` utility from the original master host, mdw, that is currently a standby master. For example:

    ```
    $ gpactivatestandby -d $MASTER_DATA_DIRECTORY
    ```

    Where the `-d` option specifies the data directory of the host you are activating.

7.  After the utility completes, run `gpstate` to check the status:

    ```
    $ gpstate -f 
    ```

    Verify the original primary master status is *Active*. When a standby master is not configured, the command displays `-No entries found` and the message indicates that a standby master instance is not configured.

8.  On the standby master host, move or remove the data directory, `gpseg-1`. This example moves the directory:

    ```
    $ mv /data/master/gpseg-1 /data/master/backup_gpseg-1
    ```

    You can remove the backup directory once the standby is successfully configured.

9.  After the original master host runs the primary Greenplum Database master, you can initialize a standby master on the original standby master host. For example:

    ```
    $ gpinitstandby -s smdw
    ```


You can display the information in the Greenplum Database system view pg\_stat\_replication. The view lists information about the `walsender` process that is used for Greenplum Database master mirroring. For example, this command displays the process ID and state of the `walsender` process:

```
$ psql dbname -c 'SELECT procpid, state FROM pg_stat_replication;'
```

**Parent topic:** [Recovering a Failed Master](../../highavail/topics/g-recovering-a-failed-master.html)

