DROP TABLE IF EXISTS features;
DROP TABLE IF EXISTS corpus;
DROP TABLE IF EXISTS documents;
DROP TABLE IF EXISTS dictionary;

-- Test simple document classIFication routines
CREATE TABLE features (a text[][]) DISTRIBUTED RANDOMLY;
INSERT INTO features values ('{am,before,being,bothered,corpus,document,i,in,is,me,never,now,one,really,second,the,third,this,until}');

DROP TABLE IF EXISTS documents;
CREATE TABLE documents (docnum int, a text[]) DISTRIBUTED RANDOMLY;
INSERT INTO documents values (1,'{this,is,one,document,in,the,corpus}');
INSERT INTO documents values (2,'{i,am,the,second,document,in,the,corpus}');
INSERT INTO documents values (3,'{being,third,never,really,bothered,me,until,now}');
INSERT INTO documents values (4,'{the,document,before,me,is,the,third,document}');

CREATE TABLE corpus (docnum int, a svec) DISTRIBUTED RANDOMLY;
INSERT INTO corpus (SELECT docnum,gp_extract_feature_histogram((SELECT a FROM features LIMIT 1),a) FROM documents);

\qecho Show the feature dictionary
SELECT a dictionary FROM features;

\qecho Show each document
SELECT docnum Document_Number, a document FROM documents ORDER BY 1;

\qecho The extracted feature vector for each document
SELECT docnum Document_Number, a::float8[] feature_vector FROM corpus ORDER BY 1;

\qecho Count the number of times each feature occurs at least once in all documents
SELECT (vec_count_nonzero(a))::float8[] count_in_document FROM corpus;

\qecho Count all occurrences of each term in all documents
SELECT (sum(a))::float8[] sum_in_document FROM corpus;

\qecho Calculate Term Frequency / Inverse Document Frequency
SELECT docnum, (a*logidf)::float8[] tf_idf FROM (SELECT log(count(a)/vec_count_nonzero(a)) logidf FROM corpus) foo, corpus ORDER BY docnum;

\qecho Show the same calculation in compressed vector format
SELECT docnum, (a*logidf) tf_idf FROM (SELECT log(count(a)/vec_count_nonzero(a)) logidf FROM corpus) foo, corpus ORDER BY docnum;

\qecho Create a table with TF / IDF weighted vectors in it
DROP TABLE IF EXISTS WEIGHTS;
CREATE TABLE weights AS (SELECT docnum, (a*logidf) tf_idf FROM (SELECT log(count(a)/vec_count_nonzero(a)) logidf FROM corpus) foo, corpus ORDER BY docnum) DISTRIBUTED RANDOMLY;

\qecho Calculate the angular distance between the first document to each other document
SELECT docnum,trunc((180.*(ACOS(dmin(1.,(tf_idf%*%testdoc)/(l2norm(tf_idf)*l2norm(testdoc))))/(4.*ATAN(1.))))::numeric,2) angular_distance FROM weights,(SELECT tf_idf testdoc FROM weights WHERE docnum = 1 LIMIT 1) foo ORDER BY 1;
