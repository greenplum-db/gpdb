---
title: About Full Text Search 
---

This topic provides an overview of Greenplum Database full text search, basic text search expressions, configuring, and customizing text search. Greenplum Database full text search is compared with VMware Greenplum Text.

This section contains the following subtopics:

-   [What is a Document?](#document)
-   [Basic Text Matching](#basic-text-matching)
-   [Configurations](#configurations)
-   [Comparing Greenplum Database Text Search with VMware Greenplum Text](#gptext)

Full Text Searching \(or just "text search"\) provides the capability to identify natural-language *documents* that satisfy a *query*, and optionally to rank them by relevance to the query. The most common type of search is to find all documents containing given *query terms* and return them in order of their *similarity* to the query.

Greenplum Database provides a data type `tsvector` to store preprocessed documents, and a data type `tsquery` to store processed queries \([Text Search Data Types](../../ref_guide/datatype-textsearch.html)\). There are many functions and operators available for these data types \([Text Search Functions and Operators](../../ref_guide/function-summary.html)\), the most important of which is the match operator `@@`, which we introduce in [Basic Text Matching](#basic-text-matching). Full text searches can be accelerated using indexes \([GiST and GIN Indexes for Text Search](gist-gin.html)\).

Notions of query and similarity are very flexible and depend on the specific application. The simplest search considers query as a set of words and similarity as the frequency of query words in the document.

Greenplum Database supports the standard text matching operators `~`, `~*`, `LIKE`, and `ILIKE` for textual data types, but these operators lack many essential properties required for searching documents:

-   There is no linguistic support, even for English. Regular expressions are not sufficient because they cannot easily handle derived words, e.g., `satisfies` and `satisfy`. You might miss documents that contain `satisfies`, although you probably would like to find them when searching for `satisfy`. It is possible to use OR to search for multiple derived forms, but this is tedious and error-prone \(some words can have several thousand derivatives\).
-   They provide no ordering \(ranking\) of search results, which makes them ineffective when thousands of matching documents are found.

-   They tend to be slow because there is no index support, so they must process all documents for every search.


Full text indexing allows documents to be preprocessed and an index saved for later rapid searching. Preprocessing includes:

-   **Parsing documents into tokens.** It is useful to identify various classes of tokens, e.g., numbers, words, complex words, email addresses, so that they can be processed differently. In principle token classes depend on the specific application, but for most purposes it is adequate to use a predefined set of classes. Greenplum Database uses a *parser* to perform this step. A standard parser is provided, and custom parsers can be created for specific needs.
-   **Converting tokens into lexemes.** A lexeme is a string, just like a token, but it has been *normalized* so that different forms of the same word are made alike. For example, normalization almost always includes folding upper-case letters to lower-case, and often involves removal of suffixes \(such as s or es in English\). This allows searches to find variant forms of the same word, without tediously entering all the possible variants. Also, this step typically eliminates *stop words*, which are words that are so common that they are useless for searching. \(In short, then, tokens are raw fragments of the document text, while lexemes are words that are believed useful for indexing and searching.\) Greenplum Database uses *dictionaries* to perform this step. Various standard dictionaries are provided, and custom ones can be created for specific needs.
-   **Storing preprocessed documents optimized for searching.** For example, each document can be represented as a sorted array of normalized lexemes. Along with the lexemes it is often desirable to store positional information to use for *proximity ranking*, so that a document that contains a more "dense" region of query words is assigned a higher rank than one with scattered query words.

Dictionaries allow fine-grained control over how tokens are normalized. With appropriate dictionaries, you can:

-   Define stop words that should not be indexed.
-   Map synonyms to a single word using Ispell.
-   Map phrases to a single word using a thesaurus.
-   Map different variations of a word to a canonical form using an Ispell dictionary.
-   Map different variations of a word to a canonical form using Snowball stemmer rules.

## <a id="document"></a>What is a Document? 

A *document* is the unit of searching in a full text search system; for example, a magazine article or email message. The text search engine must be able to parse documents and store associations of lexemes \(key words\) with their parent document. Later, these associations are used to search for documents that contain query words.

For searches within Greenplum Database, a document is normally a textual field within a row of a database table, or possibly a combination \(concatenation\) of such fields, perhaps stored in several tables or obtained dynamically. In other words, a document can be constructed from different parts for indexing and it might not be stored anywhere as a whole. For example:

```
SELECT title || ' ' ||  author || ' ' ||  abstract || ' ' || body AS document
FROM messages
WHERE mid = 12;

SELECT m.title || ' ' || m.author || ' ' || m.abstract || ' ' || d.body AS document
FROM messages m, docs d
WHERE mid = did AND mid = 12;
```

> **Note** In these example queries, `coalesce` should be used to prevent a single `NULL` attribute from causing a `NULL` result for the whole document.

Another possibility is to store the documents as simple text files in the file system. In this case, the database can be used to store the full text index and to run searches, and some unique identifier can be used to retrieve the document from the file system. However, retrieving files from outside the database requires superuser permissions or special function support, so this is usually less convenient than keeping all the data inside Greenplum Database. Also, keeping everything inside the database allows easy access to document metadata to assist in indexing and display.

For text search purposes, each document must be reduced to the preprocessed `tsvector` format. Searching and ranking are performed entirely on the tsvector representation of a document — the original text need only be retrieved when the document has been selected for display to a user. We therefore often speak of the `tsvector` as being the document, but of course it is only a compact representation of the full document.

## <a id="basic-text-matching"></a>Basic Text Matching 

Full text searching in Greenplum Database is based on the match operator `@@`, which returns `true` if a `tsvector` \(document\) matches a `tsquery` \(query\). It does not matter which data type is written first:

```
SELECT 'a fat cat sat on a mat and ate a fat rat'::tsvector @@ 'cat & rat'::tsquery;
 ?column?
----------
 t

SELECT 'fat & cow'::tsquery @@ 'a fat cat sat on a mat and ate a fat rat'::tsvector;
 ?column?
----------
 f
```

As the above example suggests, a `tsquery` is not just raw text, any more than a `tsvector` is. A `tsquery` contains search terms, which must be already-normalized lexemes, and may combine multiple terms using AND, OR, and NOT operators. \(For details see.\) There are functions `to_tsquery` and `plainto_tsquery` that are helpful in converting user-written text into a proper `tsquery`, for example by normalizing words appearing in the text. Similarly, `to_tsvector` is used to parse and normalize a document string. So in practice a text search match would look more like this:

```
SELECT to_tsvector('fat cats ate fat rats') @@ to_tsquery('fat & rat');
 ?column? 
----------
 t
```

Observe that this match would not succeed if written as

```
SELECT 'fat cats ate fat rats'::tsvector @@ to_tsquery('fat & rat');
 ?column? 
----------
 f
```

since here no normalization of the word `rats` will occur. The elements of a `tsvector` are lexemes, which are assumed already normalized, so `rats` does not match `rat`.

The `@@` operator also supports `text` input, allowing explicit conversion of a text string to `tsvector` or `tsquery` to be skipped in simple cases. The variants available are:

```
tsvector @@ tsquery
tsquery  @@ tsvector
text @@ tsquery
text @@ text
```

The first two of these we saw already. The form `text @@ tsquery` is equivalent to `to_tsvector(x) @@ y`. The form `text @@ text` is equivalent to `to_tsvector(x) @@ plainto_tsquery(y)`.

## <a id="configurations"></a>Configurations 

The above are all simple text search examples. As mentioned before, full text search functionality includes the ability to do many more things: skip indexing certain words \(stop words\), process synonyms, and use sophisticated parsing, e.g., parse based on more than just white space. This functionality is controlled by *text search configurations*. Greenplum Database comes with predefined configurations for many languages, and you can easily create your own configurations. \(psql's `\dF` command shows all available configurations.\)

During installation an appropriate configuration is selected and [default\_text\_search\_config](../../ref_guide/config_params/guc-list.html) is set accordingly in `postgresql.conf`. If you are using the same text search configuration for the entire cluster you can use the value in `postgresql.conf`. To use different configurations throughout the cluster but the same configuration within any one database, use`ALTER DATABASE ... SET`. Otherwise, you can set `default_text_search_config` in each session.

Each text search function that depends on a configuration has an optional `regconfig` argument, so that the configuration to use can be specified explicitly. `default_text_search_config` is used only when this argument is omitted.

To make it easier to build custom text search configurations, a configuration is built up from simpler database objects. Greenplum Database's text search facility provides four types of configuration-related database objects:

-   *Text search parsers* break documents into tokens and classify each token \(for example, as words or numbers\).
-   *Text search dictionaries* convert tokens to normalized form and reject stop words.
-   *Text search templates* provide the functions underlying dictionaries. \(A dictionary simply specifies a template and a set of parameters for the template.\)
-   *Text search configurations* select a parser and a set of dictionaries to use to normalize the tokens produced by the parser.

Text search parsers and templates are built from low-level C functions; therefore it requires C programming ability to develop new ones, and superuser privileges to install one into a database. \(There are examples of add-on parsers and templates in the `contrib/` area of the Greenplum Database distribution.\) Since dictionaries and configurations just parameterize and connect together some underlying parsers and templates, no special privilege is needed to create a new dictionary or configuration. Examples of creating custom dictionaries and configurations appear later in this chapter.

## <a id="gptext"></a>Comparing Greenplum Database Text Search with VMware Greenplum Text 

Greenplum Database text search is PostgreSQL text search ported to the Greenplum Database MPP platform. VMware also offers VMware Greenplum Text, which integrates Greenplum Database with the Apache Solr text search platform. VMware Greenplum Text installs an Apache Solr cluster alongside your Greenplum Database cluster and provides Greenplum Database functions you can use to create Solr indexes, query them, and receive results in the database session.

Both of these systems provide powerful, enterprise-quality document indexing and searching services. Greenplum Database text search is immediately available to you, with no need to install and maintain additional software. If it meets your applications' requirements, you should use it.

VMware Greenplum Text, with Solr, has many capabilities that are not available with Greenplum Database text search. In particular, it is better for advanced text analysis applications. Following are some of the advantages and capabilities available to you when you use VMware Greenplum Text for text search applications.

-   The Apache Solr cluster can be scaled separately from the database. Solr nodes can be deployed on the Greenplum Database hosts or on separate hosts on the network.
-   Indexing and search workloads can be moved out of Greenplum Database to Solr to maintain database query performance.
-   VMware Greenplum Text creates Solr indexes that are split into *shards*, one per Greenplum Database segment, so the advantages of the Greenplum Database MPP architecture are extended to text search workloads.
-   Indexing and searching documents with Solr is very fast and can be scaled by adding more Solr nodes to the cluster.
-   Document content can be stored in Greenplum Database tables, in the Solr index, or both.
-   Through VMware Greenplum Text, Solr can index documents stored as text in Greenplum Database tables, as well as documents in external stores accessible using HTTP, FTP, S3, or HDFS URLs.
-   Solr automatically recognizes most rich document formats and indexes document content and metadata separately.
-   Solr indexes are highly customizable. You can customize the text analysis chain down to the field level.
-   In addition to the large number of languages, tokenizers, and filters available from the Apache project, VMware Greenplum Text provides a social media tokenizer, an international text tokenizer, and a universal query parser that understands several common text search syntaxes.
-   The VMware Greenplum Text API supports advanced text analysis tools, such as facetting, named entity recognition \(NER\), and parts of speech \(POS\) recognition.

See the [VMware Greenplum Text Documentation web site](https://docs.vmware.com/en/VMware-Tanzu-Greenplum-Text/index.html) for more information.

**Parent topic:** [Using Full Text Search](../textsearch/full-text-search.html)

