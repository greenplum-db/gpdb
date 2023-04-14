---
title: Using a Custom Format 
---

You specify a custom data format in the `FORMAT` clause of `CREATE EXTERNAL TABLE`.

```
FORMAT 'CUSTOM' (formatter=format_function, key1=val1,...keyn=valn)

```

Where the `'CUSTOM'` keyword indicates that the data has a custom format and `formatter` specifies the function to use to format the data, followed by comma-separated parameters to the formatter function.

Greenplum Database provides functions for formatting fixed-width data, but you must author the formatter functions for variable-width data. The steps are as follows.

1.  Author and compile input and output functions as a shared library.
2.  Specify the shared library function with `CREATE FUNCTION` in Greenplum Database.
3.  Use the `formatter` parameter of `CREATE EXTERNAL TABLE`'s `FORMAT` clause to call the function.

-   **[Importing and Exporting Fixed Width Data](../../load/topics/g-importing-and-exporting-fixed-width-data.html)**  

-   **[Examples: Read Fixed-Width Data](../../load/topics/g-examples-read-fixed-width-data.html)**  


**Parent topic:** [Loading and Writing Non-HDFS Custom Data](../../load/topics/g-loading-and-writing-non-hdfs-custom-data.html)

