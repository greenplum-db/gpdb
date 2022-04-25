---
title: Using PL/Container 
---

This topic covers further details on:

-   [PL/Container Resource Management](#topic_resmgmt)
-   [PL/Container Functions](#topic_rh3_p3q_dw)

## <a id="topic_resmgmt"></a>PL/Container Resource Management 

The Docker containers and the Greenplum Database servers share CPU and memory resources on the same hosts. In the default case, Greenplum Database is unaware of the resources consumed by running PL/Container instances. You can use Greenplum Database resource groups to control overall CPU and memory resource usage for running PL/Container instances.

PL/Container manages resource usage at two levels - the container level and the runtime level. You can control container-level CPU and memory resources with the `memory_mb` and `cpu_share` settings that you configure for the PL/Container runtime. `memory_mb` governs the memory resources available to each container instance. The `cpu_share` setting identifies the relative weighting of a container's CPU usage compared to other containers. See [plcontainer Configuration File](../utility_guide/ref/plcontainer-configuration.html) for further details.

You cannot, by default, restrict the number of running PL/Container container instances, nor can you restrict the total amount of memory or CPU resources that they consume.

### <a id="topic_resgroup"></a>Using Resource Groups to Manage PL/Container Resources 

With PL/Container 1.2.0 and later, you can use Greenplum Database resource groups to manage and limit the total CPU and memory resources of containers in PL/Container runtimes. For more information about enabling, configuring, and using Greenplum Database resource groups, refer to [Using Resource Groups](../admin_guide/workload_mgmt_resgroups.html) in the *Greenplum Database Administrator Guide*.

**Note:** If you do not explicitly configure resource groups for a PL/Container runtime, its container instances are limited only by system resources. The containers may consume resources at the expense of the Greenplum Database server.

Resource groups for external components such as PL/Container use Linux control groups \(cgroups\) to manage component-level use of memory and CPU resources. When you manage PL/Container resources with resource groups, you configure both a memory limit and a CPU limit that Greenplum Database applies to all container instances that share the same PL/Container runtime configuration.

When you create a resource group to manage the resources of a PL/Container runtime, you must specify `MEMORY_AUDITOR=cgroup` and `CONCURRENCY=0` in addition to the required CPU and memory limits. For example, the following command creates a resource group named `plpy_run1_rg` for a PL/Container runtime:

```
CREATE RESOURCE GROUP plpy_run1_rg WITH (MEMORY_AUDITOR=cgroup, CONCURRENCY=0,
                                                  CPU_RATE_LIMIT=10, MEMORY_LIMIT=10);
```

PL/Container does not use the `MEMORY_SHARED_QUOTA` and `MEMORY_SPILL_RATIO` resource group memory limits. Refer to the [CREATE RESOURCE GROUP](../ref_guide/sql_commands/CREATE_RESOURCE_GROUP.html) reference page for detailed information about this SQL command.

You can create one or more resource groups to manage your running PL/Container instances. After you create a resource group for PL/Container, you assign the resource group to one or more PL/Container runtimes. You make this assignment using the `groupid` of the resource group. You can determine the `groupid` for a given resource group name from the `gp_resgroup_config` `gp_toolkit` view. For example, the following query displays the `groupid` of a resource group named `plpy_run1_rg`:

```
SELECT groupname, groupid FROM gp_toolkit.gp_resgroup_config
 WHERE groupname='plpy_run1_rg';
                            
 groupname   |  groupid
 --------------+----------
 plpy_run1_rg |   16391
 (1 row)
```

You assign a resource group to a PL/Container runtime configuration by specifying the `-s resource_group_id=rg\_groupid` option to the `plcontainer runtime-add` \(new runtime\) or `plcontainer runtime-replace` \(existing runtime\) commands. For example, to assign the `plpy_run1_rg` resource group to a new PL/Container runtime named `python_run1`:

```
plcontainer runtime-add -r python_run1 -i pivotaldata/plcontainer_python_shared:devel -l python -s resource_group_id=16391
```

You can also assign a resource group to a PL/Container runtime using the `plcontainer runtime-edit` command. For information about the `plcontainer` command, see [plcontainer](../utility_guide/ref/plcontainer.html) reference page.

After you assign a resource group to a PL/Container runtime, all container instances that share the same runtime configuration are subject to the memory limit and the CPU limit that you configured for the group. If you decrease the memory limit of a PL/Container resource group, queries running in containers in the group may fail with an out of memory error. If you drop a PL/Container resource group while there are running container instances, Greenplum Database terminates the running containers.

### <a id="topic_resgroupcfg"></a>Configuring Resource Groups for PL/Container 

To use Greenplum Database resource groups to manage PL/Container resources, you must explicitly configure both resource groups and PL/Container.

Perform the following procedure to configure PL/Container to use Greenplum Database resource groups for CPU and memory resource management:

1.  If you have not already configured and enabled resource groups in your Greenplum Database deployment, configure cgroups and enable Greenplum Database resource groups as described in [Using Resource Groups](../admin_guide/workload_mgmt_resgroups.html#topic71717999) in the *Greenplum Database Administrator Guide*.

    **Note:** If you have previously configured and enabled resource groups in your deployment, ensure that the Greenplum Database resource group `gpdb.conf` cgroups configuration file includes a `memory { }` block as described in the previous link.

2.  Analyze the resource usage of your Greenplum Database deployment. Determine the percentage of resource group CPU and memory resources that you want to allocate to PL/Container Docker containers.
3.  Determine how you want to distribute the total PL/Container CPU and memory resources that you identified in the step above among the PL/Container runtimes. Identify:
    -   The number of PL/Container resource group\(s\) that you require.
    -   The percentage of memory and CPU resources to allocate to each resource group.
    -   The resource-group-to-PL/Container-runtime assignment\(s\).
4.  Create the PL/Container resource groups that you identified in the step above. For example, suppose that you choose to allocate 25% of both memory and CPU Greenplum Database resources to PL/Container. If you further split these resources among 2 resource groups 60/40, the following SQL commands create the resource groups:

    ```
    CREATE RESOURCE GROUP plr_run1_rg WITH (MEMORY_AUDITOR=cgroup, CONCURRENCY=0,
                                               CPU_RATE_LIMIT=15, MEMORY_LIMIT=15);
     CREATE RESOURCE GROUP plpy_run1_rg WITH (MEMORY_AUDITOR=cgroup, CONCURRENCY=0,
                                              CPU_RATE_LIMIT=10, MEMORY_LIMIT=10);
    ```

5.  Find and note the `groupid` associated with each resource group that you created. For example:

    ```
    SELECT groupname, groupid FROM gp_toolkit.gp_resgroup_config
    WHERE groupname IN ('plpy_run1_rg', 'plr_run1_rg');
                                        
    groupname   |  groupid
    --------------+----------
    plpy_run1_rg |   16391
    plr_run1_rg  |   16393
    (1 row)
    ```

6.  Assign each resource group that you created to the desired PL/Container runtime configuration. If you have not yet created the runtime configuration, use the `plcontainer runtime-add` command. If the runtime already exists, use the `plcontainer runtime-replace` or `plcontainer runtime-edit` command to add the resource group assignment to the runtime configuration. For example:

    ```
    plcontainer runtime-add -r python_run1 -i pivotaldata/plcontainer_python_shared:devel -l python -s resource_group_id=16391
    plcontainer runtime-replace -r r_run1 -i pivotaldata/plcontainer_r_shared:devel -l r -s resource_group_id=16393
    ```

    For information about the `plcontainer` command, see [plcontainer](../utility_guide/ref/plcontainer.html) reference page.


### <a id="plc_notes"></a>Notes 

**PL/Container logging**

When PL/Container logging is enabled, you can set the log level with the Greenplum Database server configuration parameter [log\_min\_messages](../ref_guide/config_params/guc-list.html). The default log level is `warning`. The parameter controls the PL/Container log level and also controls the Greenplum Database log level.

-   PL/Container logging is enabled or disabled for each runtime ID with the `setting` attribute `use_container_logging`. The default is no logging.
-   The PL/Container log information is the information from the UDF that is run in the Docker container. By default, the PL/Container log information is sent to a system service. On Red Hat 7 or CentOS 7 systems, the log information is sent to the `journald` service.
-   The Greenplum Database log information is sent to log file on the Greenplum Database master.
-   When testing or troubleshooting a PL/Container UDF, you can change the Greenplum Database log level with the `SET` command. You can set the parameter in the session before you run your PL/Container UDF. This example sets the log level to `debug1`.

    ```
    SET log_min_messages='debug1' ;
    ```

    **Note:** The parameter `log_min_messages` controls both the Greenplum Database and PL/Container logging, increasing the log level might affect Greenplum Database performance even if a PL/Container UDF is not running.


## <a id="topic_rh3_p3q_dw"></a>PL/Container Functions 

When you enable PL/Container in a database of a Greenplum Database system, the language `plcontainer` is registered in that database. Specify `plcontainer` as a language in a UDF definition to create and run user-defined functions in the procedural languages supported by the PL/Container Docker images.

### <a id="topic_c3v_clg_wkb"></a>Limitations 

Review the following limitations when creating and using PL/Container PL/Python and PL/R functions:

-   Greenplum Database domains are not supported.
-   Multi-dimensional arrays are not supported.
-   Python and R call stack information is not displayed when debugging a UDF.
-   The `plpy.execute()` methods `nrows()` and `status()` are not supported.
-   The PL/Python function `plpy.SPIError()` is not supported.
-   Running the `SAVEPOINT` command with `plpy.execute()` is not supported.
-   The `DO` command \(anonymous code block\) is supported only with PL/Container 3 \(currently a Beta feature\).
-   Container flow control is not supported.
-   Triggers are not supported.
-   `OUT` parameters are not supported.
-   The Python `dict` type cannot be returned from a PL/Python UDF. When returning the Python `dict` type from a UDF, you can convert the `dict` type to a Greenplum Database user-defined data type \(UDT\).

### <a id="using_functions"></a>Using PL/Container functions 

A UDF definition that uses PL/Container must have the these items.

-   The first line of the UDF must be `# container: ID`
-   The `LANGUAGE` attribute must be `plcontainer`

The ID is the name that PL/Container uses to identify a Docker image. When Greenplum Database runs a UDF on a host, the Docker image on the host is used to start a Docker container that runs the UDF. In the XML configuration file `plcontainer_configuration.xml`, there is a `runtime` XML element that contains a corresponding `id` XML element that specifies the Docker container startup information. See [../utility\_guide/ref/plcontainer-configuration.md\#](../utility_guide/ref/plcontainer-configuration.html) for information about how PL/Container maps the ID to a Docker image.

The PL/Container configuration file is read only on the first invocation of a PL/Container function in each Greenplum Database session that runs PL/Container functions. You can force the configuration file to be re-read by performing a `SELECT` command on the view `plcontainer_refresh_config` during the session. For example, this `SELECT` command forces the configuration file to be read.

```
SELECT * FROM plcontainer_refresh_config;
```

The command runs a PL/Container function that updates the configuration on the master and segment instances and returns the status of the refresh.

```
 gp_segment_id | plcontainer_refresh_local_config
 ---------------+----------------------------------
 1 | ok
 0 | ok
-1 | ok
(3 rows)
```

Also, you can show all the configurations in the session by performing a `SELECT` command on the view `plcontainer_show_config`. For example, this `SELECT` command returns the PL/Container configurations.

```
SELECT * FROM plcontainer_show_config;
```

Running the command executes a PL/Container function that displays configuration information from the master and segment instances. This is an example of the start and end of the view output.

```
INFO:  plcontainer: Container 'plc_py_test' configuration
 INFO:  plcontainer:     image = 'pivotaldata/plcontainer_python_shared:devel'
 INFO:  plcontainer:     memory_mb = '1024'
 INFO:  plcontainer:     use container network = 'no'
 INFO:  plcontainer:     use container logging  = 'no'
 INFO:  plcontainer:     shared directory from host '/usr/local/greenplum-db/./bin/plcontainer_clients' to container '/clientdir'
 INFO:  plcontainer:     access = readonly
                
 ...
                
 INFO:  plcontainer: Container 'plc_r_example' configuration  (seg0 slice3 192.168.180.45:40000 pid=3304)
 INFO:  plcontainer:     image = 'pivotaldata/plcontainer_r_without_clients:0.2'  (seg0 slice3 192.168.180.45:40000 pid=3304)
 INFO:  plcontainer:     memory_mb = '1024'  (seg0 slice3 192.168.180.45:40000 pid=3304)
 INFO:  plcontainer:     use container network = 'no'  (seg0 slice3 192.168.180.45:40000 pid=3304)
 INFO:  plcontainer:     use container logging  = 'yes'  (seg0 slice3 192.168.180.45:40000 pid=3304)
 INFO:  plcontainer:     shared directory from host '/usr/local/greenplum-db/bin/plcontainer_clients' to container '/clientdir'  (seg0 slice3 192.168.180.45:40000 pid=3304)
 INFO:  plcontainer:         access = readonly  (seg0 slice3 192.168.180.45:40000 pid=3304)
 gp_segment_id | plcontainer_show_local_config
 ---------------+-------------------------------
  0 | ok
 -1 | ok
  1 | ok
```

The PL/Container function `plcontainer_containers_summary()` displays information about the currently running Docker containers.

```
SELECT * FROM plcontainer_containers_summary();
```

If a normal \(non-superuser\) Greenplum Database user runs the function, the function displays information only for containers created by the user. If a Greenplum Database superuser runs the function, information for all containers created by Greenplum Database users is displayed. This is sample output when 2 containers are running.

```
 SEGMENT_ID |                           CONTAINER_ID                           |   UP_TIME    |  OWNER  | MEMORY_USAGE(KB)
 ------------+------------------------------------------------------------------+--------------+---------+------------------
 1          | 693a6cb691f1d2881ec0160a44dae2547a0d5b799875d4ec106c09c97da422ea | Up 8 seconds | gpadmin | 12940
 1          | bc9a0c04019c266f6d8269ffe35769d118bfb96ec634549b2b1bd2401ea20158 | Up 2 minutes | gpadmin | 13628
 (2 rows)
```

When Greenplum Database runs a PL/Container UDF, Query Executer \(QE\) processes start Docker containers and reuse them as needed. After a certain amount of idle time, a QE process quits and destroys its Docker containers. You can control the amount of idle time with the Greenplum Database server configuration parameter [gp\_vmem\_idle\_resource\_timeout](../ref_guide/config_params/guc-list.html). Controlling the idle time might help with Docker container reuse and avoid the overhead of creating and starting a Docker container.

**Warning:** Changing `gp_vmem_idle_resource_timeout` value, might affect performance due to resource issues. The parameter also controls the freeing of Greenplum Database resources other than Docker containers.

### <a id="function_examples"></a>Examples 

The values in the `# container` lines of the examples, `plc_python_shared` and `plc_r_shared`, are the `id` XML elements defined in the `plcontainer_config.xml` file. The `id` element is mapped to the `image` element that specifies the Docker image to be started. If you configured PL/Container with a different ID, change the value of the `# container` line. For information about configuring PL/Container and viewing the configuration settings, see [plcontainer Configuration File](../utility_guide/ref/plcontainer-configuration.html).

This is an example of PL/Python function that runs using the `plc_python_shared` container that contains Python 2:

```
CREATE OR REPLACE FUNCTION pylog100() RETURNS double precision AS $$
 # container: plc_python_shared
 import math
 return math.log10(100)
 $$ LANGUAGE plcontainer;
```

This is an example of a similar function using the `plc_r_shared` container:

```
CREATE OR REPLACE FUNCTION rlog100() RETURNS text AS $$
# container: plc_r_shared
return(log10(100))
$$ LANGUAGE plcontainer;
```

If the `# container` line in a UDF specifies an ID that is not in the PL/Container configuration file, Greenplum Database returns an error when you try to run the UDF.

### <a id="topic_ctk_xjg_wkb"></a>About PL/Container Running PL/Python 

In the Python language container, the module `plpy` is implemented. The module contains these methods:

-   `plpy.execute(stmt)` - Runs the query string `stmt` and returns query result in a list of dictionary objects. To be able to access the result fields ensure your query returns named fields.
-   `plpy.prepare(stmt[, argtypes])` - Prepares the execution plan for a query. It is called with a query string and a list of parameter types, if you have parameter references in the query.
-   `plpy.execute(plan[, argtypes])` - Runs a prepared plan.
-   `plpy.debug(msg)` - Sends a DEBUG2 message to the Greenplum Database log.
-   `plpy.log(msg)` - Sends a LOG message to the Greenplum Database log.
-   `plpy.info(msg)` - Sends an INFO message to the Greenplum Database log.
-   `plpy.notice(msg)` - Sends a NOTICE message to the Greenplum Database log.
-   `plpy.warning(msg)` - Sends a WARNING message to the Greenplum Database log.
-   `plpy.error(msg)` - Sends an ERROR message to the Greenplum Database log. An ERROR message raised in Greenplum Database causes the query execution process to stop and the transaction to rollback.
-   `plpy.fatal(msg)` - Sends a FATAL message to the Greenplum Database log. A FATAL message causes Greenplum Database session to be closed and transaction to be rolled back.
-   `plpy.subtransaction()` - Manages `plpy.execute` calls in an explicit subtransaction. See [Explicit Subtransactions](https://www.postgresql.org/docs/9.4/plpython-subtransaction.html) in the PostgreSQL documentation for additional information about `plpy.subtransaction()`.

If an error of level `ERROR` or `FATAL` is raised in a nested Python function call, the message includes the list of enclosing functions.

The Python language container supports these string quoting functions that are useful when constructing ad-hoc queries.

-   `plpy.quote_literal(string)` - Returns the string quoted to be used as a string literal in an SQL statement string. Embedded single-quotes and backslashes are properly doubled. `quote_literal()` returns null on null input \(empty input\). If the argument might be null, `quote_nullable()` might be more appropriate.
-   `plpy.quote_nullable(string)` - Returns the string quoted to be used as a string literal in an SQL statement string. If the argument is null, returns `NULL`. Embedded single-quotes and backslashes are properly doubled.
-   `plpy.quote_ident(string)` - Returns the string quoted to be used as an identifier in an SQL statement string. Quotes are added only if necessary \(for example, if the string contains non-identifier characters or would be case-folded\). Embedded quotes are properly doubled.

When returning text from a PL/Python function, PL/Container converts a Python unicode object to text in the database encoding. If the conversion cannot be performed, an error is returned.

PL/Container does not support this Greenplum Database PL/Python feature:

-   Multi-dimensional arrays.

Also, the Python module has two global dictionary objects that retain the data between function calls. They are named GD and SD. GD is used to share the data between all the function running within the same container, while SD is used for sharing the data between multiple calls of each separate function. Be aware that accessing the data is possible only within the same session, when the container process lives on a segment or master. Be aware that for idle sessions Greenplum Database terminates segment processes, which means the related containers would be shut down and the data from GD and SD lost.

For information about PL/Python, see [PL/Python Language](pl_python.html).

For information about the `plpy` methods, see [https://www.postgresql.org/docs/9.4/plpython-database.htm](https://www.postgresql.org/docs/9.4/plpython-database.html).

### <a id="topic_plc_py3"></a>About PL/Container Running PL/Python with Python 3 

PL/Container for Greenplum Database 5 supports Python version 3.6+. PL/Container for Greenplum Database 6 supports Python 3.7+.

If you want to use PL/Container to run the same function body in both Python2 and Python3, you must create 2 different user-defined functions.

Keep in mind that UDFs that you created for Python 2 may not run in PL/Container with Python 3. The following Python references may be useful:

-   Changes to Python - [What’s New in Python 3](https://docs.python.org/3/whatsnew/3.0.html)
-   Porting from Python 2 to 3 - [Porting Python 2 Code to Python 3](https://docs.python.org/3/howto/pyporting.html)

### <a id="topic_lqz_t3q_dw"></a>About PL/Container Running PL/R 

In the R language container, the module `pg.spi` is implemented. The module contains these methods:

-   `pg.spi.exec(stmt)` - Runs the query string `stmt` and returns query result in R `data.frame`. To be able to access the result fields make sure your query returns named fields.
-   `pg.spi.prepare(stmt[, argtypes])` - Prepares the execution plan for a query. It is called with a query string and a list of parameter types if you have parameter references in the query.
-   `pg.spi.execp(plan[, argtypes])` - Runs a prepared plan.
-   `pg.spi.debug(msg)` - Sends a DEBUG2 message to the Greenplum Database log.
-   `pg.spi.log(msg)` - Sends a LOG message to the Greenplum Database log.
-   `pg.spi.info(msg)` - Sends an INFO message to the Greenplum Database log.
-   `pg.spi.notice(msg)` - Sends a NOTICE message to the Greenplum Database log.
-   `pg.spi.warning(msg)` - Sends a WARNING message to the Greenplum Database log.
-   `pg.spi.error(msg)` - Sends an ERROR message to the Greenplum Database log. An ERROR message raised in Greenplum Database causes the query execution process to stop and the transaction to rollback.
-   `pg.spi.fatal(msg)` - Sends a FATAL message to the Greenplum Database log. A FATAL message causes Greenplum Database session to be closed and transaction to be rolled back.

PL/Container does not support this PL/R feature:

-   Multi-dimensional arrays.

For information about PL/R, see [PL/R Language](pl_r.html).

For information about the `pg.spi` methods, see [http://www.joeconway.com/plr/doc/plr-spi-rsupport-funcs-normal.html](http://www.joeconway.com/plr/doc/plr-spi-rsupport-funcs-normal.html)

