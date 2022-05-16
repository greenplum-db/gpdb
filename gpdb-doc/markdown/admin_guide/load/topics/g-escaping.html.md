---
title: Escaping 
---

There are two reserved characters that have special meaning to Greenplum Database:

-   The designated delimiter character separates columns or fields in the data file.
-   The newline character designates a new row in the data file.

If your data contains either of these characters, you must escape the character so that Greenplum treats it as data and not as a field separator or new row. By default, the escape character is a \\ \(backslash\) for text-formatted files and a double quote \("\) for csv-formatted files.

-   **[Escaping in Text Formatted Files](../../load/topics/g-escaping-in-text-formatted-files.html)**  

-   **[Escaping in CSV Formatted Files](../../load/topics/g-escaping-in-csv-formatted-files.html)**  


**Parent topic:** [Formatting Data Files](../../load/topics/g-formatting-data-files.html)

