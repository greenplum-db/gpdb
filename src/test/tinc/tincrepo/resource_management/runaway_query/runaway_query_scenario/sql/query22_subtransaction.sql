-- @author balasr3
-- @description TPC-H query22
-- @created 2012-07-26 22:04:56
-- @modified 2012-07-26 22:04:56
-- @tags orca

BEGIN;
INSERT INTO region (r_name, r_comment) values ('QUERY EXECUTION', 'SAVEPOINT_NAME');
SAVEPOINT sp_SAVEPOINT_NAME;
INSERT INTO region (r_name, r_comment) values ('QUERY EXECUTION', 'inner_SAVEPOINT_NAME');

select 
                cntrycode, 
                count(*) as numcust, 
                sum(c_acctbal) as totacctbal 
        from 
                ( 
                        select 
                                substring(c_phone from 1 for 2) as cntrycode, 
                                c_acctbal 
                        from 
                                customer 
                        where 
                                substring(c_phone from 1 for 2) in 
                                        ('11', '24', '25', '19', '30', '26', '16')
                                and c_acctbal > ( 
                                        select 
                                                avg(c_acctbal) 
                                        from 
                                                customer 
                                        where 
                                                c_acctbal > 0.00 
                                                and substring(c_phone from 1 for 2) in 
                                                        ('11', '24', '25', '19', '30', '26', '16')
                                ) 
                                and not exists (
					select
						*
					from
						orders
					where
						o_custkey = c_custkey
                                )
                ) as custsale 
        group by 
                cntrycode 
        order by 
                cntrycode;

RELEASE SAVEPOINT sp_SAVEPOINT_NAME;
COMMIT;
