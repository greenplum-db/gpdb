# When a segment host is not recoverable 

If a host is nonoperational, for example, due to hardware failure, recover the segments onto a spare set of hardware resources. If mirroring is enabled, you can recover a segment from its mirror onto an alternate host using `gprecoverseg`. For example:

```
$ gprecoverseg -i <recover_config_file>
      
```

Where the format of `<recover_config_file>` is:

```
filespaceOrder=[<filespace1_name>[:<filespace2_name>:...]<failed_host_address>:
<port>:<fselocation> [<recovery_host_address>:<port>:<replication_port>:<fselocation>
[:<fselocation>:...]]
```

For example, to recover to a different host than the failed host without additional filespaces configured \(besides the default *pg\_system* filespace\):

```
filespaceOrder=sdw5-2:50002:/gpdata/gpseg2 sdw9-2:50002:53002:/gpdata/gpseg2
```

The *gp\_segment\_configuration* and *pg\_filespace\_entry* system catalog tables can help determine your current segment configuration so you can plan your mirror recovery configuration. For example, run the following query:

```
=# SELECT dbid, content, hostname, address, port, 
   replication_port, fselocation as datadir 
   FROM gp_segment_configuration, pg_filespace_entry 
   WHERE dbid=fsedbid 
   ORDER BY dbid;
```

The new recovery segment host must be pre-installed with the Greenplum Database software and configured exactly as the existing segment hosts.

**Parent topic:** [Recovering From Segment Failures](../../highavail/topics/g-recovering-from-segment-failures.html)

