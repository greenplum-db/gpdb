---
title: Managing Resources 
---

Greenplum Database provides features to help you prioritize and allocate resources to queries according to business requirements and to prevent queries from starting when resources are unavailable.

You can use resource management features to limit the number of concurrent queries, the amount of memory used to run a query, and the relative amount of CPU devoted to processing a query. Greenplum Database provides two schemes to manage resources - Resource Queues and Resource Groups.

Either the resource queue or the resource group management scheme can be active in Greenplum Database; both schemes cannot be active at the same time.

Resource queues are enabled by default when you install your Greenplum Database cluster. While you can create and assign resource groups when resource queues are active, you must explicitly enable resource groups to start using that management scheme.

The following table summarizes some of the differences between Resource Queues and Resource Groups.

|Metric|Resource Queues|Resource Groups|
|------|---------------|---------------|
|Concurrency|Managed at the query level|Managed at the transaction level|
|CPU|Specify query priority|Specify percentage of CPU resources; uses Linux Control Groups|
|Memory|Managed at the queue and operator level; users can over-subscribe|Managed at the transaction level, with enhanced allocation and tracking; users cannot over-subscribe|
|Memory Isolation|None|Memory is isolated between resource groups and between transactions within the same resource group|
|Users|Limits are applied only to non-admin users|Limits are applied to `SUPERUSER` and non-admin users alike|
|Queueing|Queue only when no slot available|Queue when no slot is available or not enough available memory|
|Query Failure|Query may fail immediately if not enough memory|Query may fail after reaching transaction fixed memory limit when no shared resource group memory exists and the transaction requests more memory|
|Limit Bypass|Limits are not enforced for `SUPERUSER` roles and certain operators and functions|Limits are not enforced on `SET`, `RESET`, and `SHOW` commands|
|External Components|None|Manage PL/Container CPU and memory resources|

-   **[Using Resource Groups](workload_mgmt_resgroups.html)**  

-   **[Using Resource Queues](workload_mgmt.html)**  


**Parent topic:** [Managing Performance](partV.html)

