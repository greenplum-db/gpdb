# Parameter Categories 

Configuration parameters affect categories of server behaviors, such as resource consumption, query tuning, and authentication. The following topics describe Greenplum configuration parameter categories.

-   [Connection and Authentication Parameters](#topic12)
-   [System Resource Consumption Parameters](#topic15)
-   [GPORCA Parameters](#topic57)
-   [Query Tuning Parameters](#topic21)
-   [Error Reporting and Logging Parameters](#topic29)
-   [System Monitoring Parameters](#topic33)
-   [Runtime Statistics Collection Parameters](#topic37)
-   [Automatic Statistics Collection Parameters](#topic38)
-   [Client Connection Default Parameters](#topic39)
-   [Lock Management Parameters](#topic43)
-   [Resource Management Parameters \(Resource Queues\)](#topic44)
-   [Resource Management Parameters \(Resource Groups\)](#topic444)
-   [External Table Parameters](#topic45)
-   [Database Table Parameters](#topic46)
-   [Database and Tablespace/Filespace Parameters](#topic47)
-   [Past Version Compatibility Parameters](#topic48)
-   [Greenplum Database Array Configuration Parameters](#topic49)
-   [Greenplum Mirroring Parameters for Master and Segments](#topic55)
-   [Greenplum Database Extension Parameters](#topic56)

## Connection and Authentication Parameters 

These parameters control how clients connect and authenticate to Greenplum Database.

### Connection Parameters 

|[gp\_connection\_send\_timeout](guc-list.html)

 [gp\_vmem\_idle\_resource\_timeout](guc-list.html)

 [listen\_addresses](guc-list.html)

 [max\_connections](guc-list.html)

 [max\_prepared\_transactions](guc-list.html)

 [superuser\_reserved\_connections](guc-list.html)

|[tcp\_keepalives\_count](guc-list.html)

 [tcp\_keepalives\_idle](guc-list.html)

 [tcp\_keepalives\_interval](guc-list.html)

 [unix\_socket\_directory](guc-list.html)

 [unix\_socket\_group](guc-list.html)

 [unix\_socket\_permissions](guc-list.html)

|

### Security and Authentication Parameters 

|[authentication\_timeout](guc-list.html)

 [db\_user\_namespace](guc-list.html)

 [krb\_caseins\_users](guc-list.html)

 [krb\_server\_keyfile](guc-list.html)

|[krb\_srvname](guc-list.html)

 [password\_encryption](guc-list.html)

 [password\_hash\_algorithm](guc-list.html)

 [ssl](guc-list.html)

 [ssl\_ciphers](guc-list.html)

|

## System Resource Consumption Parameters 

These parameters set the limits for system resources consumed by Greenplum Database.

### Memory Consumption Parameters 

These parameters control system memory usage.

|[gp\_vmem\_idle\_resource\_timeout](guc-list.html)

 [gp\_resource\_group\_memory\_limit](guc-list.html) \(resource group-based resource management\)

 [gp\_vmem\_protect\_limit](guc-list.html) \(resource queue-based resource management\)

 [gp\_vmem\_protect\_segworker\_cache\_limit](guc-list.html)

 [gp\_workfile\_limit\_files\_per\_query](guc-list.html)

|[gp\_workfile\_limit\_per\_query](guc-list.html)

 [gp\_workfile\_limit\_per\_segment](guc-list.html)

 [max\_stack\_depth](guc-list.html)

 [shared\_buffers](guc-list.html)

 [temp\_buffers](guc-list.html)

|

### Free Space Map Parameters 

These parameters control the sizing of the *free space map, which contains*expired rows. Use `VACUUM` to reclaim the free space map disk space.

|[max\_fsm\_pages](guc-list.html)

 [max\_fsm\_relations](guc-list.html)

|

### OS Resource Parameters 

|[max\_files\_per\_process](guc-list.html)

 [shared\_preload\_libraries](guc-list.html)

|

### Cost-Based Vacuum Delay Parameters 

**Warning:** Do not use cost-based vacuum delay because it runs asynchronously among the segment instances. The vacuum cost limit and delay is invoked at the segment level without taking into account the state of the entire Greenplum Database array

You can configure the execution cost of `VACUUM` and `ANALYZE` commands to reduce the I/O impact on concurrent database activity. When the accumulated cost of I/O operations reaches the limit, the process performing the operation sleeps for a while, Then resets the counter and continues execution

|[vacuum\_cost\_delay](guc-list.html)

 [vacuum\_cost\_limit](guc-list.html)

 [vacuum\_cost\_page\_dirty](guc-list.html)

|[vacuum\_cost\_page\_hit](guc-list.html)

 [vacuum\_cost\_page\_miss](guc-list.html)

|

### Transaction ID Management Parameters 

|[xid\_stop\_limit](guc-list.html)

 [xid\_warn\_limit](guc-list.html)

|

## GPORCA Parameters 

These parameters control the usage of GPORCA by Greenplum Database. For information about GPORCA, see [About GPORCA](../../admin_guide/query/topics/query-piv-optimizer.html)in the *Greenplum Database Administrator Guide*.

|[gp\_enable\_relsize\_collection](guc-list.html)

 [optimizer](guc-list.html)

 [optimizer\_analyze\_root\_partition](guc-list.html)

 [optimizer\_array\_expansion\_threshold](guc-list.html)

 [optimizer\_control](guc-list.html)

 [optimizer\_cost\_model](guc-list.html)

 [optimizer\_cte\_inlining\_bound](guc-list.html)

 [optimizer\_dpe\_stats](guc-list.html)

 [optimizer\_enable\_associativity](guc-list.html)

 [optimizer\_enable\_dml](guc-list.html)

 [optimizer\_enable\_master\_only\_queries](guc-list.html)

 [optimizer\_force\_agg\_skew\_avoidance](guc-list.html)

|[optimizer\_force\_multistage\_agg](guc-list.html)

 [optimizer\_force\_three\_stage\_scalar\_dqa](guc-list.html)

 [optimizer\_join\_arity\_for\_associativity\_commutativity](guc-list.html)

 [optimizer\_join\_order](guc-list.html)

 [optimizer\_join\_order\_threshold](guc-list.html)

 [optimizer\_mdcache\_size](guc-list.html)

 [optimizer\_metadata\_caching](guc-list.html)

 [optimizer\_parallel\_union](guc-list.html)

 [optimizer\_penalize\_skew](guc-list.html)

 [optimizer\_print\_missing\_stats](guc-list.html)

 [optimizer\_print\_optimization\_stats](guc-list.html)

 [optimizer\_sort\_factor](guc-list.html)

 [optimizer\_use\_gpdb\_allocators](guc-list.html)

 [optimizer\_xform\_bind\_threshold](guc-list.html)

|

## Query Tuning Parameters 

These parameters control aspects of SQL query processing such as query operators and operator settings and statistics sampling.

### Legacy Query Optimizer Operator Control Parameters 

The following parameters control the types of plan operations the Postgres Planner can use. Enable or disable plan operations to force the Postgres Planner to choose a different plan. This is useful for testing and comparing query performance using different plan types.

|[enable\_bitmapscan](guc-list.html)

 [enable\_groupagg](guc-list.html)

 [enable\_hashagg](guc-list.html)

 [enable\_hashjoin](guc-list.html)

 [enable\_indexscan](guc-list.html)

 [enable\_mergejoin](guc-list.html)

 [enable\_nestloop](guc-list.html)

 [enable\_seqscan](guc-list.html)

 [enable\_sort](guc-list.html)

 [enable\_tidscan](guc-list.html)

 [gp\_enable\_agg\_distinct](guc-list.html)

|[gp\_enable\_agg\_distinct\_pruning](guc-list.html)

 [gp\_enable\_direct\_dispatch](guc-list.html)

 [gp\_enable\_fallback\_plan](guc-list.html)

 [gp\_enable\_fast\_sri](guc-list.html)

 [gp\_enable\_groupext\_distinct\_gather](guc-list.html)

 [gp\_enable\_groupext\_distinct\_pruning](guc-list.html)

 [gp\_enable\_multiphase\_agg](guc-list.html)

 [gp\_enable\_predicate\_propagation](guc-list.html)

 [gp\_enable\_preunique](guc-list.html)

 [gp\_enable\_relsize\_collection](guc-list.html)

 [gp\_enable\_sequential\_window\_plans](guc-list.html)

 [gp\_enable\_sort\_distinct](guc-list.html)

 [gp\_enable\_sort\_limit](guc-list.html)

|

### Legacy Query Optimizer Costing Parameters 

**Warning:** Do not adjust these query costing parameters. They are tuned to reflect Greenplum Database hardware configurations and typical workloads. All of these parameters are related. Changing one without changing the others can have adverse affects on performance.

|[cpu\_index\_tuple\_cost](guc-list.html)

 [cpu\_operator\_cost](guc-list.html)

 [cpu\_tuple\_cost](guc-list.html)

 [cursor\_tuple\_fraction](guc-list.html)

 [effective\_cache\_size](guc-list.html)

|[gp\_motion\_cost\_per\_row](guc-list.html)

 [gp\_segments\_for\_planner](guc-list.html)

 [random\_page\_cost](guc-list.html)

 [seq\_page\_cost](guc-list.html)

|

### Database Statistics Sampling Parameters 

These parameters adjust the amount of data sampled by an `ANALYZE` operation. Adjusting these parameters affects statistics collection system-wide. You can configure statistics collection on particular tables and columns by using the `ALTER TABLE``SET STATISTICS` clause.

|[default\_statistics\_target](guc-list.html)

 [gp\_analyze\_relative\_error](guc-list.html)

|

### Sort Operator Configuration Parameters 

|[gp\_enable\_sort\_distinct](guc-list.html)

 [gp\_enable\_sort\_limit](guc-list.html)

|

### Aggregate Operator Configuration Parameters 

|[gp\_enable\_agg\_distinct](guc-list.html)

 [gp\_enable\_agg\_distinct\_pruning](guc-list.html)

 [gp\_enable\_multiphase\_agg](guc-list.html)

 [gp\_enable\_preunique](guc-list.html)

|[gp\_enable\_groupext\_distinct\_gather](guc-list.html)

 [gp\_enable\_groupext\_distinct\_pruning](guc-list.html)

 [gp\_workfile\_compress\_algorithm](guc-list.html)

|

### Join Operator Configuration Parameters 

|[join\_collapse\_limit](guc-list.html)

 [gp\_adjust\_selectivity\_for\_outerjoins](guc-list.html)

 [gp\_hashjoin\_tuples\_per\_bucket](guc-list.html)

|[gp\_statistics\_use\_fkeys](guc-list.html)

 [gp\_workfile\_compress\_algorithm](guc-list.html)

|

### Other Legacy Query Optimizer Configuration Parameters 

|[from\_collapse\_limit](guc-list.html)

 [gp\_enable\_predicate\_propagation](guc-list.html)

 [gp\_max\_plan\_size](guc-list.html)

 [gp\_statistics\_pullup\_from\_child\_partition](guc-list.html)

|

### Query Plan Execution 

Control the query plan execution.

|[gp\_max\_slices](guc-list.html)

|

## Error Reporting and Logging Parameters 

These configuration parameters control Greenplum Database logging.

### Log Rotation 

|[log\_rotation\_age](guc-list.html)

 [log\_rotation\_size](guc-list.html)

|[log\_truncate\_on\_rotation](guc-list.html)

|

### When to Log 

|[client\_min\_messages](guc-list.html)

 [gp\_interconnect\_debug\_retry\_interval](guc-list.html)

 [log\_error\_verbosity](guc-list.html)

 [log\_min\_duration\_statement](guc-list.html)

|[log\_min\_error\_statement](guc-list.html)

 [log\_min\_messages](guc-list.html)

 [optimizer\_minidump](guc-list.html)

|

### What to Log 

|[debug\_pretty\_print](guc-list.html)

 [debug\_print\_parse](guc-list.html)

 [debug\_print\_plan](guc-list.html)

 [debug\_print\_prelim\_plan](guc-list.html)

 [debug\_print\_rewritten](guc-list.html)

 [debug\_print\_slice\_table](guc-list.html)

 [log\_autostats](guc-list.html)

 [log\_connections](guc-list.html)

 [log\_disconnections](guc-list.html)

 [log\_dispatch\_stats](guc-list.html)

 [log\_duration](guc-list.html)

|[log\_executor\_stats](guc-list.html)

 [log\_hostname](guc-list.html)

 [gp\_log\_interconnect](guc-list.html)

 [log\_parser\_stats](guc-list.html)

 [log\_planner\_stats](guc-list.html)

 [log\_statement](guc-list.html)

 [log\_statement\_stats](guc-list.html)

 [log\_timezone](guc-list.html)

 [gp\_debug\_linger](guc-list.html)

 [gp\_log\_format](guc-list.html)

 [gp\_max\_csv\_line\_length](guc-list.html)

 [gp\_reraise\_signal](guc-list.html)

|

## System Monitoring Parameters 

These configuration parameters control Greenplum Database data collection and notifications related to database monitoring.

### SNMP Alerts 

The following parameters send SNMP notifications when events occur.

|[gp\_snmp\_community](guc-list.html)

 [gp\_snmp\_monitor\_address](guc-list.html)

|[gp\_snmp\_use\_inform\_or\_trap](guc-list.html)

|

### Email Alerts 

The following parameters configure the system to send email alerts for fatal error events, such as a segment going down or a server crash and reset.

|[gp\_email\_from](guc-list.html)

 [gp\_email\_smtp\_password](guc-list.html)

 [gp\_email\_smtp\_server](guc-list.html)

|[gp\_email\_smtp\_userid](guc-list.html)

 [gp\_email\_to](guc-list.html)

|

### Greenplum Command Center Agent 

The following parameters configure the data collection agents that populate the `gpperfmon` database for Greenplum Command Center.

|[gp\_enable\_gpperfmon](guc-list.html)

 [gp\_gpperfmon\_send\_interval](guc-list.html)

|[gpperfmon\_log\_alert\_level](guc-list.html)

 [gpperfmon\_port](guc-list.html)

|

## Runtime Statistics Collection Parameters 

These parameters control the server statistics collection feature. When statistics collection is enabled, you can access the statistics data using the *pg\_stat* family of system catalog views.

|[stats\_queue\_level](guc-list.html)

 [track\_activities](guc-list.html)

|[track\_counts](guc-list.html)

 [update\_process\_title](guc-list.html)

|

## Automatic Statistics Collection Parameters 

When automatic statistics collection is enabled, you can run `ANALYZE` automatically in the same transaction as an `INSERT`, `UPDATE`, `DELETE`, `COPY` or `CREATE TABLE...AS SELECT` statement when a certain threshold of rows is affected \(`on_change`\), or when a newly generated table has no statistics \(`on_no_stats`\). To enable this feature, set the following server configuration parameters in your Greenplum Database master `postgresql.conf` file and restart Greenplum Database:

|[gp\_autostats\_mode](guc-list.html)

 [gp\_autostats\_mode\_in\_functions](guc-list.html)

 [gp\_autostats\_on\_change\_threshold](guc-list.html)

 [log\_autostats](guc-list.html)

|

**Warning:** Depending on the specific nature of your database operations, automatic statistics collection can have a negative performance impact. Carefully evaluate whether the default setting of `on_no_stats` is appropriate for your system.

## Client Connection Default Parameters 

These configuration parameters set defaults that are used for client connections.

### Statement Behavior Parameters 

|[check\_function\_bodies](guc-list.html)

 [default\_tablespace](guc-list.html)

 [default\_transaction\_isolation](guc-list.html)

 [default\_transaction\_read\_only](guc-list.html)

|[search\_path](guc-list.html)

 [statement\_timeout](guc-list.html)

 [vacuum\_freeze\_min\_age](guc-list.html)

|

### Locale and Formatting Parameters 

|[client\_encoding](guc-list.html)

 [DateStyle](guc-list.html)

 [extra\_float\_digits](guc-list.html)

 [IntervalStyle](guc-list.html)

 [lc\_collate](guc-list.html)

 [lc\_ctype](guc-list.html)

|[lc\_messages](guc-list.html)

 [lc\_monetary](guc-list.html)

 [lc\_numeric](guc-list.html)

 [lc\_time](guc-list.html)

 [TimeZone](guc-list.html)

|

### Other Client Default Parameters 

|[dynamic\_library\_path](guc-list.html)

 [explain\_pretty\_print](guc-list.html)

|[local\_preload\_libraries](guc-list.html)

|

## Lock Management Parameters 

These configuration parameters set limits for locks and deadlocks.

|[deadlock\_timeout](guc-list.html)

 [max\_locks\_per\_transaction](guc-list.html)

|

## Resource Management Parameters \(Resource Queues\) 

The following configuration parameters configure the Greenplum Database resource management feature \(resource queues\), query prioritization, memory utilization and concurrency control.

|[gp\_resqueue\_memory\_policy](guc-list.html)

 [gp\_resqueue\_priority](guc-list.html)

 [gp\_resqueue\_priority\_cpucores\_per\_segment](guc-list.html)

 [gp\_resqueue\_priority\_sweeper\_interval](guc-list.html)

 [gp\_vmem\_idle\_resource\_timeout](guc-list.html)

 [gp\_vmem\_protect\_limit](guc-list.html)

 [gp\_vmem\_protect\_segworker\_cache\_limit](guc-list.html)

 [max\_resource\_queues](guc-list.html)

|[max\_resource\_portals\_per\_transaction](guc-list.html)

 [max\_statement\_mem](guc-list.html)

 [resource\_cleanup\_gangs\_on\_wait](guc-list.html)

 [resource\_select\_only](guc-list.html)

 [runaway\_detector\_activation\_percent](guc-list.html)

 [statement\_mem](guc-list.html)

 [stats\_queue\_level](guc-list.html)

 [vmem\_process\_interrupt](guc-list.html)

|

## Resource Management Parameters \(Resource Groups\) 

The following parameters configure the Greenplum Database resource group workload management feature.

|[gp\_resgroup\_memory\_policy](guc-list.html)

 [gp\_resource\_group\_bypass](guc-list.html) [gp\_resource\_group\_cpu\_limit](guc-list.html)

[gp\_resource\_group\_memory\_limit](guc-list.html)

[gp\_resource\_manager](guc-list.html)

|[gp\_vmem\_idle\_resource\_timeout](guc-list.html)

 [gp\_vmem\_protect\_segworker\_cache\_limit](guc-list.html)

 [max\_statement\_mem](guc-list.html)

 [memory\_spill\_ratio](guc-list.html)

 [runaway\_detector\_activation\_percent](guc-list.html)

 [statement\_mem](guc-list.html)

 [vmem\_process\_interrupt](guc-list.html)

|

## External Table Parameters 

The following parameters configure the external tables feature of Greenplum Database.

|[gp\_external\_enable\_exec](guc-list.html)

 [gp\_external\_enable\_filter\_pushdown](guc-list.html)

 [gp\_external\_max\_segs](guc-list.html)

 [gp\_initial\_bad\_row\_limit](guc-list.html)

 [gp\_reject\_percent\_threshold](guc-list.html)

|[readable\_external\_table\_timeout](guc-list.html)

 [writable\_external\_table\_bufsize](guc-list.html)

 [verify\_gpfdists\_cert](guc-list.html)

|

## Database Table Parameters 

The following parameter configures default option settings for Greenplum Database tables.

|[gp\_create\_table\_random\_default\_distribution](guc-list.html)

 [gp\_default\_storage\_options](guc-list.html)

 [gp\_enable\_exchange\_default\_partition](guc-list.html)

 [gp\_enable\_segment\_copy\_checking](guc-list.html)

|

### Append-Optimized Table Parameters 

The following parameters configure the append-optimized tables feature of Greenplum Database.

|[max\_appendonly\_tables](guc-list.html)

 [gp\_appendonly\_compaction](guc-list.html)

 [gp\_appendonly\_compaction\_threshold](guc-list.html)

 [validate\_previous\_free\_tid](guc-list.html)

|

## Database and Tablespace/Filespace Parameters 

The following parameters configure the maximum number of databases, tablespaces, and filespaces allowed in a system.

|[gp\_max\_tablespaces](guc-list.html)

 [gp\_max\_filespaces](guc-list.html)

 [gp\_max\_databases](guc-list.html)

|

## Past Version Compatibility Parameters 

The following parameters provide compatibility with older PostgreSQL and Greenplum Database versions. You do not need to change these parameters in Greenplum Database.

### PostgreSQL 

|[add\_missing\_from](guc-list.html)

 [array\_nulls](guc-list.html)

 [backslash\_quote](guc-list.html)

 [escape\_string\_warning](guc-list.html)

|[regex\_flavor](guc-list.html)

 [standard\_conforming\_strings](guc-list.html)

 [transform\_null\_equals](guc-list.html)

|

### Greenplum Database 

|[gp\_ignore\_error\_table](guc-list.html)

|

## Greenplum Database Array Configuration Parameters 

The parameters in this topic control the configuration of the Greenplum Database array and its components: segments, master, distributed transaction manager, master mirror, and interconnect.

### Interconnect Configuration Parameters 

|[gp\_interconnect\_fc\_method](guc-list.html)

 [gp\_interconnect\_hash\_multiplier](guc-list.html)

 [gp\_interconnect\_queue\_depth](guc-list.html)

 [gp\_interconnect\_setup\_timeout](guc-list.html)

|[gp\_interconnect\_snd\_queue\_depth](guc-list.html)

 [gp\_interconnect\_transmit\_timeout](guc-list.html)

 [gp\_interconnect\_type](guc-list.html)

 [gp\_max\_packet\_size](guc-list.html)

|

**Note:** Greenplum Database supports only the UDPIFC \(default\) and TCP interconnect types.

### Dispatch Configuration Parameters 

|[gp\_cached\_segworkers\_threshold](guc-list.html)

 [gp\_connections\_per\_thread](guc-list.html)

 [gp\_enable\_direct\_dispatch](guc-list.html)

|[gp\_segment\_connect\_timeout](guc-list.html)

 [gp\_set\_proc\_affinity](guc-list.html)

|

### Fault Operation Parameters 

|[gp\_set\_read\_only](guc-list.html)

 [gp\_fts\_probe\_interval](guc-list.html)

 [gp\_fts\_probe\_retries](guc-list.html)

|[gp\_fts\_probe\_timeout](guc-list.html)

 [gp\_fts\_probe\_threadcount](guc-list.html)

 [gp\_log\_fts](guc-list.html)

|

### Distributed Transaction Management Parameters 

|[gp\_max\_local\_distributed\_cache](guc-list.html)

|[dtx\_phase2\_retry\_count](guc-list.html)

|

### Read-Only Parameters 

|[gp\_command\_count](guc-list.html)

 [gp\_content](guc-list.html)

 [gp\_dbid](guc-list.html)

 [gp\_num\_contents\_in\_cluster](guc-list.html)

|[gp\_role](guc-list.html)

 [gp\_session\_id](guc-list.html)

 [gp\_server\_version](guc-list.html)

 [gp\_server\_version\_num](guc-list.html)

|

## Greenplum Mirroring Parameters for Master and Segments 

These parameters control the configuration of the replication between Greenplum Database primary master and standby master.

|[keep\_wal\_segments](guc-list.html)

 [repl\_catchup\_within\_range](guc-list.html)

 [replication\_timeout](guc-list.html)

 [wal\_receiver\_status\_interval](guc-list.html)

|

This parameter controls validation between Greenplum Database primary segment and standby segment during incremental resynchronization.

|[filerep\_mirrorvalidation\_during\_resync](guc-list.html)

|

## Greenplum Database Extension Parameters 

The parameters in this topic control the configuration of Greenplum Database extensions.

|[pgcrypto.fips](guc-list.html)

 [pljava\_classpath](guc-list.html)

 [pljava\_classpath\_insecure](guc-list.html)

 [pljava\_statement\_cache\_size](guc-list.html)

 [pljava\_release\_lingering\_savepoints](guc-list.html)

 [pljava\_vmoptions](guc-list.html)

|

## XML Data Parameters 

The parameters in this topic control the configuration of the Greenplum Database XML data type.

|[xmlbinary](guc-list.html)

 [xmloption](guc-list.html)

|

