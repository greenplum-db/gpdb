# About Management and Monitoring Utilities 

Greenplum Database provides standard command-line utilities for performing common monitoring and administration tasks.

Greenplum command-line utilities are located in the $GPHOME/bin directory and are executed on the master host. Greenplum provides utilities for the following administration tasks:

-   Installing Greenplum Database on an array
-   Initializing a Greenplum Database System
-   Starting and stopping Greenplum Database
-   Adding or removing a host
-   Expanding the array and redistributing tables among new segments
-   Managing recovery for failed segment instances
-   Managing failover and recovery for a failed master instance
-   Backing up and restoring a database \(in parallel\)
-   Loading data in parallel
-   Transferring data between Greenplum databases
-   System state reporting

Greenplum Database includes an optional performance management database that contains query status information and system metrics. The `gpperfmon_install` management utility creates the database, named `gpperfmon`, and enables data collection agents that execute on the Greenplum Database master and segment hosts. Data collection agents on the segment hosts collect query status from the segments, as well as system metrics such as CPU and memory utilization. An agent on the master host periodically \(typically every 15 seconds\) retrieves the data from the segment host agents and updates the `gpperfmon` database. Users can query the `gpperfmon` database to see the query and system metrics.

VMware provides an optional system monitoring and management tool, Greenplum Command Center, which administrators can install and enable with Greenplum Database. Greenplum Command Center, which depends upon the `gpperfmon` database, provides a web-based user interface for viewing the system metrics and allows administrators to perform additional system management tasks. For more information about Greenplum Command Center, see the [Greenplum Command Center documentation](https://docs.vmware.com/en/VMware-Tanzu-Greenplum-Command-Center/index.html).

![](../graphics/cc_arch_gpdb.png "Greenplum Command Center Architecture")

**Parent topic:** [Greenplum Database Concepts](../intro/partI.html)

