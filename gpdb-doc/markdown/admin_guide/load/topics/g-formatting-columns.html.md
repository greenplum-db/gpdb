---
title: Formatting Columns 
---

The default column or field delimiter is the horizontal `TAB` character \(`0x09`\) for text files and the comma character \(`0x2C`\) for CSV files. You can declare a single character delimiter using the `DELIMITER` clause of `COPY`, `CREATE EXTERNAL TABLE` or `gpload` when you define your data format. The delimiter character must appear between any two data value fields. Do not place a delimiter at the beginning or end of a row. For example, if the pipe character \( \| \) is your delimiter:

```
data value 1|data value 2|data value 3

```

The following command shows the use of the pipe character as a column delimiter:

```
=# CREATE EXTERNAL TABLE ext_table (name text, date date)
LOCATION ('gpfdist://<hostname>/filename.txt)
FORMAT 'TEXT' (DELIMITER '|');

```

**Parent topic:** [Formatting Data Files](../../load/topics/g-formatting-data-files.html)

