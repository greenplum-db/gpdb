\c " DB`~@#$%^&*()_-+[{]}|\;: \'/?><;1 ";

SET SEARCH_PATH=" S`~@#$%^&*()-+[{]}|\;: \'""/?><1 ";

Create table " co_T`~@#$%^&*()-+[{]}|\;: \'""/?><1 " (Column1 int, Column2 varchar(20), Column3 date) 
    WITH(appendonly = true, orientation = column)  
    Distributed Randomly Partition by list(Column2)
    Subpartition by range(Column3) Subpartition Template
        (start (date '2014-01-01') end (date '2016-01-01') every (interval '1 year'))
    (Partition p1 values('backup') , Partition p2 values('restore')) ;

Create table " ao_T`~@#$%^&*()-+[{]}|\;: \'""/?><1 " (Column1 int, Column2 varchar(20), Column3 date) 
    WITH(appendonly = true, orientation = row)  
    Distributed Randomly Partition by list(Column2) 
    Subpartition by range(Column3) Subpartition Template
        (start (date '2014-01-01') end (date '2016-01-01') every (interval '1 year'))
    (Partition p1 values('backup') , Partition p2 values('restore')) ;

Create table " heap_T`~@#$%^&*()-+[{]}|\;: \'""/?><1 " (Column1 int, Column2 varchar(20), Column3 date);
