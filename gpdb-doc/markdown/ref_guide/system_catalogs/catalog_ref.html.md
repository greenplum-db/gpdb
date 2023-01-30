# System Catalogs 

This reference describes the Greenplum Database system catalog tables and views. System tables prefixed with `gp_` relate to the parallel features of Greenplum Database. Tables prefixed with `pg_` are either standard PostgreSQL system catalog tables supported in Greenplum Database, or are related to features Greenplum that provides to enhance PostgreSQL for data warehousing workloads. Note that the global system catalog for Greenplum Database resides on the coordinator instance.

> **Caution** Changes to Greenplum Database system catalog tables or views are not supported. If a catalog table or view is changedby the customer, the VMware Greenplum cluster is not supported. The cluster must be reinitialized and restored by the customer.

-   [System Tables](catalog_ref-tables.html)
-   [System Views](catalog_ref-views.html)
-   [System Catalogs Definitions](catalog_ref-html.html)

**Parent topic:** [Greenplum Database Reference Guide](../ref_guide.html)

