\c gptest;

-- Alter table rename default partition
Alter table sto_althp1 rename default partition to new_others;
Insert into sto_althp1 values(1,10,3,4);
select * from sto_althp1 order by b,c;

-- Alter table rename partition
Alter table sto_althp3 RENAME PARTITION FOR ('2008-01-01') TO jan08;
select count(*) from sto_althp3;

-- Alter table drop default partition
Alter table sto_althp1 drop default partition;
select * from sto_althp1 order by b,c;

-- Alter table drop partition
Alter table sto_althp1 drop partition for (rank(1));
select * from sto_althp1 order by b,c;

-- Alter table add heap partition
Alter table sto_althp1 add partition new_p start(6) end(8);
Insert into sto_althp1 values(1,7,3,4);
select * from sto_althp1 order by b,c;

-- Alter table add ao partition
Alter table sto_althp1 add partition p1 start(9) end(13) with(appendonly=true);
Insert into sto_althp1 values(1,10,3,4);
select * from sto_althp1 order by b,c;

-- Alter table add co partition
Alter table sto_althp1 add partition p2 start(14) end(17) with(appendonly=true, orientation=column);
Insert into sto_althp1 values(1,15,3,4);
select * from sto_althp1 order by b,c ;

-- Alter table add default partition
Alter table sto_althp1 add default partition part_others;
Insert into sto_althp1 values(1,25,3,4);
select * from sto_althp1 order by b,c;

-- Alter table split subpartition
Alter table  sto_althp1 alter partition p1 split partition FOR (RANK(1)) at(2) into (partition splita,partition splitb);
select * from sto_althp1 order by b,c;

select count(*) from sto_althp2;

-- Alter table split partition
Alter table sto_althp2 split partition p1 at(3) into (partition splitc,partition splitd) ;
select * from sto_althp2 order by a;

-- Alter table split default partition
Alter table sto_althp2 split default partition start(15) end(17) into (partition p1, partition others);
select * from sto_althp2 order by a;
