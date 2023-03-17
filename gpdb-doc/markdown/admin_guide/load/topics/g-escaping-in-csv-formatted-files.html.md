---
title: Escaping in CSV Formatted Files 
---

By default, the escape character is a `"` \(double quote\) for CSV-formatted files. If you want to use a different escape character, use the `ESCAPE` clause of `COPY`, `CREATE EXTERNAL TABLE` or `gpload` to declare a different escape character. In cases where your selected escape character is present in your data, you can use it to escape itself.

For example, suppose you have a table with three columns and you want to load the following three fields:

-   `Free trip to A,B`
-   `5.89`
-   `Special rate "1.79"`

Your designated delimiter character is `,` \(comma\), and your designated escape character is `"` \(double quote\). The formatted row in your data file looks like this:

```
"Free trip to A,B","5.89","Special rate ""1.79"""   
```

The data value with a comma character that is part of the data is enclosed in double quotes. The double quotes that are part of the data are escaped with a double quote even though the field value is enclosed in double quotes.

Embedding the entire field inside a set of double quotes guarantees preservation of leading and trailing whitespace characters:

```
"Free trip to A,B ","5.89 ","Special rate ""1.79"" "
```

> **Note** In CSV mode, all characters are significant. A quoted value surrounded by white space, or any characters other than `DELIMITER`, includes those characters. This can cause errors if you import data from a system that pads CSV lines with white space to some fixed width. In this case, preprocess the CSV file to remove the trailing white space before importing the data into Greenplum Database.

**Parent topic:** [Escaping](../../load/topics/g-escaping.html)

