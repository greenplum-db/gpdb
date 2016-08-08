-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
CREATE TABLE heap_hybrid_part (
                PS_PARTKEY INTEGER,
                PS_SUPPKEY INTEGER,
                PS_AVAILQTY integer,
                PS_SUPPLYCOST decimal,
                PS_COMMENT VARCHAR(199)
                )   
 partition by range (ps_supplycost) 
 subpartition by range (ps_suppkey)
,subpartition by range (ps_partkey) subpartition template (start('1') end('200001') every(66666)

)
(
partition p1 start('1') end('20') inclusive
(
 subpartition sp1 start('1') end('3030')  ,
 subpartition sp2 end('6096') inclusive ,
 subpartition sp3   start('6096') exclusive end('7201') ,
 subpartition sp4 end('10001')  
), 
partition p2 end('1001') inclusive
(
subpartition sp1 start('1') ,    
subpartition sp2 start('4139')  ,
subpartition sp3 start('4685')   ,   
subpartition sp4 start('5675') )
);

create index heap_hybrid_part_btree_index on heap_hybrid_part (PS_PARTKEY);

BEGIN;
     
         INSERT INTO heap_hybrid_part  VALUES( generate_series(1,500), generate_series(500,1000),generate_series(600,1100),591.18,substring( 'final accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously afterfinal accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously after' for (random()*150)));
    SAVEPOINT save1;

         INSERT INTO heap_hybrid_part  VALUES( generate_series(1,500), generate_series(500,1000),generate_series(600,1100),591.18,substring( 'final accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously afterfinal accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously after' for (random()*150)));
    SAVEPOINT save2;

         INSERT INTO heap_hybrid_part  VALUES( generate_series(1,500), generate_series(500,1000),generate_series(600,1100),591.18,substring( 'final accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously afterfinal accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously after' for (random()*150)));
    SAVEPOINT save3;

         INSERT INTO heap_hybrid_part  VALUES( generate_series(1,500), generate_series(500,1000),generate_series(600,1100),591.18,substring( 'final accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously afterfinal accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously after' for (random()*150)));
    SAVEPOINT save4;

         INSERT INTO heap_hybrid_part  VALUES( generate_series(1,500), generate_series(500,1000),generate_series(600,1100),591.18,substring( 'final accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously afterfinal accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously after' for (random()*150)));
    ROLLBACK TO SAVEPOINT save3;

         INSERT INTO heap_hybrid_part  VALUES( generate_series(1,500), generate_series(500,1000),generate_series(600,1100),591.18,substring( 'final accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously afterfinal accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously after' for (random()*150)));
    SAVEPOINT save5;
         INSERT INTO heap_hybrid_part  VALUES( generate_series(1,500), generate_series(500,1000),generate_series(600,1100),591.18,substring( 'final accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously afterfinal accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously after' for (random()*150)));
    SAVEPOINT save6;

         INSERT INTO heap_hybrid_part  VALUES( generate_series(1,500), generate_series(500,1000),generate_series(600,1100),591.18,substring( 'final accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously afterfinal accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously after' for (random()*150)));
    SAVEPOINT save7;
         INSERT INTO heap_hybrid_part  VALUES( generate_series(1,500), generate_series(500,1000),generate_series(600,1100),591.18,substring( 'final accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously afterfinal accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously after' for (random()*150)));
    SAVEPOINT save8;
         INSERT INTO heap_hybrid_part  VALUES( generate_series(1,500), generate_series(500,1000),generate_series(600,1100),591.18,substring( 'final accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously afterfinal accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously after' for (random()*150)));
    ROLLBACK TO SAVEPOINT save7;

         INSERT INTO heap_hybrid_part  VALUES( generate_series(1,500), generate_series(500,1000),generate_series(600,1100),591.18,substring( 'final accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously afterfinal accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously after' for (random()*150)));
    SAVEPOINT save9;
         INSERT INTO heap_hybrid_part  VALUES( generate_series(1,500), generate_series(500,1000),generate_series(600,1100),591.18,substring( 'final accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously afterfinal accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously after' for (random()*150)));
    SAVEPOINT save10;

         INSERT INTO heap_hybrid_part  VALUES( generate_series(1,500), generate_series(500,1000),generate_series(600,1100),591.18,substring( 'final accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously afterfinal accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously after' for (random()*150)));
    SAVEPOINT save11;
         INSERT INTO heap_hybrid_part  VALUES( generate_series(1,500), generate_series(500,1000),generate_series(600,1100),591.18,substring( 'final accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously afterfinal accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously after' for (random()*150)));
    SAVEPOINT save12;
         INSERT INTO heap_hybrid_part  VALUES( generate_series(1,500), generate_series(500,1000),generate_series(600,1100),591.18,substring( 'final accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously afterfinal accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously after' for (random()*150)));
    ROLLBACK TO SAVEPOINT save11;

         INSERT INTO heap_hybrid_part  VALUES( generate_series(1,500), generate_series(500,1000),generate_series(600,1100),591.18,substring( 'final accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously afterfinal accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously after' for (random()*150)));
    SAVEPOINT save13;

        INSERT INTO heap_hybrid_part  VALUES( generate_series(1,500), generate_series(500,1000),generate_series(600,1100),591.18,substring( 'final accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously afterfinal accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously after' for (random()*150)));
    SAVEPOINT save14;

         INSERT INTO heap_hybrid_part  VALUES( generate_series(1,500), generate_series(500,1000),generate_series(600,1100),591.18,substring( 'final accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously afterfinal accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously after' for (random()*150)));
    SAVEPOINT save15;
         INSERT INTO heap_hybrid_part  VALUES( generate_series(1,500), generate_series(500,1000),generate_series(600,1100),591.18,substring( 'final accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously afterfinal accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously after' for (random()*150)));
    SAVEPOINT save16;
         INSERT INTO heap_hybrid_part  VALUES( generate_series(1,500), generate_series(500,1000),generate_series(600,1100),591.18,substring( 'final accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously afterfinal accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously after' for (random()*150)));
    ROLLBACK TO SAVEPOINT save14;

         INSERT INTO heap_hybrid_part  VALUES( generate_series(1,500), generate_series(500,1000),generate_series(600,1100),591.18,substring( 'final accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously afterfinal accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously after' for (random()*150)));
    SAVEPOINT save17;
         INSERT INTO heap_hybrid_part  VALUES( generate_series(1,500), generate_series(500,1000),generate_series(600,1100),591.18,substring( 'final accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously afterfinal accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously after' for (random()*150)));
    SAVEPOINT save18;

         INSERT INTO heap_hybrid_part  VALUES( generate_series(1,500), generate_series(500,1000),generate_series(600,1100),591.18,substring( 'final accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously afterfinal accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously after' for (random()*150)));
    SAVEPOINT save19;
         INSERT INTO heap_hybrid_part  VALUES( generate_series(1,500), generate_series(500,1000),generate_series(600,1100),591.18,substring( 'final accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously afterfinal accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously after' for (random()*150)));
    SAVEPOINT save20;
         INSERT INTO heap_hybrid_part  VALUES( generate_series(1,500), generate_series(500,1000),generate_series(600,1100),591.18,substring( 'final accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously afterfinal accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously after' for (random()*150)));
    ROLLBACK TO SAVEPOINT save18;

         INSERT INTO heap_hybrid_part  VALUES( generate_series(1,500), generate_series(500,1000),generate_series(600,1100),591.18,substring( 'final accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously afterfinal accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously after' for (random()*150)));
    SAVEPOINT save21;
         INSERT INTO heap_hybrid_part  VALUES( generate_series(1,500), generate_series(500,1000),generate_series(600,1100),591.18,substring( 'final accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously afterfinal accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously after' for (random()*150)));
    SAVEPOINT save22;

         INSERT INTO heap_hybrid_part  VALUES( generate_series(1,500), generate_series(500,1000),generate_series(600,1100),591.18,substring( 'final accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously afterfinal accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously after' for (random()*150)));
    SAVEPOINT save23;
         INSERT INTO heap_hybrid_part  VALUES( generate_series(1,500), generate_series(500,1000),generate_series(600,1100),591.18,substring( 'final accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously afterfinal accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously after' for (random()*150)));
    SAVEPOINT save24;
         INSERT INTO heap_hybrid_part  VALUES( generate_series(1,500), generate_series(500,1000),generate_series(600,1100),591.18,substring( 'final accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously afterfinal accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously after' for (random()*150)));
    ROLLBACK TO SAVEPOINT save23;

         INSERT INTO heap_hybrid_part  VALUES( generate_series(1,500), generate_series(500,1000),generate_series(600,1100),591.18,substring( 'final accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously afterfinal accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously after' for (random()*150)));
    SAVEPOINT save25;
         INSERT INTO heap_hybrid_part  VALUES( generate_series(1,500), generate_series(500,1000),generate_series(600,1100),591.18,substring( 'final accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously afterfinal accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously after' for (random()*150)));
    SAVEPOINT save26;

         INSERT INTO heap_hybrid_part  VALUES( generate_series(1,500), generate_series(500,1000),generate_series(600,1100),591.18,substring( 'final accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously afterfinal accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously after' for (random()*150)));
    SAVEPOINT save27;
         INSERT INTO heap_hybrid_part  VALUES( generate_series(1,500), generate_series(500,1000),generate_series(600,1100),591.18,substring( 'final accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously afterfinal accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously after' for (random()*150)));
    SAVEPOINT save28;
         INSERT INTO heap_hybrid_part  VALUES( generate_series(1,500), generate_series(500,1000),generate_series(600,1100),591.18,substring( 'final accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously afterfinal accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously after' for (random()*150)));
    ROLLBACK TO SAVEPOINT save27;

         INSERT INTO heap_hybrid_part  VALUES( generate_series(1,500), generate_series(500,1000),generate_series(600,1100),591.18,substring( 'final accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously afterfinal accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously after' for (random()*150)));
    SAVEPOINT save29;
         INSERT INTO heap_hybrid_part  VALUES( generate_series(1,500), generate_series(500,1000),generate_series(600,1100),591.18,substring( 'final accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously afterfinal accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously after' for (random()*150)));
    SAVEPOINT save30;

         INSERT INTO heap_hybrid_part  VALUES( generate_series(1,500), generate_series(500,1000),generate_series(600,1100),591.18,substring( 'final accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously afterfinal accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously after' for (random()*150)));
    SAVEPOINT save31;
         INSERT INTO heap_hybrid_part  VALUES( generate_series(1,500), generate_series(500,1000),generate_series(600,1100),591.18,substring( 'final accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously afterfinal accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously after' for (random()*150)));
    SAVEPOINT save32;
         INSERT INTO heap_hybrid_part  VALUES( generate_series(1,500), generate_series(500,1000),generate_series(600,1100),591.18,substring( 'final accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously afterfinal accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously after' for (random()*150)));
    ROLLBACK TO SAVEPOINT save30;

         INSERT INTO heap_hybrid_part  VALUES( generate_series(1,500), generate_series(500,1000),generate_series(600,1100),591.18,substring( 'final accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously afterfinal accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously after' for (random()*150)));
    SAVEPOINT save31;
         INSERT INTO heap_hybrid_part  VALUES( generate_series(1,500), generate_series(500,1000),generate_series(600,1100),591.18,substring( 'final accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously afterfinal accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously after' for (random()*150)));
    SAVEPOINT save32;

COMMIT;
