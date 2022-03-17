---
title: Loading Data with gpload 
---

The Greenplum `gpload` utility loads data using readable external tables and the Greenplum parallel file server \(gpfdist or `gpfdists`\). It handles parallel file-based external table setup and allows users to configure their data format, external table definition, and gpfdist or `gpfdists` setup in a single configuration file.

**Note:** `gpfdist` and `gpload` are compatible only with the Greenplum Database major version in which they are shipped. For example, a `gpfdist` utility that is installed with Greenplum Database 4.x cannot be used with Greenplum Database 5.x or 6.x.

**Note:** `MERGE` and `UPDATE` operations are not supported if the target table column name is a reserved keyword, has capital letters, or includes any character that requires quotes \(" "\) to identify the column.

## <a id="du168147"></a>To use gpload 

1.  Ensure that your environment is set up to run `gpload`. Some dependent files from your Greenplum Database installation are required, such as gpfdist and Python, as well as network access to the Greenplum segment hosts.

    See the *Greenplum Database Reference Guide* for details.

2.  Create your load control file. This is a YAML-formatted file that specifies the Greenplum Database connection information, gpfdist configuration information, external table options, and data format.

    See the *Greenplum Database Reference Guide* for details.

    For example:

    ```
    ---
    VERSION: 1.0.0.1
    DATABASE: ops
    USER: gpadmin
    HOST: mdw-1
    PORT: 5432
    GPLOAD:
       INPUT:
        - SOURCE:
             LOCAL_HOSTNAME:
               - etl1-1
               - etl1-2
               - etl1-3
               - etl1-4
             PORT: 8081
             FILE: 
               - /var/load/data/*
        - COLUMNS:
               - name: text
               - amount: float4
               - category: text
               - descr: text
               - date: date
        - FORMAT: text
        - DELIMITER: '|'
        - ERROR_LIMIT: 25
        - LOG_ERRORS: true
       OUTPUT:
        - TABLE: payables.expenses
        - MODE: INSERT
       PRELOAD:
        - REUSE_TABLES: true 
    SQL:
       - BEFORE: "INSERT INTO audit VALUES('start', current_timestamp)"
       - AFTER: "INSERT INTO audit VALUES('end', current_timestamp)"
    
    ```

3.  Run `gpload`, passing in the load control file. For example:

    ```
    gpload -f my_load.yml
    
    ```


**Parent topic:**[Loading and Unloading Data](../../load/topics/g-loading-and-unloading-data.html)

