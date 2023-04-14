---
title: Escaping in Text Formatted Files 
---

By default, the escape character is a \\ \(backslash\) for text-formatted files. You can declare a different escape character in the `ESCAPE` clause of `COPY`, `CREATE EXTERNAL TABLE`or `gpload`. If your escape character appears in your data, use it to escape itself.

For example, suppose you have a table with three columns and you want to load the following three fields:

-   `backslash = \`
-   `vertical bar = |`
-   `exclamation point = !`

Your designated delimiter character is `|` \(pipe character\), and your designated escape character is `\` \(backslash\). The formatted row in your data file looks like this:

```
backslash = \\ | vertical bar = \| | exclamation point = !

```

Notice how the backslash character that is part of the data is escaped with another backslash character, and the pipe character that is part of the data is escaped with a backslash character.

You can use the escape character to escape octal and hexadecimal sequences. The escaped value is converted to the equivalent character when loaded into Greenplum Database. For example, to load the ampersand character \(`&`\), use the escape character to escape its equivalent hexadecimal \(`\0x26`\) or octal \(`\046`\) representation.

You can deactivate escaping in `TEXT`-formatted files using the `ESCAPE` clause of `COPY`, `CREATE EXTERNAL TABLE`or `gpload` as follows:

```
ESCAPE 'OFF'

```

This is useful for input data that contains many backslash characters, such as web log data.

**Parent topic:** [Escaping](../../load/topics/g-escaping.html)

