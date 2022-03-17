---
title: Managing a Greenplum System 
---

This section describes basic system administration tasks performed by a Greenplum Database system administrator.

-   **[About the Greenplum Database Release Version Number](../managing/versioning.html)**  
Greenplum Database version numbers and the way they change identify what has been modified from one Greenplum Database release to the next.
-   **[Starting and Stopping Greenplum Database](../managing/startstop.html)**  
In a Greenplum Database DBMS, the database server instances \(the master and all segments\) are started or stopped across all of the hosts in the system in such a way that they can work together as a unified DBMS.
-   **[Accessing the Database](../access_db/topics/g-accessing-the-database.html)**  
This topic describes the various client tools you can use to connect to Greenplum Database, and how to establish a database session.
-   **[Configuring the Greenplum Database System](../topics/g-configuring-the-greenplum-system.html)**  
Server configuration parameters affect the behavior of Greenplum Database.
-   **[Enabling Compression](../managing/compression.html)**  
You can configure Greenplum Database to use data compression with some database features and with some utilities.
-   **[Configuring Proxies for the Greenplum Interconnect](../managing/proxy-ic.html)**  
You can configure a Greenplum system to use proxies for interconnect communication to reduce the use of connections and ports during query processing.
-   **[Enabling High Availability and Data Consistency Features](../highavail/topics/g-enabling-high-availability-features.html)**  
The fault tolerance and the high-availability features of Greenplum Database can be configured.
-   **[Backing Up and Restoring Databases](../managing/backup-main.html)**  
This topic describes how to use Greenplum backup and restore features.
-   **[Expanding a Greenplum System](../expand/expand-main.html)**  
To scale up performance and storage capacity, expand your Greenplum Database system by adding hosts to the system. In general, adding nodes to a Greenplum cluster achieves a linear scaling of performance and storage capacity.
-   **[Migrating Data with gpcopy](../managing/gpcopy-migrate.html)**  
You can use the `gpcopy` utility to transfer data between databases in different Greenplum Database clusters.
-   **[Monitoring a Greenplum System](../managing/monitor.html)**  
You can monitor a Greenplum Database system using a variety of tools included with the system or available as add-ons.
-   **[Routine System Maintenance Tasks](../managing/maintain.html)**  
To keep a Greenplum Database system running efficiently, the database must be regularly cleared of expired data and the table statistics must be updated so that the query optimizer has accurate information.
-   **[Recommended Monitoring and Maintenance Tasks](../monitoring/monitoring.html)**  
This section lists monitoring and maintenance activities recommended to ensure high availability and consistent performance of your Greenplum Database cluster.

**Parent topic:**[Greenplum Database Administrator Guide](../admin_guide.html)

