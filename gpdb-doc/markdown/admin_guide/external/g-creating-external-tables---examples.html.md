---
title: Examples for Creating External Tables 
---

These examples show how to define external data with different protocols. Each `CREATE EXTERNAL TABLE` command can contain only one protocol.

**Note:** When using IPv6, always enclose the numeric IP addresses in square brackets.

Start gpfdist before you create external tables with the gpfdist protocol. The following code starts the gpfdist file server program in the background on port *8081* serving files from directory /var/data/staging. The logs are saved in /home/gpadmin/log.

```
gpfdist -p 8081 -d /var/data/staging -l /home/gpadmin/log &

```

-   **[Example 1—Single gpfdist instance on single-NIC machine](../external/g-example-1-single-gpfdist-instance-on-single-nic-machine.html)**  

-   **[Example 2—Multiple gpfdist instances](../external/g-example-2-multiple-gpfdist-instances.html)**  

-   **[Example 3—Multiple gpfdists instances](../external/g-example-3-multiple-gpfdists-instances.html)**  

-   **[Example 4—Single gpfdist instance with error logging](../external/g-example-4-single-gpfdist-instance-with-error-logging.html)**  

-   **[Example 5—TEXT Format on a Hadoop Distributed File Server](../external/g-example-5-text-format-on-a-hadoop-distributed-file-server.html)**  

-   **[Example 6—Multiple files in CSV format with header rows](../external/g-example-6-multiple-files-in-csv-format-with-header-rows.html)**  

-   **[Example 7—Readable External Web Table with Script](../external/g-example-7-readable-web-external-table-with-script.html)**  

-   **[Example 8—Writable External Table with gpfdist](../external/g-example-8-writable-external-table-with-gpfdist.html)**  

-   **[Example 9—Writable External Web Table with Script](../external/g-example-9-writable-external-web-table-with-script.html)**  

-   **[Example 10—Readable and Writable External Tables with XML Transformations](../external/g-example-10-readable-and-writable-external-tables-with-xml-transformations.html)**  


**Parent topic:**[Defining External Tables](../external/g-external-tables.html)

