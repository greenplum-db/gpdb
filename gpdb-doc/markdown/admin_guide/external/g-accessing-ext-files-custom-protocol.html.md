---
title: Using a Custom Protocol 
---

A custom protocol allows you to connect Greenplum Database to a data source that cannot be accessed with the `file://`, `gpfdist://`, or `pxf://` protocols.

Creating a custom protocol requires that you implement a set of C functions with specified interfaces, declare the functions in Greenplum Database, and then use the `CREATE TRUSTED PROTOCOL` command to enable the protocol in the database.

See [Example Custom Data Access Protocol](../load/topics/g-example-custom-data-access-protocol.html) for an example.

**Parent topic:** [Defining External Tables](../external/g-external-tables.html)

