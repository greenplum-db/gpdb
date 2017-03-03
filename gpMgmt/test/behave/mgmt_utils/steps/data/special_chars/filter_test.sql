DROP DATABASE IF EXISTS " DB`~@#$%^&*()_-+[{]}|\;: \'/?><;1 ";

CREATE DATABASE " DB`~@#$%^&*()_-+[{]}|\;: \'/?><;1 ";

\c " DB`~@#$%^&*()_-+[{]}|\;: \'/?><;1 ";

DROP TABLE IF EXISTS " co_T`~@#$%^&*()-+[{]}|\;: \'""/?><1 ";

Create table " co_T`~@#$%^&*()-+[{]}|\;: \'""/?><1 " (Column1 int, Column2 varchar(20), Column3 date) 
    WITH(appendonly = true, orientation = column)  
    Distributed Randomly Partition by list(Column2)
    Subpartition by range(Column3) Subpartition Template
        (start (date '2014-01-01') end (date '2016-01-01') every (interval '1 year'))
    (Partition p1 values('backup') , Partition p2 values('restore')) ;

DROP TABLE IF EXISTS " ao_T`~@#$%^&*()-+[{]}|\;: \'""/?><1 ";

Create table " ao_T`~@#$%^&*()-+[{]}|\;: \'""/?><1 " (Column1 int, Column2 varchar(20), Column3 date) 
    WITH(appendonly = true, orientation = row, compresstype = quicklz)  
    Distributed Randomly Partition by list(Column2) 
    Subpartition by range(Column3) Subpartition Template
        (start (date '2014-01-01') end (date '2016-01-01') every (interval '1 year'))
    (Partition p1 values('backup') , Partition p2 values('restore')) ;

DROP TABLE IF EXISTS " heap_T`~@#$%^&*()-+[{]}|\;: \'""/?><1 ";

Create table " heap_T`~@#$%^&*()-+[{]}|\;: \'""/?><1 " (Column1 int, Column2 varchar(20), Column3 date);


INSERT INTO " co_T`~@#$%^&*()-+[{]}|\;: \'""/?><1 " VALUES (2, 'backup', '2015-01-01');
INSERT INTO " co_T`~@#$%^&*()-+[{]}|\;: \'""/?><1 " VALUES (3, 'restore', '2015-01-01');

INSERT INTO " ao_T`~@#$%^&*()-+[{]}|\;: \'""/?><1 " VALUES (1, 'backup', '2014-01-01');
INSERT INTO " ao_T`~@#$%^&*()-+[{]}|\;: \'""/?><1 " VALUES (2, 'backup', '2015-01-01');
INSERT INTO " ao_T`~@#$%^&*()-+[{]}|\;: \'""/?><1 " VALUES (3, 'restore', '2015-01-01');

INSERT INTO " heap_T`~@#$%^&*()-+[{]}|\;: \'""/?><1 " VALUES (101, 'backup-restore', '2016-01-27');
