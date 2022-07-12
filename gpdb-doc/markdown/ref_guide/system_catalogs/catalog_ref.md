X# System Catalog Reference 

This reference describes the Greenplum Database system catalog tables and views. System tables prefixed with `gp_` relate to the parallel features of Greenplum Database. Tables prefixed with `pg_` are either standard PostgreSQL system catalog tables supported in Greenplum Database, or are related to features Greenplum that provides to enhance PostgreSQL for data warehousing workloads. Note that the global system catalog for Greenplum Database resides on the master instance.

**Warning:** Changes to Tanzu Greenplum system catalog tables or views are not supported. If a catalog table or view is changed by the customer, the Tanzu Greenplum cluster is not supported. Thecluster must be reinitialized and restoredby the customer.

-   [System Tables](catalog_ref-tables.html)
-   [System Views](catalog_ref-views.html)
-   [System Catalogs Definitions](catalog_ref-html.html)

**Parent topic:** [Greenplum Database Reference Guide](../ref_guide.html)

