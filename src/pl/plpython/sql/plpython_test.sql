-- first some tests of basic functionality
CREATE EXTENSION plpython2u;

-- really stupid function just to get the module loaded
CREATE FUNCTION stupid() RETURNS text AS 'return "zarkon"' LANGUAGE plpythonu;

select stupid();

-- check 2/3 versioning
CREATE FUNCTION stupidn() RETURNS text AS 'return "zarkon"' LANGUAGE plpython2u;

select stupidn();

-- test multiple arguments and odd characters in function name
CREATE FUNCTION "Argument test #1"(u users, a1 text, a2 text) RETURNS text
	AS
'keys = list(u.keys())
keys.sort()
out = []
for key in keys:
    out.append("%s: %s" % (key, u[key]))
words = a1 + " " + a2 + " => {" + ", ".join(out) + "}"
return words'
	LANGUAGE plpythonu;

select "Argument test #1"(users, fname, lname) from users where lname = 'doe' order by 1;


-- check module contents
CREATE FUNCTION module_contents() RETURNS text AS
$$
contents = list(filter(lambda x: not x.startswith("__"), dir(plpy)))
contents.sort()
return ", ".join(contents)
$$ LANGUAGE plpythonu;

select module_contents();


CREATE FUNCTION elog_test() RETURNS void
AS $$
plpy.debug('debug')
plpy.log('log')
plpy.info('info')
plpy.info(37)
plpy.info()
plpy.info('info', 37, [1, 2, 3])
plpy.notice('notice')
plpy.warning('warning')
plpy.error('error')
$$ LANGUAGE plpythonu;

SELECT elog_test();

-- Long query Log will be truncated when writing log.csv. 
-- If a UTF-8 character is truncated in its middle,
-- the encoding ERROR will appear due to the half of a UTF-8 character, like �.
-- PR-11946 can fix it.
-- If you want more detail, please refer to ISSUE-15319
SET client_encoding TO 'UTF8';

CREATE FUNCTION elog_test_string_truncate() RETURNS void
AS $$
plpy.log("1"+("床前明月光疑是地上霜举头望明月低头思故乡\n"+
"独坐幽篁里弹琴复长啸深林人不知明月来相照\n"+
"千山鸟飞绝万径人踪灭孤舟蓑笠翁独钓寒江雪\n"+
"白日依山尽黄河入海流欲穷千里目更上一层楼\n"+
"好雨知时节当春乃发生随风潜入夜润物细无声\n")*267)
$$ LANGUAGE plpythonu;

SELECT elog_test_string_truncate();

SELECT logseverity FROM gp_toolkit.__gp_log_master_ext order by logtime desc limit 5;
