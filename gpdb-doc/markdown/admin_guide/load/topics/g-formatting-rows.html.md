---
title: Formatting Rows 
---

Greenplum Database expects rows of data to be separated by the `LF` character \(Line feed, `0x0A`\), `CR` \(Carriage return, `0x0D`\), or `CR` followed by `LF` \(`CR+LF`, `0x0D 0x0A`\). `LF` is the standard newline representation on UNIX or UNIX-like operating systems. Operating systems such as Windows or Mac OS X use `CR` or `CR+LF`. All of these representations of a newline are supported by Greenplum Database as a row delimiter. For more information, see [Importing and Exporting Fixed Width Data](g-importing-and-exporting-fixed-width-data.html).

**Parent topic:** [Formatting Data Files](../../load/topics/g-formatting-data-files.html)

