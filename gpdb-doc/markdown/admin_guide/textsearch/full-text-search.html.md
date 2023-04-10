---
title: Using Full Text Search 
---

Greenplum Database provides data types, functions, operators, index types, and configurations for querying natural language documents.

-   **[About Full Text Search](../textsearch/intro.html)**  
This topic provides an overview of Greenplum Database full text search, basic text search expressions, configuring, and customizing text search. Greenplum Database full text search is compared with VMware Greenplum Text.
-   **[Searching Text in Database Tables](../textsearch/tables-indexes.html)**  
This topic shows how to use text search operators to search database tables and how to create indexes to speed up text searches.
-   **[Controlling Text Search](../textsearch/controlling.html)**  
This topic shows how to create search and query vectors, how to rank search results, and how to highlight search terms in the results of text search queries.
-   **[Additional Text Search Features](../textsearch/features.html)**  
Greenplum Database has additional functions and operators you can use to manipulate search and query vectors, and to rewrite search queries.
-   **[Text Search Parsers](../textsearch/parsers.html)**  
This topic describes the types of tokens the Greenplum Database text search parser produces from raw text.
-   **[Text Search Dictionaries](../textsearch/dictionaries.html)**  
Tokens produced by the Greenplum Database full text search parser are passed through a chain of dictionaries to produce a normalized term or "lexeme". Different kinds of dictionaries are available to filter and transform tokens in different ways and for different languages.
-   **[Text Search Configuration Example](../textsearch/configuration.html)**  
This topic shows how to create a customized text search configuration to process document and query text.
-   **[Testing and Debugging Text Search](../textsearch/testing.html)**  
This topic introduces the Greenplum Database functions you can use to test and debug a search configuration or the individual parser and dictionaries specified in a configuration.
-   **[GiST and GIN Indexes for Text Search](../textsearch/gist-gin.html)**  
This topic describes and compares the Greenplum Database index types that are used for full text searching.
-   **[psql Support](../textsearch/psql-support.html)**  
The psql command-line utility provides a meta-command to display information about Greenplum Database full text search configurations.
-   **[Limitations](../textsearch/limitations.html)**  
This topic lists limitations and maximums for Greenplum Database full text search objects.

**Parent topic:** [Querying Data](../query/topics/query.html)

