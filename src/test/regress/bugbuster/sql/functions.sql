create table test_greatest (a integer, b integer);
insert into test_greatest select a, a%25 from generate_series(1,100) a;
select greatest(a,b) from test_greatest;

SELECT round(4, 0);
CREATE or REPLACE FUNCTION sales_tax(subtotal real, OUT tax real) AS $$
                                                BEGIN
                                                    tax := subtotal * 0.06;
                                                END;
                                                $$ LANGUAGE plpgsql NO SQL;
select sales_tax(30);

-- Test DDL executed dynamically in a function
create function bad_ddl() returns void as $body$
        begin
                 execute 'create table junk_table(a int)';
                 execute 'drop table junk_table';
         end;
$body$ language plpgsql volatile MODIFIES SQL DATA;

select bad_ddl();
