-- @Description Terminating a Runaway Query in a subtransaction, aborting transaction cleanly 
-- @author George Caragea
-- @vlimMB 1000 
-- @slimMB 0
-- @redzone 80


-- one level deep subtransaction
BEGIN;
insert into rqt_it_iq values (1001, 1); 

SAVEPOINT sp_1;
insert into rqt_it_iq values (1002, 1); 


-- will hit red zone and get terminated
-- content/segment = 0; size = 850MB; sleep = 5 sec; crit_section = false
select gp_allocate_palloc_test_all_segs(0, 850*1024*1024, 5, false);

insert into rqt_it_iq values (1003, 1); 


RELEASE SAVEPOINT sp_1;

insert into rqt_it_iq values (1004, 1); 

COMMIT;

select * from rqt_it_iq where c1 > 1000 order by c1, c2; 


-- two level deep nested subtransaction
BEGIN;
insert into rqt_it_iq values (2001, 1); 

SAVEPOINT sp_1;
insert into rqt_it_iq values (2002, 1); 

SAVEPOINT sp_2;

insert into rqt_it_iq values (2003, 1); 

-- will hit red zone and get terminated
-- content/segment = 0; size = 850MB; sleep = 5 sec; crit_section = false
select gp_allocate_palloc_test_all_segs(0, 850*1024*1024, 5, false);

insert into rqt_it_iq values (2004, 1); 


RELEASE SAVEPOINT sp_2;

insert into rqt_it_iq values (2005, 1); 

RELEASE SAVEPOINT sp_1; 

insert into rqt_it_iq values (2006, 1); 

COMMIT;

select * from rqt_it_iq where c1 > 2000 order by c1, c2; 
