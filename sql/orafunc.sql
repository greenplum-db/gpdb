\set ECHO none
\i orafunc.sql
SET DATESTYLE TO ISO;
\set ECHO all


--
-- test built-in date type oracle compatibility functions
--

SELECT add_months ('2003-08-01', 3);
SELECT add_months ('2003-08-01', -3);
SELECT add_months ('2003-08-21', -3);
SELECT add_months ('2003-01-31', 1);

SELECT last_day (to_date('2003/03/15', 'yyyy/mm/dd'));
SELECT last_day (to_date('2003/02/03', 'yyyy/mm/dd'));
SELECT last_day (to_date('2004/02/03', 'yyyy/mm/dd'));

SELECT next_day ('2003-08-01', 'TUESDAY');
SELECT next_day ('2003-08-06', 'WEDNESDAY');
SELECT next_day ('2003-08-06', 'SUNDAY');

SELECT months_between (to_date ('2003/01/01', 'yyyy/mm/dd'), to_date ('2003/03/14', 'yyyy/mm/dd'));
SELECT months_between (to_date ('2003/07/01', 'yyyy/mm/dd'), to_date ('2003/03/14', 'yyyy/mm/dd'));
SELECT months_between (to_date ('2003/07/02', 'yyyy/mm/dd'), to_date ('2003/07/02', 'yyyy/mm/dd'));
SELECT months_between (to_date ('2003/08/02', 'yyyy/mm/dd'), to_date ('2003/06/02', 'yyyy/mm/dd'));

select length('jmenuji se Pavel Stehule'),dbms_pipe.pack_message('jmenuji se Pavel Stehule');
select length('a bydlim ve Skalici'),dbms_pipe.pack_message('a bydlim ve Skalici');
select dbms_pipe.send_message('pavel',0,1);
select dbms_pipe.send_message('pavel',0,2);
select dbms_pipe.receive_message('pavel',0);
select '>>>>'||dbms_pipe.unpack_message_text()||'<<<<';
select '>>>>'||dbms_pipe.unpack_message_text()||'<<<<';
select dbms_pipe.receive_message('pavel',0);

select dbms_pipe.purge('bob');
select dbms_pipe.reset_buffer();

select dbms_pipe.pack_message('012345678901234+1');
select dbms_pipe.send_message('bob',0,10);
select dbms_pipe.pack_message('012345678901234+2');
select dbms_pipe.send_message('bob',0,10);
select dbms_pipe.pack_message('012345678901234+3');
select dbms_pipe.send_message('bob',0,10);
--------------------------------------------
select dbms_pipe.receive_message('bob',0);
select dbms_pipe.unpack_message_text();
select dbms_pipe.receive_message('bob',0);
select dbms_pipe.unpack_message_text();
select dbms_pipe.receive_message('bob',0);
select dbms_pipe.unpack_message_text();

select dbms_pipe.unique_session_name() LIKE 'PG$PIPE$%';
select dbms_pipe.pack_message('012345678901234-1');
select dbms_pipe.send_message('bob',0,10);
select dbms_pipe.receive_message('bob',0);
select dbms_pipe.unpack_message_text();
select dbms_pipe.pack_message('012345678901234-2');
select dbms_pipe.send_message('bob',0,10);
select dbms_pipe.send_message('bob',0,10);
select dbms_pipe.receive_message('bob',0);
select dbms_pipe.unpack_message_text();

select dbms_pipe.pack_message(TO_DATE('2006-10-11', 'YYYY-MM-DD'));
select dbms_pipe.send_message('test_date');
select dbms_pipe.receive_message('test_date');
select dbms_pipe.next_item_type();
select dbms_pipe.unpack_message_date();

select dbms_pipe.pack_message(current_timestamp);
select dbms_pipe.send_message('test_date');
select dbms_pipe.receive_message('test_date');
select dbms_pipe.next_item_type();
select to_char(dbms_pipe.unpack_message_timestamp(),'YYYY-MM-DD::hh24:ss:mm') = to_char(current_timestamp,'YYYY-MM-DD::hh24:ss:mm');

select dbms_pipe.pack_message(6262626262::numeric);
select dbms_pipe.send_message('test_int');
select dbms_pipe.receive_message('test_int');
select dbms_pipe.next_item_type();
select dbms_pipe.unpack_message_number();

select PLVstr.betwn('Harry and Sally are very happy', 7, 9);
select PLVstr.betwn('Harry and Sally are very happy', 7, 9, FALSE);
select PLVstr.betwn('Harry and Sally are very happy', -3, -1);
select PLVstr.betwn('Harry and Sally are very happy', 'a', 'ry');
select PLVstr.betwn('Harry and Sally are very happy', 'a', 'ry', 1,1,FALSE,FALSE);
select PLVstr.betwn('Harry and Sally are very happy', 'a', 'ry', 2,1,TRUE,FALSE);
select PLVstr.betwn('Harry and Sally are very happy', 'a', 'y', 2,1);
select PLVstr.betwn('Harry and Sally are very happy', 'a', 'a', 2, 2);
select PLVstr.betwn('Harry and Sally are very happy', 'a', 'a', 2, 3, FALSE,FALSE);

select plvsubst.string('My name is %s %s.', ARRAY['Pavel','Stěhule']);
select plvsubst.string('My name is % %.', ARRAY['Pavel','Stěhule'], '%');
select plvsubst.string('My name is %s.', ARRAY['Stěhule']);
select plvsubst.string('My name is %s %s.', 'Pavel,Stěhule');
select plvsubst.string('My name is %s %s.', 'Pavel|Stěhule','|');
select plvsubst.string('My name is %s.', 'Stěhule');

select round(to_date ('22-AUG-03', 'DD-MON-YY'),'YEAR')  =  to_date ('01-JAN-04', 'DD-MON-YY');
select round(to_date ('22-AUG-03', 'DD-MON-YY'),'Q')  =  to_date ('01-OCT-03', 'DD-MON-YY');
select round(to_date ('22-AUG-03', 'DD-MON-YY'),'MONTH') =  to_date ('01-SEP-03', 'DD-MON-YY');
select round(to_date ('22-AUG-03', 'DD-MON-YY'),'DDD')  =  to_date ('22-AUG-03', 'DD-MON-YY');
select round(to_date ('22-AUG-03', 'DD-MON-YY'),'DAY')  =  to_date ('24-AUG-03', 'DD-MON-YY');
select trunc(to_date('22-AUG-03', 'DD-MON-YY'), 'YEAR')  =  to_date ('01-JAN-03', 'DD-MON-YY');
select trunc(to_date('22-AUG-03', 'DD-MON-YY'), 'Q')  =  to_date ('01-JUL-03', 'DD-MON-YY');
select trunc(to_date('22-AUG-03', 'DD-MON-YY'), 'MONTH') =  to_date ('01-AUG-03', 'DD-MON-YY');                                                                       
select trunc(to_date('22-AUG-03', 'DD-MON-YY'), 'DDD')  =  to_date ('22-AUG-03', 'DD-MON-YY');
select trunc(to_date('22-AUG-03', 'DD-MON-YY'), 'DAY')  =  to_date ('17-AUG-03', 'DD-MON-YY');
select next_day(to_date('01-Aug-03', 'DD-MON-YY'), 'TUESDAY')  =  to_date ('05-Aug-03', 'DD-MON-YY');                                                                 
select next_day(to_date('06-Aug-03', 'DD-MON-YY'), 'WEDNESDAY') =  to_date ('13-Aug-03', 'DD-MON-YY');
select next_day(to_date('06-Aug-03', 'DD-MON-YY'), 'SUNDAY')  =  to_date ('10-Aug-03', 'DD-MON-YY');
select last_day(to_date('2003/03/15', 'yyyy/mm/dd'))   =  to_date ('Mar 31, 2003', 'Mon DD, YYYY');
select last_day(to_date('2003/02/03', 'yyyy/mm/dd'))  =  to_date ('Feb 28, 2003', 'Mon DD, YYYY');
select last_day(to_date('2004/02/03', 'yyyy/mm/dd'))  =  to_date ('Feb 29, 2004', 'Mon DD, YYYY');
select instr('Tech on the net', 'e') =2;
select instr('Tech on the net', 'e', 1, 1) = 2;
select instr('Tech on the net', 'e', 1, 2) = 11;
select instr('Tech on the net', 'e', 1, 3) = 14;
select instr('Tech on the net', 'e', -3, 2) = 2;
select oracle.substr('This is a test', 6, 2) = 'is';
select oracle.substr('This is a test', 6) =  'is a test';
select oracle.substr('TechOnTheNet', 1, 4) =  'Tech';
select oracle.substr('TechOnTheNet', -3, 3) =  'Net';
select oracle.substr('TechOnTheNet', -6, 3) =  'The';
select oracle.substr('TechOnTheNet', -8, 2) =  'On';
select concat('Tech on', ' the Net') =  'Tech on the Net';
select concat('a', 'b') =  'ab';
select PLVstr.rvrs ('Jumping Jack Flash') ='hsalF kcaJ gnipmuJ';
select PLVstr.rvrs ('Jumping Jack Flash', 9) = 'hsalF kcaJ';
select PLVstr.rvrs ('Jumping Jack Flash', 4, 6) = 'nip';
select PLVstr.lstrip ('*val1|val2|val3|*', '*') = 'val1|val2|val3|*';
select PLVstr.lstrip (',,,val1,val2,val3,', ',', 3)= 'val1,val2,val3,';
select PLVstr.lstrip ('WHERE WHITE = ''FRONT'' AND COMP# = 1500', 'WHERE ') = 'WHITE = ''FRONT'' AND COMP# = 1500';
select plvstr.left('Příliš žluťoučký kůň',4) = 'Příl';

select pos,token from plvlex.tokens('select * from a.b.c join d ON x=y', true, true);


