drop table if exists skewed_lineitem;

create table skewed_lineitem as
select 1 AS l_skewkey, *
from lineitem
distributed by (l_skewkey);

insert into skewed_lineitem
values(
    2,
    generate_series(1, 3), 42, 15, 23,
    0, 4000, 0.1, 0.3,
    NULL, NULL,
    '2001-01-01', '2012-12-01', NULL,
    'foobarfoobarbaz', '0937',
    'supercalifragilisticexpialidocious');
