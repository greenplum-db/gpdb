-- Negative: using SCATTER BY outside of sub-query of ETF call.
-- The followings should fail
    SELECT * FROM transform( TABLE(select * from intable) scatter randomly );

    SELECT * FROM transform( TABLE(select * from intable) ) scatter randomly;

    SELECT * FROM transform( TABLE(select * from intable) scatter by a );

    SELECT * FROM transform( TABLE(select * from intable) ) scatter by a, b;

    SELECT * FROM transform( TABLE(select * from intable) distributed randomly );

    SELECT * FROM transform( TABLE(select * from intable) ) distributed randomly;

    SELECT * FROM transform( TABLE(select * from intable) distributed by (a) );

    SELECT * FROM transform( TABLE(select * from intable) ) distributed by (a,b);

    SELECT id,value FROM intable scatter by id;

    SELECT id,value FROM intable scatter by (id);

    SELECT id,value FROM intable scatter randomly;

    SELECT id,value FROM intable scatter by id,value;

    SELECT id,value FROM intable order by id scatter by id;

    SELECT id,value FROM intable where id < 5 and scatter by id;

    SELECT avg(a) FROM t1 scatter by c;

    SELECT avg(a) FROM t1 group by c scatter by c;

    SELECT avg(a) FROM t1 group by c having avg(a)>50 scatter by c;

    SELECT a, c, avg(a) over(scatter by c scatter by c) FROM t1 where a <10;

    SELECT a, c, avg(a) over(partition by c scatter by c) FROM t1 where a <10;
