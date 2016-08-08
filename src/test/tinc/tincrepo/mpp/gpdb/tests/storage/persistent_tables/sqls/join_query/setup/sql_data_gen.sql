INSERT INTO T1 VALUES(generate_series(1,50), 'pt_test', '2013-04-023', 'persistent tables testing with Join query for OOM exception', '2002-4-23 03:51:15+1359', generate_series(50,100), 'persistent tables' , 2013, 'Join query testing for Persistent Tables', '2013-03-25');

INSERT INTO T2 VALUES(generate_series(1,50), 'pt_test', '2013-04-023', 'persistent tables testing with Join query for OOM exception', '2002-4-23 03:51:15+1359', generate_series(50,100), 'persistent tables' , 2013, 'Join query testing for Persistent Tables', '2013-03-25');

INSERT INTO T3 VALUES(generate_series(1,50), 'pt_test', '2013-04-023', 'persistent tables testing with Join query for OOM exception', '2002-4-23 03:51:15+1359', generate_series(50,100), 'persistent tables' , 2013, 'Join query testing for Persistent Tables', '2013-03-25');

INSERT INTO T4 VALUES(generate_series(1,50), 'pt_test', '2013-04-023', 'persistent tables testing with Join query for OOM exception', '2002-4-23 03:51:15+1359', generate_series(50,100), 'persistent tables' , 2013, 'Join query testing for Persistent Tables', '2013-03-25');

INSERT INTO T5 VALUES(generate_series(1,50), 'pt_test', '2013-04-023', 'persistent tables testing with Join query for OOM exception', '2002-4-23 03:51:15+1359', generate_series(50,100), 'persistent tables' , 2013, 'Join query testing for Persistent Tables', '2013-03-25');

INSERT INTO T6 VALUES(generate_series(1,50), 'pt_test', '2013-04-023', 'persistent tables testing with Join query for OOM exception', '2002-4-23 03:51:15+1359', generate_series(50,100), 'persistent tables' , 2013, 'Join query testing for Persistent Tables', '2013-03-25');

INSERT INTO T7 VALUES(generate_series(1,50), 'pt_test', '2013-04-023', 'persistent tables testing with Join query for OOM exception', '2002-4-23 03:51:15+1359', generate_series(50,100), 'persistent tables' , 2013, 'Join query testing for Persistent Tables', '2013-03-25');

INSERT INTO T8 VALUES(generate_series(1,50), 'pt_test', '2013-04-023', 'persistent tables testing with Join query for OOM exception', '2002-4-23 03:51:15+1359', generate_series(50,100), 'persistent tables' , 2013, 'Join query testing for Persistent Tables', '2013-03-25');

INSERT INTO T9 VALUES(generate_series(1,50), 'pt_test', '2013-04-023', 'persistent tables testing with Join query for OOM exception', '2002-4-23 03:51:15+1359', generate_series(50,100), 'persistent tables' , 2013, 'Join query testing for Persistent Tables', '2013-03-25');

INSERT INTO T10 VALUES(generate_series(1,50), 'pt_test', '2013-04-023', 'persistent tables testing with Join query for OOM exception', '2002-4-23 03:51:15+1359', generate_series(50,100), 'persistent tables' , 2013, 'Join query testing for Persistent Tables', '2013-03-25');

select pg_size_pretty(pg_relation_size('T1'));
