create table mpp10847_pkeyconstraints(pkid serial, option1 int, option2 int, option3 int, primary key(pkid, option3))                           
distributed by (pkid) partition by range (option3)                                                                             
(                                                                                                                              
partition aa start(1) end(100) inclusive,                                                                                                
partition bb start(101) end(200) inclusive, 
partition cc start(201) end (300) inclusive                                                           
);

insert into mpp10847_pkeyconstraints values (10000, 50, 50, 102);
-- this is suppose to fail as you're not suppose to be able to use the same primary key in the same table
insert into mpp10847_pkeyconstraints values (10000, 50, 50, 5);

select * from mpp10847_pkeyconstraints order by pkid, option3;

drop table mpp10847_pkeyconstraints;
