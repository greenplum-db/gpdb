# pgvector

A machine language-generated embedding is a complex object transformed into a list of numbers (vector) that reflects both the semantic and syntactic relationships of the data. The `pgvector` module provides vector similarity search capabilities for Greenplum Database that enable you to search, store, and query embeddings at large scale.

The Greenplum Database `pgvector` module is equivalent to version 0.5.0 of the `pgvector` module used with PostgreSQL. The limitations of the Greenplum version of the module are described in the [Greenplum Database Limitations](#limits) topic.


## <a id="topic_reg"></a>Installing and Registering the Module

The `pgvector` module is installed when you install Greenplum Database. Before you can use the data type and index access method defined in the module, you must register the `vector` extension in each database in which you want to use these:

```
CREATE EXTENSION vector;
```

Refer to [Installing Additional Supplied Modules](../../../install_guide/install_modules.html) for more information.


## <a id="using"></a>About the vector Types, Operators, and Functions

`pgvector` provides a `vector` data type and the index access methods `ivfflat` and `hnsw`. The type, methods, and the supporting functions and operators provided by the module enable you to perform exact and approximate neighbor search on, and determine L2, inner product, and cosine distance between, embeddings. You can also use the module to store and query embeddings.

### <a id="datatype"></a>vector Data Type

The `vector` data type represents an n-dimensional coordinate. Each `vector` takes `4 * dimensions + 8` bytes of storage. Each element is a single precision floating-point number (similar to the `real` type in Greenplum Database), and all of the elements must be finite (no `NaN`, `Infinity`, or `-Infinity`). Vectors can have up to 16,000 dimensions.

### <a id="ops"></a>vector Operators

`pgvector` provides the following operators for the `vector` data type:

Operator | Description
--- | ---
\+ | Element-wise addition
\- | Element-wise subtraction
\* | Element-wise multiplication
&lt;&ndash;&gt; | Euclidean distance
<#><sup>1</sup> | Negative inner product
&lt;&equals;&gt; | Cosine distance

<sup>1</sup> Because Greenplum Database supports only `ASC` order index scans on operators, `<#>` returns the *negative* inner product.

### <a id="funcs"></a>vector Functions

`pgvector` provides the following functions for the `vector data type:

Function Name | Description
--- | ---
cosine_distance(vector, vector) → double precision | Computes the cosine distance
inner_product(vector, vector) → double precision | Computes the inner product
l2_distance(vector, vector) → double precision | Computes the Euclidean distance
l1_distance(vector, vector) → double precision | Computes the taxicab distance
vector_dims(vector) → integer | Returns the number of dimensions
vector_norm(vector) → double precision | Computes the Euclidean norm

### <a id="agg_funcs"></a>vector Aggregate Functions

`pgvector` provides the following aggregate functions for the `vector` data type:

Function | Description
--- | ---
avg(vector) → vector | Computes the arithmetic mean
sum(vector) → vector | Computes the sum of the vector elements


## <a id="Using"></a>Using the pgvector Module

You can use `pgvector` to search, store, and query embeddings in Greenplum Database.


### <a id="storing"></a>Examples: Storing Embeddings in Greenplum Database

In the following examples, you manipulate a `vector` column of a table.

Create a new table with a `vector` column with 3 dimensions:

``` sql
CREATE TABLE items (id bigserial PRIMARY KEY, embedding vector(3));
```

Or add a `vector` column to an existing table:

``` sql
ALTER TABLE items ADD COLUMN embedding vector(3);
```

Insert `vector`s into the table:

``` sql
INSERT INTO items (embedding) VALUES ('[1,2,3]'), ('[4,5,6]');
```

Upsert `vector`s:

``` sql
INSERT INTO items (id, embedding) VALUES (1, '[1,2,3]'), (2, '[4,5,6]')
    ON CONFLICT (id) DO UPDATE SET embedding = EXCLUDED.embedding;
```

Update `vector`s:

``` sql
UPDATE items SET embedding = '[1,2,3]' WHERE id = 1;
```

Delete `vector`s:

``` sql
DELETE FROM items WHERE id = 1;
```

### <a id="querying"></a>Examples: Querying Embeddings in Greenplum Database

You can query embeddings as follows.

Get the nearest neighbors to a `vector` by L2 distance:

``` sql
SELECT * FROM items ORDER BY embedding <-> '[3,1,2]' LIMIT 5;
```

Get the nearest neighbors to a row:

``` sql
SELECT * FROM items WHERE id != 1 
  ORDER BY embedding <-> (SELECT embedding FROM items WHERE id = 1) LIMIT 5;
```

Get rows within a certain distance:

``` sql
SELECT * FROM items WHERE embedding <-> '[3,1,2]' < 5;
```

Combine with the `ORDER BY` and `LIMIT` clauses to use an index.

#### <a id="distance"></a>Evaluating Embedding Distance

The following examples use the available `vector` distance operators.

Get the distance:

``` sql
SELECT embedding <-> '[3,1,2]' AS distance FROM items;
```

When you request the inner product, remember to multiply by `-1`:

``` sql
SELECT (embedding <#> '[3,1,2]') * -1 AS inner_product FROM items;
```

For cosine similarity, use `1 - <cosine_distance>`:

``` sql
SELECT 1 - (embedding <=> '[3,1,2]') AS cosine_similarity FROM items;
```

#### <a id="aggs"></a>Aggregating Embeddings

The following examples display various forms of aggregating embeddings.

Average the vectors in a table:

``` sql
SELECT AVG(embedding) FROM items;
```

Average a group of vectors in a table:

``` sql
SELECT category_id, AVG(embedding) FROM items GROUP BY category_id;
```

### <a id="indexing"></a>About Indexing Embeddings

By default, `pgvector` performs exact nearest neighbor search, which provides perfect recall. You can add an index to use approximate nearest neighbor search, trading some recall for performance.

> **Note** Unlike a typical index, a query returns different results after adding an approximate index.

When you create an index for an embedding, you use the `lists` parameter to specify the number of *clusters* created during index creation. Each cluster is a partition of the data set.

To achieve good recall, keep the following in mind:

1. Create the index *after* the table has some data.
2. Choose an appropriate number of `lists`. A reasonable initial value is `rows / 1000` for up to 1M rows and `sqrt(rows)` for over 1M rows.
3. When querying, specify an appropriate number of [probes](#query-options) (higher is better for recall, lower is better for speed). A reasonable initial value is `sqrt(lists)`.

The following examples show how to add an index for various distance methods.

#### IVFFlat Examples

Create an index on the L2 distance:

``` sql
CREATE INDEX ON items USING ivfflat (embedding vector_l2_ops) WITH (lists = 100);
```

Create an index on the inner product:

``` sql
CREATE INDEX ON items USING ivfflat (embedding vector_ip_ops) WITH (lists = 100);
```

Create an index on the cosine distance:

``` sql
CREATE INDEX ON items USING ivfflat (embedding vector_cosine_ops) WITH (lists = 100);
```

You can index a `vector` that has up to 2,000 dimensions.

**Query Options**

`pgvector` provides a `probes` parameter that you can set at query time to specify the number of regions to search during a query.

Specify the number of `probes` (1 by default):

``` sql
SET ivfflat.probes = 10;
```

A higher `probes` value provides better recall at the cost of speed. You can set it to the number of `lists` for exact nearest neighbor search (at which point the planner will not use the index).

Use `SET LOCAL` inside a transaction block to set `probes` for a single query:

``` sql
BEGIN;
SET LOCAL ivfflat.probes = 10;
SELECT ...
COMMIT;
```

#### HNSW Examples

Create an index on the L2 distance:

``` sql
CREATE INDEX ON items USING hnsw (embedding vector_l2_ops);
```

Create an index on the inner product:

``` sql
CREATE INDEX ON items USING hnsw (embedding vector_ip_ops);
```

Create an index on the cosine distance:

``` sql
CREATE INDEX ON items USING hnsw (embedding vector_cosine_ops);
```

You can index a `vector` that has up to 2,000 dimensions.

HNSW indexes support the following parameters:

- `m` specifies the maximum number of connections per layer (16 by default)
- `ef_construction` specifies the size of the dynamic candidate list for constructing the graph (64 by default)

For example:

```
CREATE INDEX ON items USING hnsw (embedding vector_l2_ops) WITH (m = 16, ef_construction = 64);
```

**Query Options**

Specify a csutom size for the dynamic candidate list for a search:

```
SET hnsw.ef_search = 100;
```

Higher value provides better recall at the cost of speed. The default size of the candidate list is 40.

This example sets the candidate size in a transaction for a single query:

```
BEGIN;
SET LOCAL hnsw.ef_search = 100;
SELECT ...
COMMIT;
```

#### <a id="index_prog"></a>Indexing Progress

You can check index creation progress in Greenplum Database as described in [CREATE INDEX Progress Reporting](../../../admin_guide/managing/progress_reporting.html#create_index_progress).

``` sql
SELECT phase, tuples_done, tuples_total FROM gp_stat_progress_create_index;
```

#### <a id="filtering"></a>Filtering

There are multiple ways to index nearest neighbor queries with a `WHERE` clause:

``` sql
SELECT * FROM items WHERE category_id = 123 ORDER BY embedding <-> '[3,1,2]' LIMIT 5;
```

For exact search, create an index on one or more of the `WHERE` columns:

``` sql
CREATE INDEX ON items (category_id);
```

For approximate search, create a partial index on the `vector` column:

``` sql
CREATE INDEX ON items USING ivfflat (embedding vector_l2_ops) WITH (lists = 100)
    WHERE (category_id = 123);
```

Use [partitioning](../../../admin_guide/ddl/ddl-partition.html) for approximate search on many different values of the `WHERE` columns:

``` sql
CREATE TABLE items (embedding vector(3), category_id int) PARTITION BY LIST(category_id);
```

### <a id="hybrid_search"></a>About Hybrid Search

You can use `pgvector` together with [full-text search](../../../admin_guide/textsearch/full-text-search.html) for a hybrid search:

``` sql
SELECT id, content FROM items, to_tsquery('hello & search') query
    WHERE textsearch @@ query ORDER BY ts_rank_cd(textsearch, query) DESC LIMIT 5;
```

### <a id="debug_perf"></a>About Debugging and Maximizing Performance

Use [EXPLAIN ANALYZE](../../sql_commands/EXPLAIN.html) to debug performance:

``` sql
EXPLAIN ANALYZE SELECT * FROM items ORDER BY embedding <-> '[3,1,2]' LIMIT 5;
```

#### <a id="exact_search"></a>Exact Search

To speed up queries without an index, increase the `max_parallel_workers_per_gather` server configuration parameter:

``` sql
SET max_parallel_workers_per_gather = 4;
```

If vectors are normalized to length `1`, use inner product for the best performance:

``` sql
SELECT * FROM items ORDER BY embedding <#> '[3,1,2]' LIMIT 5;
```

#### <a id="approx_search"></a>Approximate Search

To speed up queries with an index, increase the number of inverted `lists` (at the expense of recall):

``` sql
CREATE INDEX ON items USING ivfflat (embedding vector_l2_ops) WITH (lists = 1000);
```


## <a id="limits"></a>Greenplum Database Limitations

`pgvector` for Greenplum Database has the following limitations:

- The Greenplum query optimizer (GPORCA) does not support `ivfflat` and `hnsw` vector indexes. Queries on tables that utilize these index types fall back to the Postgres-based planner.
- Append-optimized tables cannot use vector indexes.
- The size of (a vector) index can be larger than the table size.


## <a id="addtl_refs"></a>Additional References

The following examples use `pgvector` and the VMware Greenplum documentation to build an AI assistant for the product documentation:

- [Building large-scale AI-powered search in Greenplum using pgvector and OpenAI](https://medium.com/greenplum-data-clinics/building-large-scale-ai-powered-search-in-greenplum-using-pgvector-and-openai-4f5c5811f54a)
- [ask-greenplum](https://github.com/yihong0618/ask-greenplum)

