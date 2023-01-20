---
title: Enabling Coordinator Mirroring 
---

You can configure a new Greenplum Database system with a standby coordinator using `gpinitsystem` or enable it later using `gpinitstandby`. This topic assumes you are adding a standby coordinator to an existing system that was initialized without one.

For information about the utilities [gpinitsystem](../../../utility_guide/ref/gpinitsystem.html) and [gpinitstandby](../../../utility_guide/ref/gpinitstandby.html), see the *Greenplum Database Utility Guide*.

## <a id="ki160203"></a>To add a standby coordinator to an existing system 

1.  Ensure the standby coordinator host is installed and configured: `gpadmin` system user created, Greenplum Database binaries installed, environment variables set, SSH keys exchanged, and that the data directories and tablespace directories, if needed, are created.
2.  Run the `gpinitstandby` utility on the currently active *primary* coordinator host to add a standby coordinator host to your Greenplum Database system. For example:

    ```
    $ gpinitstandby -s scdw
    ```

    Where `-s` specifies the standby coordinator host name.


To switch operations to a standby coordinator, see [Recovering a Failed Coordinator](g-recovering-a-failed-master.html).

## <a id="tocheck"></a>To check the status of the coordinator mirroring process \(optional\) 

You can run the `gpstate` utility with the `-f` option to display details of the standby coordinator host.

```
$ gpstate -f
```

The standby coordinator status should be passive, and the WAL sender state should be streaming.

For information about the [gpstate](../../../utility_guide/ref/gpstate.html) utility, see the *Greenplum Database Utility Guide*.

**Parent topic:** [Enabling Mirroring in Greenplum Database](../../highavail/topics/g-enabling-mirroring-in-greenplum-database.html)

