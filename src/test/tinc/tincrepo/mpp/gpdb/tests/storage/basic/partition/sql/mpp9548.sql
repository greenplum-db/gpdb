--start_ignore
drop table if exists mpp9548;
--end_ignore

create table mpp9548 
	(
		a int not null,
		drop1a float not null,
		b date not null,
		c time without time zone,
		drop2a text,
		d varchar,
		e varchar,
		drop1b int,
		f varchar,
		g varchar,
		drop2b int,
		h varchar,
		i varchar,
		j varchar,
		k varchar
	)
    partition by range(b) 
    (
        start (date '2010-06-01')
        end (date '2010-06-02')
        every (interval '1 day')
    );

alter table mpp9548 drop column drop1a, drop column drop1b;
alter table mpp9548 add partition drop1 
    start('2010-06-02') 
    end('2010-06-03');

alter table mpp9548 drop column drop2a, drop column drop2b;
alter table mpp9548 add partition drop2 
    start('2010-06-03') 
    end('2010-06-04');

insert into mpp9548 
values
    (1, '2010-06-01', 
     '12:00', 
     'd', 'e', 'f','g', 'h', 'i', 'j', 'k'), 
    (2, '2010-06-02', 
     '12:00', 
     'd', 'e', 'f','g', 'h', 'i', 'j', 'k'), 
    (3, '2010-06-03', 
     '12:00', 
     'd', 'e', 'f','g', 'h', 'i', 'j', 'k');

select * from mpp9548;

-- Good output looks like this:

--  a |     b      |    c     | d | e | f | g | h | i | j | k 
-- ---+------------+----------+---+---+---+---+---+---+---+---
--  1 | 2010-06-01 | 12:00:00 | d | e | f | g | h | i | j | k
--  3 | 2010-06-03 | 12:00:00 | d | e | f | g | h | i | j | k
--  2 | 2010-06-02 | 12:00:00 | d | e | f | g | h | i | j | k
-- (3 rows)
