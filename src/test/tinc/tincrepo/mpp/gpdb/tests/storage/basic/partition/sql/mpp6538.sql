-- Non-superuser
create role mpp6538role createdb login; 
-- Connect database and create table as a new role
set role mpp6538role;
create table mpp6538_partdemo(
        id int,
        color varchar(15),
        time timestamp
) distributed by(
        id
) partition by range( time ) (
        partition p20061231 end (date '2006-12-31') exclusive,
        partition p20090707 end (date '2009-07-07') exclusive
);

-- User alter split partition
ALTER TABLE mpp6538_partdemo SPLIT PARTITION p20090707 AT( '20090702' ) INTO( PARTITION p20090702, PARTITION p20090707_tmp );

set role %USER%;
-- Make sure that the split partition preserve the owner
\dt mpp6538*

drop table mpp6538_partdemo;
drop role mpp6538role;
