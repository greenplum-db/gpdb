-- start_ignore
drop schema gpdb_language cascade;
create schema gpdb_language;
set search_path=gpdb_language,"$user",public;
-- end_ignore

CREATE TABLE example3(id int, class text, A1 int, A2 int, A3 int)
DISTRIBUTED BY (id);

insert into example3 values(1, 'C1', 1, 2, 3);
insert into example3 values(2, 'C1', 1, 2, 1);
insert into example3 values(3, 'C1', 1, 4, 3);
insert into example3 values(4, 'C2', 1, 2, 2);
insert into example3 values(5, 'C2', 0, 2, 2);
insert into example3 values(6, 'C2', 0, 1, 3);

select * from example3
ORDER BY 1,2,3,4,5;

CREATE OR REPLACE VIEW example3_unpivot AS
SELECT id, class,
unnest(array['A1','A2','A3']) as attr,
unnest(array[a1,a2,a3]) as value
FROM example3;

SELECT * FROM example3_unpivot
ORDER BY 1,2,3,4;

-- Next we have to create a training table:

-- start_ignore
DROP table example3_nb_training CASCADE;
-- end_ignore

CREATE table example3_nb_training AS
SELECT attr, value, pivot_sum(array['C1', 'C2'], class, 1) as class_count
FROM example3_unpivot
GROUP BY attr, value
DISTRIBUTED by (attr);

SELECT * FROM example3_nb_training
ORDER BY 1,2,3;

-- And a summary view over the training.

CREATE OR REPLACE VIEW example3_nb_classify AS
SELECT attr, value, class_count, array['C1', 'C2'] as classes,
sum(class_count) over (wa)::integer[] as class_total,
count(distinct value) over (wa) as attr_count
FROM example3_nb_training
WINDOW wa as (partition by attr);

-- classify a new row (A1 = 0, A2 = 2, A3 = 1)
select nb_classify(classes, attr_count, class_count, class_total) as class
from example3_nb_classify
where (attr = 'A1' and value = 0) or (attr = 'A2' and value = 2) or (attr = 'A3' and value = 1);
