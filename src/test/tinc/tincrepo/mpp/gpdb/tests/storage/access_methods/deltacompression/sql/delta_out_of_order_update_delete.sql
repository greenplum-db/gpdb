-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
-- Out of order update, delete on delta compressed columns
-- start_ignore
Drop table if exists delta_update_t1;
Drop table if exists delta_update_t2;
-- end_ignore

Create table delta_update_t1(
    a1 integer,
    a2 bigint,
    a3 date,
    a4 time,
    a5 timestamp,
    a6 timestamp with time zone
    ) with(appendonly=true, orientation=column, compresstype=rle_type, compresslevel=1);

Insert into delta_update_t1 values
    (1, 2147483648, '2014-07-29', '14:22:23.776890', '2014-07-30 14:22:58.356229', '2014-07-30 14:22:23.776892-07'),
    (1, 2147483648, '2014-07-29', '14:22:23.776890', '2014-07-30 14:22:58.356229', '2014-07-30 14:22:23.776892-07'),
    (2, 2147483630, '2014-07-29', '14:22:23.776899', '2014-07-30 14:22:58.356230', '2014-07-30 14:22:23.776888-07'),
    (2, 2147483650, '2014-07-29', '14:22:23.776899', '2014-07-30 14:22:58.356230', '2014-07-30 14:22:23.776888-07'),
    (1, 2147483648, '2014-07-29', '14:22:23.776890', '2014-07-30 14:22:58.356229', '2014-07-30 14:22:23.776892-07'),
    (10, 2147483660, '2014-07-30', '14:22:23.776892', '2014-07-30 14:22:58.356249', '2014-07-30 14:22:23.776899-07'),
    (10, 2147483660, '2014-07-30', '14:22:23.776892', '2014-07-30 14:22:58.356249', '2014-07-30 14:22:23.776899-07'),
    (11, 2147483677, '2014-07-29', '14:22:23.776894', '2014-07-30 14:22:58.366249', '2014-07-30 14:22:23.776899-07'),
    (11, 2147483677, '2014-07-29', '14:22:23.776894', '2014-07-30 14:22:58.366249', '2014-07-30 14:22:23.777899-07'),
    (10, 2147483660, '2014-07-30', '14:22:23.776892', '2014-07-30 14:22:58.366249', '2014-07-30 14:22:23.777899-07'),
    (13, 2147483660, '2014-07-30', '14:22:23.776892', '2014-07-30 14:22:58.366249', '2014-07-30 14:22:23.777899-07'),
    (12, 2147483660, '2014-07-30', '14:22:23.776892', '2014-07-30 14:22:58.366249', '2014-07-30 14:22:23.777899-07'),
    (13, 2147483660, '2014-07-30', '14:22:23.776892', '2014-07-30 14:22:58.366249', '2014-07-30 14:22:23.777899-07'),
    (10, 2147479999, '2014-07-31', '14:22:23.778899-07', '2014-07-30 14:22:58.357229', '2014-07-30 14:22:23.778899-07'),
    (10, 2147499999, '2024-07-30', '14:22:24.778899', '2014-07-30 10:22:31', '2014-07-30 14:22:24.776892-07'),
    (800012, 2145499999, '2024-07-31', '14:22:25.778899', '2014-07-30 10:26:31', '2014-07-30 14:26:24.776892-07'),
    (800000, 2147499999, '2024-07-30', '14:22:24.778899', '2014-07-30 10:22:31', '2014-07-30 14:22:24.776892-07'),
    (80000000, 2243322399, '990834-07-30', '14:24:23.776899', '2014-07-30 14:26:23.776899', '2014-07-30 14:24:23.776899-07');

Create table delta_update_t2(
    a1 integer,
    a2 bigint,
    a3 date,
    a4 time,
    a5 timestamp,
    a6 timestamp with time zone
    ) with(appendonly=true, orientation=column, compresstype=rle_type, compresslevel=1);

Insert into delta_update_t2 values
    (1, 2147483648, '2014-07-29', '14:22:23.776890', '2014-07-30 14:22:58.356229', '2014-07-30 14:22:23.776892-07'),
    (1, 2147483648, '2014-07-28', '14:22:23.776890', '2014-07-30 14:22:58.356229', '2014-07-30 14:22:23.776893-07'),
    (1, 2147483649, '2014-07-28', '14:22:23.776891', '2014-07-30 14:22:58.356228', '2014-07-30 14:22:23.776893-07'),
    (1, 2147483649, '2014-07-29', '14:22:23.776891', '2014-07-30 14:22:58.356228', '2014-07-30 14:22:23.776893-07'),
    (2, 2147483630, '2014-07-29', '14:22:23.776899', '2014-07-30 14:22:58.356230', '2014-07-30 14:22:23.776888-07'),
    (2, 2147483650, '2014-07-29', '14:22:23.776899', '2014-07-30 14:22:58.356230', '2014-07-30 14:22:23.776888-07'),
    (1, 2147483651, '2014-07-29', '14:22:23.776890', '2014-07-30 14:22:58.356229', '2014-07-30 14:22:23.776892-07'),
    (10, 2147483660, '2014-07-30', '14:22:23.776892', '2014-07-30 14:22:58.356249', '2014-07-30 14:22:23.776899-07'),
    (10, 2147483660, '2014-07-30', '14:22:23.776892', '2014-07-30 14:22:58.356249', '2014-07-30 14:22:23.776899-07'),
    (11, 2147483677, '2014-07-29', '14:22:23.776894', '2014-07-30 14:22:58.366249', '2014-07-30 14:22:23.776899-07'),
    (10, 2147483677, '2014-07-29', '14:22:23.776894', '2014-07-30 14:22:58.366249', '2014-07-30 14:22:23.777899-07'),
    (10, 2147483660, '2014-07-30', '14:22:23.776892', '2014-07-30 14:22:58.366249', '2014-07-30 14:22:23.777898-07'),
    (12, 2147483660, '2014-07-30', '14:22:23.776892', '2014-07-30 14:22:58.366248', '2014-07-30 14:22:23.777898-07'),
    (12, 2147483661, '2014-07-31', '14:22:23.776891', '2014-07-30 14:22:58.366248', '2014-07-30 14:22:23.777899-07'),
    (13, 2147483661, '2014-07-31', '14:22:23.776891', '2014-07-30 14:22:58.366249', '2014-07-30 14:22:23.777899-07');


Select * from delta_update_t1 order by 1,2,3,4,5,6;
Select * from delta_update_t2 order by 1,2,3,4,5,6;

Update delta_update_t1 set a2 = delta_update_t2.a2 from delta_update_t2 where delta_update_t1.a1 = delta_update_t2.a1;
 
Select * from delta_update_t1 order by 1,2,3,4,5,6;
Select * from delta_update_t2 order by 1,2,3,4,5,6;

delete from delta_update_t1 using delta_update_t2 where delta_update_t1.a1 = delta_update_t2.a1;

Select * from delta_update_t1 order by 1,2,3,4,5,6;
Select * from delta_update_t2 order by 1,2,3,4,5,6;


