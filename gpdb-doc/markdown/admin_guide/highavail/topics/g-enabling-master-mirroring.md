# Enabling Master Mirroring 

You can configure a new Greenplum Database system with a standby master using `gpinitsystem` or enable it later using `gpinitstandby`. This topic assumes you are adding a standby master to an existing system that was initialized without one.

For information about the utilities `gpinitsystem` and `gpinitstandby`, see the *Greenplum Database Utility Guide*.

## To add a standby master to an existing system 

1.  Ensure the standby master host is installed and configured: `gpadmin` system user created, Greenplum Database binaries installed, environment variables set, SSH keys exchanged, and data directory created.
2.  Run the `gpinitstandby` utility on the currently active *primary* master host to add a standby master host to your Greenplum Database system. For example:

    ```
    $ gpinitstandby -s smdw
    ```

    Where `-s` specifies the standby master host name.

3.  To switch operations to a standby master, see [Recovering a Failed Master](g-recovering-a-failed-master.html).

You can display the information in the Greenplum Database system view pg\_stat\_replication. The view lists information about the `walsender` process that is used for Greenplum Database master mirroring. For example, this command displays the process ID and state of the `walsender` process:

```
$ psql dbname -c 'SELECT procpid, state FROM pg_stat_replication;'
```

For information about the `pg_stat_replication` system view, see the *Greenplum Database Reference Guide*.

**Parent topic:** [Enabling Mirroring in Greenplum Database](../../highavail/topics/g-enabling-mirroring-in-greenplum-database.html)

