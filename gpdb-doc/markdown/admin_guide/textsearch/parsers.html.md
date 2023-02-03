---
title: Text Search Parsers 
---

This topic describes the types of tokens the Greenplum Database text search parser produces from raw text.

Text search parsers are responsible for splitting raw document text into *tokens* and identifying each token's type, where the set of possible types is defined by the parser itself. Note that a parser does not modify the text at all — it simply identifies plausible word boundaries. Because of this limited scope, there is less need for application-specific custom parsers than there is for custom dictionaries. At present Greenplum Database provides just one built-in parser, which has been found to be useful for a wide range of applications.

The built-in parser is named `pg_catalog.default`. It recognizes 23 token types, shown in the following table.

|Alias|Description|Example|
|-----|-----------|-------|
|asciiword|Word, all ASCII letters|elephant|
|word|Word, all letters|mañana|
|numword|Word, letters and digits|beta1|
|asciihword|Hyphenated word, all ASCII|up-to-date|
|hword|Hyphenated word, all letters|lógico-matemática|
|numhword|Hyphenated word, letters and digits|postgresql-beta1|
|hword\_asciipart|Hyphenated word part, all ASCII|postgresql in the context postgresql-beta1|
|hword\_part|Hyphenated word part, all letters|lógico or matemática in the context lógico-matemática|
|hword\_numpart|Hyphenated word part, letters and digits|beta1 in the context postgresql-beta1|
|email|Email address|foo@example.com|
|protocol|Protocol head|http://|
|url|URL|example.com/stuff/index.html|
|host|Host|example.com|
|url\_path|URL path|/stuff/index.html, in the context of a URL|
|file|File or path name|/usr/local/foo.txt, if not within a URL|
|sfloat|Scientific notation|-1.234e56|
|float|Decimal notation|-1.234|
|int|Signed integer|-1234|
|uint|Unsigned integer|1234|
|version|Version number|8.3.0|
|tag|XML tag|\<a href="dictionaries.html"\>|
|entity|XML entity|\&amp;|
|blank|Space symbols|\(any whitespace or punctuation not otherwise recognized\)|

> **Note** The parser's notion of a "letter" is determined by the database's locale setting, specifically `lc_ctype`. Words containing only the basic ASCII letters are reported as a separate token type, since it is sometimes useful to distinguish them. In most European languages, token types `word` and `asciiword` should be treated alike.

`email` does not support all valid email characters as defined by RFC 5322. Specifically, the only non-alphanumeric characters supported for email user names are period, dash, and underscore.

It is possible for the parser to produce overlapping tokens from the same piece of text. As an example, a hyphenated word will be reported both as the entire word and as each component:

```
SELECT alias, description, token FROM ts_debug('foo-bar-beta1');
      alias      |               description                |     token     
-----------------+------------------------------------------+---------------
 numhword        | Hyphenated word, letters and digits      | foo-bar-beta1
 hword_asciipart | Hyphenated word part, all ASCII          | foo
 blank           | Space symbols                            | -
 hword_asciipart | Hyphenated word part, all ASCII          | bar
 blank           | Space symbols                            | -
 hword_numpart   | Hyphenated word part, letters and digits | beta1
```

This behavior is desirable since it allows searches to work for both the whole compound word and for components. Here is another instructive example:

```
SELECT alias, description, token FROM ts_debug('http://example.com/stuff/index.html');
  alias   |  description  |            token             
----------+---------------+------------------------------
 protocol | Protocol head | http://
 url      | URL           | example.com/stuff/index.html
 host     | Host          | example.com
 url_path | URL path      | /stuff/index.html
```

**Parent topic:** [Using Full Text Search](../textsearch/full-text-search.html)

