-- Non-superuser
create role mpp6537role createdb login; 
-- Connect database and create table as a new role
set role mpp6537role;
create table mpp6537_partdemo(
        id int,
        color varchar(15),
        time timestamp
) distributed by(
        id
) partition by range( time ) (
        partition p20061231 end (date '2006-12-31') exclusive,
        partition p20090707 end (date '2009-07-07') exclusive
);
set role %USER%;
-- Superuser alter split partition for non-superuser
ALTER TABLE mpp6537_partdemo SPLIT PARTITION p20090707 AT( '20090702' ) INTO( PARTITION p20090702, PARTITION p20090707_tmp );
-- Make sure that the split partition preserve the owner
\dt mpp6537*

drop table mpp6537_partdemo;
drop role mpp6537role;
