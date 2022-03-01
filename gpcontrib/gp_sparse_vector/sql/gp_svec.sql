CREATE EXTENSION gp_sparse_vector;
SET search_path TO sparse_vector;

DROP TABLE if exists test;
CREATE TABLE test (a int, b svec) DISTRIBUTED BY (a);

INSERT INTO test (SELECT 1,gp_extract_feature_histogram('{"one","two","three","four","five","six"}','{"twe","four","five","six","one","three","two","one"}'));
INSERT INTO test (SELECT 2,gp_extract_feature_histogram('{"one","two","three","four","five","six"}','{"the","brown","cat","ran","across","three","dogs"}'));
INSERT INTO test (SELECT 3,gp_extract_feature_histogram('{"one","two","three","four","five","six"}','{"two","four","five","six","one","three","two","one"}'));

-- Test the equals operator (should be only 3 rows)
SELECT a,b::float8[] cross_product_equals FROM (SELECT a,b FROM test) foo WHERE b = foo.b ORDER BY a;

DROP TABLE IF EXISTS test2;
CREATE TABLE test2 AS SELECT * FROM test DISTRIBUTED BY (a);
-- Test the plus operator (should be 9 rows)
SELECT (t1.b+t2.b)::float8[] cross_product_sum FROM test t1, test2 t2;

-- Test ORDER BY
SELECT (t1.b+t2.b)::float8[] cross_product_sum, l2norm(t1.b+t2.b) l2norm, (t1.b+t2.b) sparse_vector FROM test t1, test2 t2 ORDER BY 3;

SELECT (sum(t1.b))::float8[] AS features_sum FROM test t1;
-- Test the div operator
SELECT (t1.b/(SELECT sum(b) FROM test))::float8[] AS weights FROM test t1 ORDER BY a;
-- Test the * operator
SELECT t1.b %*% (t1.b/(SELECT sum(b) FROM test)) AS raw_score FROM test t1 ORDER BY a;
-- Test the * and l2norm operators
SELECT (t1.b %*% (t1.b/(SELECT sum(b) FROM test))) / (l2norm(t1.b) * l2norm((SELECT sum(b) FROM test))) AS norm_score FROM test t1 ORDER BY a;
-- Test the ^ and l1norm operators
SELECT ('{1,2}:{20.,10.}'::svec)^('{1}:{3.}'::svec);
SELECT (t1.b %*% (t1.b/(SELECT sum(b) FROM test))) / (l1norm(t1.b) * l1norm((SELECT sum(b) FROM test))) AS norm_score FROM test t1 ORDER BY a;

-- Test the multi-concatenation and show sizes compared with a normal array
DROP TABLE IF EXISTS corpus_proj;
DROP TABLE IF EXISTS corpus_proj_array;
CREATE TABLE corpus_proj AS (SELECT 10000 *|| ('{45,2,35,4,15,1}:{0,1,0,1,0,2}'::svec) result ) distributed randomly;
CREATE TABLE corpus_proj_array AS (SELECT result::float8[] FROM corpus_proj) distributed randomly;
-- Calculate on-disk size of sparse vector
SELECT pg_size_pretty(pg_total_relation_size('corpus_proj'));
-- Calculate on-disk size of normal array
SELECT pg_size_pretty(pg_total_relation_size('corpus_proj_array'));

-- Calculate L1 norm from sparse vector
SELECT l1norm(result) FROM corpus_proj;
-- Calculate L1 norm from float8[]
SELECT l1norm(result) FROM corpus_proj_array;
-- Calculate L2 norm from sparse vector
SELECT l2norm(result) FROM corpus_proj;
-- Calculate L2 norm from float8[]
SELECT l2norm(result) FROM corpus_proj_array;


DROP TABLE corpus_proj;
DROP TABLE corpus_proj_array;
DROP TABLE test;
DROP TABLE test2;

-- Test operators between svec and float8[]
SELECT ('{1,2,3,4}:{3,4,5,6}'::svec)           %*% ('{1,2,3,4}:{3,4,5,6}'::svec)::float8[];
SELECT ('{1,2,3,4}:{3,4,5,6}'::svec)::float8[] %*% ('{1,2,3,4}:{3,4,5,6}'::svec);
SELECT ('{1,2,3,4}:{3,4,5,6}'::svec)            /  ('{1,2,3,4}:{3,4,5,6}'::svec)::float8[];
SELECT ('{1,2,3,4}:{3,4,5,6}'::svec)::float8[]  /  ('{1,2,3,4}:{3,4,5,6}'::svec);
SELECT ('{1,2,3,4}:{3,4,5,6}'::svec)            *  ('{1,2,3,4}:{3,4,5,6}'::svec)::float8[];
SELECT ('{1,2,3,4}:{3,4,5,6}'::svec)::float8[]  *  ('{1,2,3,4}:{3,4,5,6}'::svec);
SELECT ('{1,2,3,4}:{3,4,5,6}'::svec)            +  ('{1,2,3,4}:{3,4,5,6}'::svec)::float8[];
SELECT ('{1,2,3,4}:{3,4,5,6}'::svec)::float8[]  +  ('{1,2,3,4}:{3,4,5,6}'::svec);
SELECT ('{1,2,3,4}:{3,4,5,6}'::svec)            -  ('{1,2,3,4}:{3,4,5,6}'::svec)::float8[];
SELECT ('{1,2,3,4}:{3,4,5,6}'::svec)::float8[]  -  ('{1,2,3,4}:{3,4,5,6}'::svec);

-- Test the pivot operator in the presence of NULL values
DROP TABLE IF EXISTS pivot_test;
CREATE TABLE pivot_test(a float8) distributed randomly;
INSERT INTO pivot_test VALUES (0),(1),(NULL),(2),(3);
SELECT l1norm(array_agg(a)) FROM pivot_test;
DROP TABLE IF EXISTS pivot_test;
-- Answer should be 5
SELECT vec_median(array_agg(a)) FROM (SELECT generate_series(1,9) a) foo;
-- Answer should be a 10-wide vector
SELECT array_agg(a) FROM (SELECT trunc(7) a,generate_series(1,100000) ORDER BY a) foo;
-- Average is 4.50034, median is 5
SELECT vec_median('{9960,9926,10053,9993,10080,10050,9938,9941,10030,10029}:{1,9,8,7,6,5,4,3,2,0}'::svec);
SELECT vec_median('{9960,9926,10053,9993,10080,10050,9938,9941,10030,10029}:{1,9,8,7,6,5,4,3,2,0}'::svec::float8[]);

-- Test operator == as a joining condition when this column could be null. (See issue #12986)
SELECT DISTINCT a.table_name, a.character_maximum_length
FROM information_schema.columns a INNER JOIN information_schema.columns b ON a.table_name=b.table_name AND a.column_name=b.column_name 
WHERE a.character_maximum_length == b.character_maximum_length ORDER BY a.table_name;

DROP EXTENSION gp_sparse_vector;
RESET search_path;
