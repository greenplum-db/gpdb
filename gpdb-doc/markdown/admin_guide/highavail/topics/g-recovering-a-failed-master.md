# Recovering a Failed Master 

If the primary master fails, log replication stops. Use the `gpstate -f` command to check the state of standby replication. Use `gpactivatestandby` to activate the standby master. Upon activation of the standby master, Greenplum Database reconstructs the master host state at the time of the last successfully committed transaction.

## To activate the standby master 

1.  Ensure a standby master host is configured for the system. See [Enabling Master Mirroring](g-enabling-master-mirroring.html).
2.  Run the `gpactivatestandby` utility from the standby master host you are activating. For example:

    ```
    $ gpactivatestandby -d /data/master/gpseg-1
    ```

    Where `-d` specifies the data directory of the master host you are activating.

    After you activate the standby, it becomes the *active* or *primary* master for your Greenplum Database array.

3.  After the utility finishes, run `gpstate` to check the status:

    ```
    $ gpstate -f
    ```

    The newly activated master's status should be *Active*. If you configured a new standby host, its status is *Passive*. When a standby master is not configured, the command displays `-No entries found` and the message indicates that a standby master instance is not configured.

4.  Optional: If you did not specify a new standby host when running the `gpactivatestandby` utility, use `gpinitstandby` to configure a new standby master at a later time. Run `gpinitstandby` on your active master host. For example:

    ```
    $ gpinitstandby -s <new_standby_master_hostname>
                   
    ```


-   **[Restoring Master Mirroring After a Recovery](../../highavail/topics/g-restoring-master-mirroring-after-a-recovery.html)**  


**Parent topic:** [Enabling High Availability and Data Consistency Features](../../highavail/topics/g-enabling-high-availability-features.html)

