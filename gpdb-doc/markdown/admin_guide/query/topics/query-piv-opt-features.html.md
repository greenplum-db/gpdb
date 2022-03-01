---
title: GPORCA Features and Enhancements 
---

GPORCA, the Greenplum next generation query optimizer, includes enhancements for specific types of queries and operations:

-   [Queries Against Partitioned Tables](#topic_dwy_zml_gr)
-   [Queries that Contain Subqueries](#topic_vph_wml_gr)
-   [Queries that Contain Common Table Expressions](#topic_c3v_rml_gr)
-   [DML Operation Enhancements with GPORCA](#topic_plx_mml_gr)

GPORCA also includes these optimization enhancements:

-   Improved join ordering
-   Join-Aggregate reordering
-   Sort order optimization
-   Data skew estimates included in query optimization

**Parent topic:**[About GPORCA](../../query/topics/query-piv-optimizer.html)

## <a id="topic_dwy_zml_gr"></a>Queries Against Partitioned Tables 

GPORCA includes these enhancements for queries against partitioned tables:

-   Partition elimination is improved.
-   Uniform multi-level partitioned tables are supported. For information about uniform multi-level partitioned tables, see [About Uniform Multi-level Partitioned Tables](query-piv-uniform-part-tbl.html)
-   Query plan can contain the Partition selector operator.
-   Partitions are not enumerated in EXPLAIN plans.

    For queries that involve static partition selection where the partitioning key is compared to a constant, GPORCA lists the number of partitions to be scanned in the `EXPLAIN` output under the Partition Selector operator. This example Partition Selector operator shows the filter and number of partitions selected:

    ```
    Partition Selector for Part_Table (dynamic scan id: 1) 
           Filter: a > 10
           Partitions selected:  1 (out of 3)
    ```

    For queries that involve dynamic partition selection where the partitioning key is compared to a variable, the number of partitions that are scanned will be known only during query execution. The partitions selected are not shown in the `EXPLAIN` output.

-   Plan size is independent of number of partitions.
-   Out of memory errors caused by number of partitions are reduced.

This example CREATE TABLE command creates a range partitioned table.

```
CREATE TABLE sales(order_id int, item_id int, amount numeric(15,2), 
      date date, yr_qtr int)
   PARTITION BY RANGE (yr_qtr) (start (201501) INCLUSIVE end (201504) INCLUSIVE, 
   start (201601) INCLUSIVE end (201604) INCLUSIVE,
   start (201701) INCLUSIVE end (201704) INCLUSIVE,     
   start (201801) INCLUSIVE end (201804) INCLUSIVE,
   start (201901) INCLUSIVE end (201904) INCLUSIVE,
   start (202001) INCLUSIVE end (202004) INCLUSIVE);
```

GPORCA improves on these types of queries against partitioned tables:

-   Full table scan. Partitions are not enumerated in plans.

    ```
    SELECT * FROM sales;
    ```

-   Query with a constant filter predicate. Partition elimination is performed.

    ```
    SELECT * FROM sales WHERE yr_qtr = 201501;
    ```

-   Range selection. Partition elimination is performed.

    ```
    SELECT * FROM sales WHERE yr_qtr BETWEEN 201601 AND 201704 ;
    ```

-   Joins involving partitioned tables. In this example, the partitioned dimension table *date\_dim* is joined with fact table *catalog\_sales*:

    ```
    SELECT * FROM catalog_sales
       WHERE date_id IN (SELECT id FROM date_dim WHERE month=12);
    ```


## <a id="topic_vph_wml_gr"></a>Queries that Contain Subqueries 

GPORCA handles subqueries more efficiently. A subquery is query that is nested inside an outer query block. In the following query, the SELECT in the WHERE clause is a subquery.

```
SELECT * FROM part
  WHERE price > (SELECT avg(price) FROM part);
```

GPORCA also handles queries that contain a correlated subquery \(CSQ\) more efficiently. A correlated subquery is a subquery that uses values from the outer query. In the following query, the `price` column is used in both the outer query and the subquery. 

```
SELECT * FROM part p1 WHERE price > (SELECT avg(price) FROM part p2 WHERE p2.brand = p1.brand);
```

GPORCA generates more efficient plans for the following types of subqueries:

-   CSQ in the SELECT list.

    ```
    SELECT *,
     (SELECT min(price) FROM part p2 WHERE p1.brand = p2.brand)
     AS foo
    FROM part p1;
    ```

-   CSQ in disjunctive \(OR\) filters.

    ```
    SELECT FROM part p1 WHERE p_size > 40 OR 
          p_retailprice > 
          (SELECT avg(p_retailprice) 
              FROM part p2 
              WHERE p2.p_brand = p1.p_brand)
    ```

-   Nested CSQ with skip level correlations

    ```
    SELECT * FROM part p1 WHERE p1.p_partkey 
    IN (SELECT p_partkey FROM part p2 WHERE p2.p_retailprice = 
         (SELECT min(p_retailprice)
           FROM part p3 
           WHERE p3.p_brand = p1.p_brand)
    );
    ```

    **Note:** Nested CSQ with skip level correlations are not supported by the Postgres Planner.

-   CSQ with aggregate and inequality. This example contains a CSQ with an inequality.

    ```
    SELECT * FROM part p1 WHERE p1.p_retailprice =
     (SELECT min(p_retailprice) FROM part p2 WHERE p2.p_brand <> p1.p_brand);
    ```


-   CSQ that must return one row.

    ```
    SELECT p_partkey, 
      (SELECT p_retailprice FROM part p2 WHERE p2.p_brand = p1.p_brand )
    FROM part p1;
    ```


## <a id="topic_c3v_rml_gr"></a>Queries that Contain Common Table Expressions 

GPORCA handles queries that contain the WITH clause. The WITH clause, also known as a common table expression \(CTE\), generates temporary tables that exist only for the query. This example query contains a CTE.

```
WITH v AS (SELECT a, sum(b) as s FROM T where c < 10 GROUP BY a)
  SELECT *FROM  v AS v1 ,  v AS v2
  WHERE v1.a <> v2.a AND v1.s < v2.s;
```

As part of query optimization, GPORCA can push down predicates into a CTE. For example query, GPORCA pushes the equality predicates to the CTE.

```
WITH v AS (SELECT a, sum(b) as s FROM T GROUP BY a)
  SELECT *
  FROM v as v1, v as v2, v as v3
  WHERE v1.a < v2.a
    AND v1.s < v3.s
    AND v1.a = 10
    AND v2.a = 20
    AND v3.a = 30;
```

GPORCA can handle these types of CTEs:

-   CTE that defines one or multiple tables. In this query, the CTE defines two tables.

    ```
    WITH cte1 AS (SELECT a, sum(b) as s FROM T 
                   where c < 10 GROUP BY a),
          cte2 AS (SELECT a, s FROM cte1 where s > 1000)
      SELECT *
      FROM cte1 as v1, cte2 as v2, cte2 as v3
      WHERE v1.a < v2.a AND v1.s < v3.s;
    ```

-   Nested CTEs.

    ```
    WITH v AS (WITH w AS (SELECT a, b FROM foo 
                          WHERE b < 5) 
               SELECT w1.a, w2.b 
               FROM w AS w1, w AS w2 
               WHERE w1.a = w2.a AND w1.a > 2)
      SELECT v1.a, v2.a, v2.b
      FROM v as v1, v as v2
      WHERE v1.a < v2.a; 
    ```


## <a id="topic_plx_mml_gr"></a>DML Operation Enhancements with GPORCA 

GPORCA contains enhancements for DML operations such as INSERT, UPDATE, and DELETE.

-   A DML node in a query plan is a query plan operator.
    -   Can appear anywhere in the plan, as a regular node \(top slice only for now\)
    -   Can have consumers
-   UPDATE operations use the query plan operator Split and supports these operations:

    -   UPDATE operations on the table distribution key columns.
    -   UPDATE operations on the table on the partition key column.
    This example plan shows the Split operator.

    ```
    QUERY PLAN
    --------------------------------------------------------------
    Update  (cost=0.00..5.46 rows=1 width=1)
       ->  Redistribute Motion 2:2  (slice1; segments: 2)
             Hash Key: a
             ->  Result  (cost=0.00..3.23 rows=1 width=48)
                   ->  Split  (cost=0.00..2.13 rows=1 width=40)
                         ->  Result  (cost=0.00..1.05 rows=1 width=40)
                               ->  Seq Scan on dmltest
    ```

-   New query plan operator Assert is used for constraints checking.

    This example plan shows the Assert operator.

    ```
    QUERY PLAN
    ------------------------------------------------------------
     Insert  (cost=0.00..4.61 rows=3 width=8)
       ->  Assert  (cost=0.00..3.37 rows=3 width=24)
             Assert Cond: (dmlsource.a > 2) IS DISTINCT FROM 
    false
             ->  Assert  (cost=0.00..2.25 rows=3 width=24)
                   Assert Cond: NOT dmlsource.b IS NULL
                   ->  Result  (cost=0.00..1.14 rows=3 width=24)
                         ->  Seq Scan on dmlsource
    ```


