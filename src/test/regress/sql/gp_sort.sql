-- GPDB used to define Datum as a signed type. That caused below
-- soring issues.
create table mac_tbl (id int, mac macaddr) distributed by (id);
insert into mac_tbl values
(0, '00:00:00:00:00:00'),
(0, 'ff:ff:ff:ff:ff:ff'),
(0, '11:11:11:11:11:11');
select * from mac_tbl order by mac;
create table uuid_tbl (id int, uid uuid) distributed by (id);
insert into uuid_tbl values
(0, '00000000000000000000000000000000'),
(0, 'ffffffffffffffffffffffffffffffff'),
(0, '11111111111111111111111111111111');
select * from uuid_tbl order by uid;
