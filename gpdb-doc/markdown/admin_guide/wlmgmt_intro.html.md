---
title: Greenplum Database Memory Overview 
---

Memory is a key resource for a Greenplum Database system and, when used efficiently, can ensure high performance and throughput. This topic describes how segment host memory is allocated between segments and the options available to administrators to configure memory.

A Greenplum Database segment host runs multiple PostgreSQL instances, all sharing the host's memory. The segments have an identical configuration and they consume similar amounts of memory, CPU, and disk IO simultaneously, while working on queries in parallel.

For best query throughput, the memory configuration should be managed carefully. There are memory configuration options at every level in Greenplum Database, from operating system parameters, to managing resources with resource queues and resource groups, to setting the amount of memory allocated to an individual query.

## <a id="seghost"></a>Segment Host Memory 

On a Greenplum Database segment host, the available host memory is shared among all the processes running on the computer, including the operating system, Greenplum Database segment instances, and other application processes. Administrators must determine what Greenplum Database and non-Greenplum Database processes share the hosts' memory and configure the system to use the memory efficiently. It is equally important to monitor memory usage regularly to detect any changes in the way host memory is consumed by Greenplum Database or other processes.

The following figure illustrates how memory is consumed on a Greenplum Database segment host when resource queue-based resource management is active.

![](graphics/memory.png "Greenplum Database Segment Host Memory")

Beginning at the bottom of the illustration, the line labeled *A* represents the total host memory. The line directly above line *A* shows that the total host memory comprises both physical RAM and swap space.

The line labelled *B* shows that the total memory available must be shared by Greenplum Database and all other processes on the host. Non-Greenplum Database processes include the operating system and any other applications, for example system monitoring agents. Some applications may use a significant portion of memory and, as a result, you may have to adjust the number of segments per Greenplum Database host or the amount of memory per segment.

The segments \(*C*\) each get an equal share of the Greenplum Database Memory \(*B*\).

Within a segment, the currently active resource management scheme, *Resource Queues* or *Resource Groups*, governs how memory is allocated to run a SQL statement. These constructs allow you to translate business requirements into execution policies in your Greenplum Database system and to guard against queries that could degrade performance. For an overview of resource groups and resource queues, refer to [Managing Resources](wlmgmt.html).

## <a id="opts"></a>Options for Configuring Segment Host Memory 

Host memory is the total memory shared by all applications on the segment host. You can configure the amount of host memory using any of the following methods:

-   Add more RAM to the nodes to increase the physical memory.
-   Allocate swap space to increase the size of virtual memory.
-   Set the kernel parameters vm.overcommit\_memory and vm.overcommit\_ratio to configure how the operating system handles large memory allocation requests.

The physical RAM and OS configuration are usually managed by the platform team and system administrators. See the [Greenplum Database Installation Guide](../install_guide/prep_os.html#topic3) for the recommended kernel parameters and for how to set the `/etc/sysctl.conf` file parameters.

The amount of memory to reserve for the operating system and other processes is workload dependent. The minimum recommendation for operating system memory is 32GB, but if there is much concurrency in Greenplum Database, increasing to 64GB of reserved memory may be required. The largest user of operating system memory is SLAB, which increases as Greenplum Database concurrency and the number of sockets used increases.

The `vm.overcommit_memory` kernel parameter should always be set to 2, the only safe value for Greenplum Database.

The `vm.overcommit_ratio` kernel parameter sets the percentage of RAM that is used for application processes, the remainder reserved for the operating system. The default for Red Hat is 50 \(50%\). Setting this parameter too high may result in insufficient memory reserved for the operating system, which can cause segment host failure or database failure. Leaving the setting at the default of 50 is generally safe, but conservative. Setting the value too low reduces the amount of concurrency and the complexity of queries you can run at the same time by reducing the amount of memory available to Greenplum Database. When increasing `vm.overcommit_ratio`, it is important to remember to always reserve some memory for operating system activities.

**Configuring vm.overcommit\_ratio when Resource Group-Based Resource Management is Active**

When resource group-based resource management is active, tune the operating system `vm.overcommit_ratio` as necessary. If your memory utilization is too low, increase the value; if your memory or swap usage is too high, decrease the setting.

**Configuring vm.overcommit\_ratio when Resource Queue-Based Resource Management is Active**

To calculate a safe value for `vm.overcommit_ratio` when resource queue-based resource management is active, first determine the total memory available to Greenplum Database processes, `gp_vmem_rq`.

-   If the total system memory is less than 256 GB, use this formula:

    ```
    gp_vmem_rq = ((SWAP + RAM) – (7.5GB + 0.05 * RAM)) / 1.7
    ```

-   If the total system memory is equal to or greater than 256 GB, use this formula:

    ```
    gp_vmem_rq = ((SWAP + RAM) – (7.5GB + 0.05 * RAM)) / 1.17
    ```


where `SWAP` is the swap space on the host in GB, and `RAM` is the number of GB of RAM installed on the host.

When resource queue-based resource management is active, use `gp_vmem_rq` to calculate the `vm.overcommit_ratio` value with this formula:

```
vm.overcommit_ratio = (RAM - 0.026 * gp_vmem_rq) / RAM
```

## <a id="section_nfn_q5r_bs"></a>Configuring Greenplum Database Memory 

Greenplum Database Memory is the amount of memory available to all Greenplum Database segment instances.

When you set up the Greenplum Database cluster, you determine the number of primary segments to run per host and the amount of memory to allocate for each segment. Depending on the CPU cores, amount of physical RAM, and workload characteristics, the number of segments is usually a value between 4 and 8. With segment mirroring enabled, it is important to allocate memory for the maximum number of primary segments running on a host during a failure. For example, if you use the default grouping mirror configuration, a segment host failure doubles the number of acting primaries on the host that has the failed host's mirrors. Mirror configurations that spread each host's mirrors over multiple other hosts can lower the maximum, allowing more memory to be allocated for each segment. For example, if you use a block mirroring configuration with 4 hosts per block and 8 primary segments per host, a single host failure would cause other hosts in the block to have a maximum of 11 active primaries, compared to 16 for the default grouping mirror configuration.

**Configuring Segment Memory when Resource Group-Based Resource Management is Active**

When resource group-based resource management is active, the amount of memory allocated to each segment on a segment host is the memory available to Greenplum Database multiplied by the gp\_resource\_group\_memory\_limit server configuration parameter and divided by the number of active primary segments on the host. Use the following formula to calculate segment memory when using resource groups for resource management.

```

rg_perseg_mem = ((RAM * (vm.overcommit_ratio / 100) + SWAP) * gp_resource_group_memory_limit) / num_active_primary_segments
```

Resource groups expose additional configuration parameters that enable you to further control and refine the amount of memory allocated for queries.

**Configuring Segment Memory when Resource Queue-Based Resource Management is Active**

When resource queue-based resource management is active, the gp\_vmem\_protect\_limit server configuration parameter value identifies the amount of memory to allocate to each segment. This value is estimated by calculating the memory available for all Greenplum Database processes and dividing by the maximum number of primary segments during a failure. If gp\_vmem\_protect\_limit is set too high, queries can fail. Use the following formula to calculate a safe value for gp\_vmem\_protect\_limit; provide the `gp_vmem_rq` value that you calculated earlier.

```

gp_vmem_protect_limit = gp_vmem_rq / max_acting_primary_segments
```

where `max_acting_primary_segments` is the maximum number of primary segments that could be running on a host when mirror segments are activated due to a host or segment failure.

**Note:** The gp\_vmem\_protect\_limit setting is enforced only when resource queue-based resource management is active in Greenplum Database. Greenplum ignores this configuration parameter when resource group-based resource management is active.

Resource queues expose additional configuration parameters that enable you to further control and refine the amount of memory allocated for queries.

## <a id="section_example"></a>Example Memory Configuration Calculations 

This section provides example memory calculations for resource queues and resource groups for a Greenplum Database system with the following specifications:

-   Total RAM = 256GB
-   Swap = 64GB
-   8 primary segments and 8 mirror segments per host, in blocks of 4 hosts
-   Maximum number of primaries per host during failure is 11

**Resource Group Example**

When resource group-based resource management is active in Greenplum Database, the usable memory available on a host is a function of the amount of RAM and swap space configured for the system, as well as the `vm.overcommit_ratio` system parameter setting:

```

total_node_usable_memory = RAM * (vm.overcommit_ratio / 100) + Swap
                         = 256GB * (50/100) + 64GB
                         = 192GB
```

Assuming the default `gp_resource_group_memory_limit` value \(.7\), the memory allocated to a Greenplum Database host with the example configuration is:

```

total_gp_memory = total_node_usable_memory * gp_resource_group_memory_limit
                = 192GB * .7
                = 134.4GB
```

The memory available to a Greenplum Database segment on a segment host is a function of the memory reserved for Greenplum on the host and the number of active primary segments on the host. On cluster startup:

```

gp_seg_memory = total_gp_memory / number_of_active_primary_segments
              = 134.4GB / 8
              = 16.8GB
```

Note that when 3 mirror segments switch to primary segments, the per-segment memory is still 16.8GB. Total memory usage on the segment host may approach:

```
total_gp_memory_with_primaries = 16.8GB * 11 = 184.8GB
```

**Resource Queue Example**

The `vm.overcommit_ratio` calculation for the example system when resource queue-based resource management is active in Greenplum Database follows:

```

gp_vmem_rq = ((SWAP + RAM) – (7.5GB + 0.05 * RAM)) / 1.7
        = ((64 + 256) - (7.5 + 0.05 * 256)) / 1.7
        = 176

vm.overcommit_ratio = (RAM - (0.026 * gp_vmem_rq)) / RAM
                    = (256 - (0.026 * 176)) / 256
                    = .982
```

You would set `vm.overcommit_ratio` of the example system to 98.

The `gp_vmem_protect_limit` calculation when resource queue-based resource management is active in Greenplum Database:

```

gp_vmem_protect_limit = gp_vmem_rq / maximum_acting_primary_segments
                      = 176 / 11
                      = 16GB
                      = 16384MB

```

You would set the `gp_vmem_protect_limit` server configuration parameter on the example system to 16384.

**Parent topic:**[Managing Performance](partV.html)

