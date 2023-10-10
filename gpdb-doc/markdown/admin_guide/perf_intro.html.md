---
title: Defining Database Performance 
---

Managing system performance includes measuring performance, identifying the causes of performance problems, and applying the tools and techniques available to you to remedy the problems.

Greenplum measures database performance based on the rate at which the database management system \(DBMS\) supplies information to requesters.

**Parent topic:** [Managing Performance](partV.html)

## <a id="topic2"></a>Understanding the Performance Factors 

Several key performance factors influence database performance. Understanding these factors helps identify performance opportunities and avoid problems:

-   [System Resources](#topic3)
-   [Workload](#topic4)
-   [Throughput](#topic5)
-   [Contention](#topic6)
-   [Optimization](#topic7)

### <a id="topic3"></a>System Resources 

Database performance relies heavily on disk I/O and memory usage. To accurately set performance expectations, you need to know the baseline performance of the hardware on which your DBMS is deployed. Performance of hardware components such as CPUs, hard disks, disk controllers, RAM, and network interfaces will significantly affect how fast your database performs.

> **Caution** Do not install anti-virus software of any type on Greenplum Database hosts. VMware Greenplum is not supported for use with anti-virus software because the additional CPU and IO load interferes with Greenplum Database operations.

### <a id="topic4"></a>Workload 

The workload equals the total demand from the DBMS, and it varies over time. The total workload is a combination of user queries, applications, batch jobs, transactions, and system commands directed through the DBMS at any given time. For example, it can increase when month-end reports are run or decrease on weekends when most users are out of the office. Workload strongly influences database performance. Knowing your workload and peak demand times helps you plan for the most efficient use of your system resources and enables processing the largest possible workload.

### <a id="topic5"></a>Throughput 

A system's throughput defines its overall capability to process data. DBMS throughput is measured in queries per second, transactions per second, or average response times. DBMS throughput is closely related to the processing capacity of the underlying systems \(disk I/O, CPU speed, memory bandwidth, and so on\), so it is important to know the throughput capacity of your hardware when setting DBMS throughput goals.

### <a id="topic6"></a>Contention 

Contention is the condition in which two or more components of the workload attempt to use the system in a conflicting way — for example, multiple queries that try to update the same piece of data at the same time or multiple large workloads that compete for system resources. As contention increases, throughput decreases.

### <a id="topic7"></a>Optimization 

DBMS optimizations can affect the overall system performance. SQL formulation, database configuration parameters, table design, data distribution, and so on enable the database query optimizer to create the most efficient access plans.

## <a id="topic8"></a>Determining Acceptable Performance 

When approaching a performance tuning initiative, you should know your system's expected level of performance and define measurable performance requirements so you can accurately evaluate your system's performance. Consider the following when setting performance goals:

-   [Baseline Hardware Performance](#topic9)
-   [Performance Benchmarks](#topic10)

### <a id="topic9"></a>Baseline Hardware Performance 

Most database performance problems are caused not by the database, but by the underlying systems on which the database runs. I/O bottlenecks, memory problems, and network issues can notably degrade database performance. Knowing the baseline capabilities of your hardware and operating system \(OS\) will help you identify and troubleshoot hardware-related problems before you explore database-level or query-level tuning initiatives.

See the *Greenplum Database Reference Guide* for information about running the `gpcheckperf` utility to validate hardware and network performance.

### <a id="topic10"></a>Performance Benchmarks 

To maintain good performance or fix performance issues, you should know the capabilities of your DBMS on a defined workload. A benchmark is a predefined workload that produces a known result set. Periodically run the same benchmark tests to help identify system-related performance degradation over time. Use benchmarks to compare workloads and identify queries or applications that need optimization.

Many third-party organizations, such as the Transaction Processing Performance Council \(TPC\), provide benchmark tools for the database industry. TPC provides TPC-H, a decision support system that examines large volumes of data, runs queries with a high degree of complexity, and gives answers to critical business questions. For more information about TPC-H, go to:

[http://www.tpc.org/tpch](http://www.tpc.org/tpch)

