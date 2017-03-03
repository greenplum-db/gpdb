-- @Description Terminating a Runaway Query in a subtransaction with cursor, aborting transaction cleanly 
-- @author George Caragea
-- @vlimMB 1000 
-- @slimMB 0
-- @redzone 80


-- one level deep subtransaction
BEGIN;
insert into rqt_it_iq values (1001, 1); 

DECLARE cursor_rqt_hold CURSOR WITH HOLD FOR
select * from rqt_it_iq
where c1 > 1000
ORDER BY c1, c2;

DECLARE cursor_rqt_share CURSOR FOR SELECT * FROM rqt_it_iq WHERE c1 > 3 FOR SHARE;
-- start_ignore
FETCH 1 FROM cursor_rqt_share;
-- end_ignore

UPDATE rqt_it_iq SET c2 = -9 WHERE CURRENT OF cursor_rqt_share;

SAVEPOINT sp_1;
insert into rqt_it_iq values (1002, 1); 

-- start_ignore
FETCH 1 FROM cursor_rqt_share;
-- end_ignore

UPDATE rqt_it_iq SET c2 = -10 WHERE CURRENT OF cursor_rqt_share;

FETCH FROM cursor_rqt_hold; 

-- will hit red zone and get terminated
-- content/segment = 0; size = 850MB; sleep = 5 sec; crit_section = false
select gp_allocate_palloc_test_all_segs(0, 850*1024*1024, 5, false);

insert into rqt_it_iq values (1003, 1); 

RELEASE SAVEPOINT sp_1;

insert into rqt_it_iq values (1004, 1); 

COMMIT;

select * from rqt_it_iq where c1 > 1000 or c2 < 0 order by c1, c2; 

-- two level deep nested subtransaction
BEGIN;
insert into rqt_it_iq values (2001, 1); 

DECLARE cursor_rqt_hold CURSOR WITH HOLD FOR
select * from rqt_it_iq
where c1 > 1000
ORDER BY c1, c2;

DECLARE cursor_rqt_share CURSOR FOR SELECT * FROM rqt_it_iq WHERE c1 > 3 FOR SHARE;

-- start_ignore
FETCH 1 FROM cursor_rqt_share;
-- end_ignore

UPDATE rqt_it_iq SET c2 = -9 WHERE CURRENT OF cursor_rqt_share;

SAVEPOINT sp_1;
insert into rqt_it_iq values (2002, 1); 

SAVEPOINT sp_2;

insert into rqt_it_iq values (2003, 1); 

-- start_ignore
FETCH 1 FROM cursor_rqt_share;
-- end_ignore

UPDATE rqt_it_iq SET c2 = -10 WHERE CURRENT OF cursor_rqt_share;

FETCH FROM cursor_rqt_hold; 

-- will hit red zone and get terminated
-- content/segment = 0; size = 850MB; sleep = 5 sec; crit_section = false
select gp_allocate_palloc_test_all_segs(0, 850*1024*1024, 5, false);

insert into rqt_it_iq values (2004, 1); 

RELEASE SAVEPOINT sp_2;

insert into rqt_it_iq values (2005, 1); 

RELEASE SAVEPOINT sp_1; 

insert into rqt_it_iq values (2006, 1); 

COMMIT;

select * from rqt_it_iq where c1 > 2000 or c2 < 0 order by c1, c2; 
