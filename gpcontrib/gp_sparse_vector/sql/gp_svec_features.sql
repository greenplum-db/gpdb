CREATE EXTENSION gp_sparse_vector;

SET search_path TO sparse_vector;

DROP TABLE IF EXISTS features;
DROP TABLE IF EXISTS corpus;
DROP TABLE IF EXISTS documents;
DROP TABLE IF EXISTS dictionary;

-- Test simple document classIFication routines
CREATE TABLE features (dictionary text[][]) DISTRIBUTED RANDOMLY;
INSERT INTO features values ('{am,before,being,bothered,corpus,document,i,in,is,me,never,now,one,really,second,the,third,this,until}');
CREATE TABLE documents (docnum int, document text[]) DISTRIBUTED RANDOMLY;
INSERT INTO documents values (1,'{this,is,one,document,in,the,corpus}');
INSERT INTO documents values (2,'{i,am,the,second,document,in,the,corpus}');
INSERT INTO documents values (3,'{being,third,never,really,bothered,me,until,now}');
INSERT INTO documents values (4,'{the,document,before,me,is,the,third,document}');
CREATE TABLE corpus (docnum int, feature_vector svec) DISTRIBUTED RANDOMLY;
INSERT INTO corpus (SELECT docnum,gp_extract_feature_histogram((SELECT dictionary FROM features LIMIT 1),document) FROM documents);
-- Show the feature dictionary
SELECT dictionary FROM features;

-- Show each document
SELECT docnum AS Document_Number, document FROM documents ORDER BY 1;

-- The extracted feature vector for each document
SELECT docnum AS Document_Number, feature_vector::float8[] FROM corpus ORDER BY 1;

-- Count the number of times each feature occurs at least once in all documents
SELECT (vec_count_nonzero(feature_vector))::float8[] AS count_in_document FROM corpus;

-- Count all occurrences of each term in all documents
SELECT (sum(feature_vector))::float8[] AS sum_in_document FROM corpus;

-- Calculate Term Frequency / Inverse Document Frequency
SELECT docnum, (feature_vector*logidf)::float8[] AS tf_idf FROM (SELECT log(count(feature_vector)/vec_count_nonzero(feature_vector)) AS logidf FROM corpus) AS foo, corpus ORDER BY docnum;

-- Show the same calculation in compressed vector format
SELECT docnum, (feature_vector*logidf) AS tf_idf FROM (SELECT log(count(feature_vector)/vec_count_nonzero(feature_vector)) AS logidf FROM corpus) foo, corpus ORDER BY docnum;

-- Create a table with TF / IDF weighted vectors in it
DROP TABLE IF EXISTS WEIGHTS;
CREATE TABLE weights AS (SELECT docnum, (feature_vector*logidf) tf_idf FROM (SELECT log(count(feature_vector)/vec_count_nonzero(feature_vector)) AS logidf FROM corpus) foo, corpus ORDER BY docnum) DISTRIBUTED RANDOMLY;

-- Calculate the angular distance between the first document to each other document
SELECT docnum,trunc((180.*(ACOS(dmin(1.,(tf_idf%*%testdoc)/(l2norm(tf_idf)*l2norm(testdoc))))/(4.*ATAN(1.))))::numeric,2) AS angular_distance FROM weights,(SELECT tf_idf testdoc FROM weights WHERE docnum = 1 LIMIT 1) foo ORDER BY 1;

DROP TABLE features;
DROP TABLE corpus;
DROP TABLE documents;
DROP TABLE WEIGHTS;
DROP EXTENSION gp_sparse_vector;
