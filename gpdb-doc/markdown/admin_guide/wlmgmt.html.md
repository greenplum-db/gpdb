---
title: Managing Resources 
---

Greenplum Database provides features to help you prioritize and allocate resources to queries according to business requirements and to prevent queries from starting when resources are unavailable.

You can use resource management features to limit the number of concurrent queries, the amount of memory used to run a query, and the relative amount of CPU devoted to processing a query. Greenplum Database provides two schemes to manage resources: [Resource Queues](workload_mgmt.html) and [Resource Groups](workload_mgmt_resgroups.html).

Either the resource queue or the resource group management scheme can be active in Greenplum Database; both schemes cannot be active at the same time. Set the server configuration parameter [gp_resource_manager](../ref_guide/config_params/guc-list.html.md#gp_resource_manager) to enable the preferred scheme in the Greenplum Database cluster.

The following table summarizes some of the differences between resource queues and resource groups.

|Metric|Resource Queues|Resource Groups|
|------|---------------|---------------|
|Concurrency|Defines the number of query slots available at a time|Defines the number of transaction slots available at a time|
|CPU|Specify query priority|Specify percentage of CPU resources or the specific number of CPU cores; uses Linux Control Groups|
|Memory|Managed at the queue and operator level; users can over-subscribe|Managed at the transaction level, with enhanced allocation and tracking; users can over-subscribe|
|Users|Limits are applied only to non-admin users|Limits are applied to `SUPERUSER`, non-admin users, and system processes of non-user classes|
|Disk I/O|None|Limit the maximum read/write disk I/O throughput, and maximum read/write I/O operations per second|
|Queueing|Queue when no slot available or not enough available memory|Queue only when no slot is available|
|Query Failure|Query may fail immediately if the allocated memory for the query surpasses the available system memory and spill limits|Query may fail if the allocated memory for the query surpasses the available system memory and spill limits|
|Limit Bypass|Limits are not enforced for `SUPERUSER` roles and certain operators and functions|Limits are not enforced on `SET`, `RESET`, and `SHOW` commands. Additionally, certain queries may be configured to bypass the concurrency limit|
|External Components|None|None|

**Parent topic:** [Managing Performance](partV.html)

