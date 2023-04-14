-- Create our own copies of these test tables in our own
-- schema (see qp_functions_in_contexts_setup.sql)
CREATE SCHEMA qp_funcs_in_subquery;
set search_path='qp_funcs_in_subquery', 'qp_funcs_in_contexts';

CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
ANALYZE foo;
ANALYZE bar;

-- @description function_in_subqry_0.sql
SELECT * FROM foo, (SELECT func1_nosql_vol(5)) r order by 1,2,3;

-- @description function_in_subqry_1.sql
SELECT * FROM foo, (SELECT func1_nosql_stb(5)) r order by 1,2,3;

-- @description function_in_subqry_2.sql
SELECT * FROM foo, (SELECT func1_nosql_imm(5)) r order by 1,2,3;
-- @description function_in_subqry_3.sql
SELECT * FROM foo, (SELECT func1_sql_int_vol(5)) r order by 1,2,3;

-- @description function_in_subqry_4.sql
SELECT * FROM foo, (SELECT func1_sql_int_stb(5)) r order by 1,2,3;

-- @description function_in_subqry_5.sql
SELECT * FROM foo, (SELECT func1_sql_int_imm(5)) r order by 1,2,3;

-- @description function_in_subqry_6.sql
SELECT * FROM foo, (SELECT func1_sql_setint_vol(5)) r order by 1,2,3;

-- @description function_in_subqry_7.sql
SELECT * FROM foo, (SELECT func1_sql_setint_stb(5)) r order by 1,2,3;

-- @description function_in_subqry_8.sql
SELECT * FROM foo, (SELECT func1_sql_setint_imm(5)) r order by 1,2,3;

-- @description function_in_subqry_9.sql
SELECT * FROM foo, (SELECT func1_read_int_sql_vol(5)) r order by 1,2,3;

-- @description function_in_subqry_10.sql
SELECT * FROM foo, (SELECT func1_read_int_sql_stb(5)) r order by 1,2,3;

-- @description function_in_subqry_12.sql
SELECT * FROM foo, (SELECT func1_read_setint_sql_stb(5)) r order by 1,2,3;

-- @description function_in_subqry_13.sql
begin;
SELECT * FROM foo, (SELECT func1_mod_int_vol(5)) r order by 1,2,3;
rollback;

-- @description function_in_subqry_14.sql
begin;
SELECT * FROM foo, (SELECT func1_mod_int_stb(5)) r order by 1,2,3;
rollback;

-- @description function_in_subqry_16.sql
begin;
SELECT * FROM foo, (SELECT func1_mod_setint_stb(5)) r order by 1,2,3;
rollback;

-- @description function_in_subqry_withfunc2_0.sql
SELECT * FROM foo, (SELECT func1_nosql_vol(func2_nosql_vol(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_1.sql
SELECT * FROM foo, (SELECT func1_nosql_vol(func2_nosql_stb(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_2.sql
SELECT * FROM foo, (SELECT func1_nosql_vol(func2_nosql_imm(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_3.sql
SELECT * FROM foo, (SELECT func1_nosql_vol(func2_sql_int_vol(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_4.sql
SELECT * FROM foo, (SELECT func1_nosql_vol(func2_sql_int_stb(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_5.sql
SELECT * FROM foo, (SELECT func1_nosql_vol(func2_sql_int_imm(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_6.sql
SELECT * FROM foo, (SELECT func1_nosql_vol(func2_read_int_vol(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_7.sql
SELECT * FROM foo, (SELECT func1_nosql_vol(func2_read_int_stb(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_8.sql
begin;
SELECT * FROM foo, (SELECT func1_nosql_vol(func2_mod_int_vol(5))) r order by 1,2,3;
rollback;

-- @description function_in_subqry_withfunc2_9.sql
begin;
SELECT * FROM foo, (SELECT func1_nosql_vol(func2_mod_int_stb(5))) r order by 1,2,3;
rollback;

-- @description function_in_subqry_withfunc2_10.sql
SELECT * FROM foo, (SELECT func1_nosql_stb(func2_nosql_vol(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_11.sql
SELECT * FROM foo, (SELECT func1_nosql_stb(func2_nosql_stb(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_12.sql
SELECT * FROM foo, (SELECT func1_nosql_stb(func2_nosql_imm(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_13.sql
SELECT * FROM foo, (SELECT func1_nosql_stb(func2_sql_int_vol(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_14.sql
SELECT * FROM foo, (SELECT func1_nosql_stb(func2_sql_int_stb(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_15.sql
SELECT * FROM foo, (SELECT func1_nosql_stb(func2_sql_int_imm(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_16.sql
SELECT * FROM foo, (SELECT func1_nosql_stb(func2_read_int_vol(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_17.sql
SELECT * FROM foo, (SELECT func1_nosql_stb(func2_read_int_stb(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_18.sql
begin;
SELECT * FROM foo, (SELECT func1_nosql_stb(func2_mod_int_vol(5))) r order by 1,2,3;
rollback;

-- @description function_in_subqry_withfunc2_19.sql
begin;
SELECT * FROM foo, (SELECT func1_nosql_stb(func2_mod_int_stb(5))) r order by 1,2,3;
rollback;

-- @description function_in_subqry_withfunc2_20.sql
SELECT * FROM foo, (SELECT func1_nosql_imm(func2_nosql_vol(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_21.sql
SELECT * FROM foo, (SELECT func1_nosql_imm(func2_nosql_stb(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_22.sql
SELECT * FROM foo, (SELECT func1_nosql_imm(func2_nosql_imm(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_23.sql
SELECT * FROM foo, (SELECT func1_nosql_imm(func2_sql_int_vol(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_24.sql
SELECT * FROM foo, (SELECT func1_nosql_imm(func2_sql_int_stb(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_25.sql
SELECT * FROM foo, (SELECT func1_nosql_imm(func2_sql_int_imm(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_26.sql
SELECT * FROM foo, (SELECT func1_nosql_imm(func2_read_int_vol(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_27.sql
SELECT * FROM foo, (SELECT func1_nosql_imm(func2_read_int_stb(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_28.sql
begin;
SELECT * FROM foo, (SELECT func1_nosql_imm(func2_mod_int_vol(5))) r order by 1,2,3;
rollback;

-- @description function_in_subqry_withfunc2_29.sql
begin;
SELECT * FROM foo, (SELECT func1_nosql_imm(func2_mod_int_stb(5))) r order by 1,2,3;
rollback;

-- @description function_in_subqry_withfunc2_30.sql
SELECT * FROM foo, (SELECT func1_sql_int_vol(func2_nosql_vol(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_31.sql
SELECT * FROM foo, (SELECT func1_sql_int_vol(func2_nosql_stb(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_32.sql
SELECT * FROM foo, (SELECT func1_sql_int_vol(func2_nosql_imm(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_33.sql
SELECT * FROM foo, (SELECT func1_sql_int_vol(func2_sql_int_vol(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_34.sql
SELECT * FROM foo, (SELECT func1_sql_int_vol(func2_sql_int_stb(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_35.sql
SELECT * FROM foo, (SELECT func1_sql_int_vol(func2_sql_int_imm(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_36.sql
SELECT * FROM foo, (SELECT func1_sql_int_vol(func2_read_int_vol(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_37.sql
SELECT * FROM foo, (SELECT func1_sql_int_vol(func2_read_int_stb(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_38.sql
begin;
SELECT * FROM foo, (SELECT func1_sql_int_vol(func2_mod_int_vol(5))) r order by 1,2,3;
rollback;

-- @description function_in_subqry_withfunc2_39.sql
begin;
SELECT * FROM foo, (SELECT func1_sql_int_vol(func2_mod_int_stb(5))) r order by 1,2,3;
rollback;

-- @description function_in_subqry_withfunc2_40.sql
SELECT * FROM foo, (SELECT func1_sql_int_stb(func2_nosql_vol(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_41.sql
SELECT * FROM foo, (SELECT func1_sql_int_stb(func2_nosql_stb(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_42.sql
SELECT * FROM foo, (SELECT func1_sql_int_stb(func2_nosql_imm(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_43.sql
SELECT * FROM foo, (SELECT func1_sql_int_stb(func2_sql_int_vol(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_44.sql
SELECT * FROM foo, (SELECT func1_sql_int_stb(func2_sql_int_stb(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_45.sql
SELECT * FROM foo, (SELECT func1_sql_int_stb(func2_sql_int_imm(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_46.sql
SELECT * FROM foo, (SELECT func1_sql_int_stb(func2_read_int_vol(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_47.sql
SELECT * FROM foo, (SELECT func1_sql_int_stb(func2_read_int_stb(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_48.sql
begin;
SELECT * FROM foo, (SELECT func1_sql_int_stb(func2_mod_int_vol(5))) r order by 1,2,3;
rollback;

-- @description function_in_subqry_withfunc2_49.sql
begin;
SELECT * FROM foo, (SELECT func1_sql_int_stb(func2_mod_int_stb(5))) r order by 1,2,3;
rollback;

-- @description function_in_subqry_withfunc2_50.sql
SELECT * FROM foo, (SELECT func1_sql_int_imm(func2_nosql_vol(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_51.sql
SELECT * FROM foo, (SELECT func1_sql_int_imm(func2_nosql_stb(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_52.sql
SELECT * FROM foo, (SELECT func1_sql_int_imm(func2_nosql_imm(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_53.sql
SELECT * FROM foo, (SELECT func1_sql_int_imm(func2_sql_int_vol(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_54.sql
SELECT * FROM foo, (SELECT func1_sql_int_imm(func2_sql_int_stb(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_55.sql
SELECT * FROM foo, (SELECT func1_sql_int_imm(func2_sql_int_imm(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_56.sql
SELECT * FROM foo, (SELECT func1_sql_int_imm(func2_read_int_vol(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_57.sql
SELECT * FROM foo, (SELECT func1_sql_int_imm(func2_read_int_stb(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_58.sql
begin;
SELECT * FROM foo, (SELECT func1_sql_int_imm(func2_mod_int_vol(5))) r order by 1,2,3;
rollback;

-- @description function_in_subqry_withfunc2_59.sql
begin;
SELECT * FROM foo, (SELECT func1_sql_int_imm(func2_mod_int_stb(5))) r order by 1,2,3;
rollback;

-- @description function_in_subqry_withfunc2_60.sql
SELECT * FROM foo, (SELECT func1_sql_setint_vol(func2_nosql_vol(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_61.sql
SELECT * FROM foo, (SELECT func1_sql_setint_vol(func2_nosql_stb(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_62.sql
SELECT * FROM foo, (SELECT func1_sql_setint_vol(func2_nosql_imm(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_63.sql
SELECT * FROM foo, (SELECT func1_sql_setint_vol(func2_sql_int_vol(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_64.sql
SELECT * FROM foo, (SELECT func1_sql_setint_vol(func2_sql_int_stb(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_65.sql
SELECT * FROM foo, (SELECT func1_sql_setint_vol(func2_sql_int_imm(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_66.sql
SELECT * FROM foo, (SELECT func1_sql_setint_vol(func2_read_int_vol(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_67.sql
SELECT * FROM foo, (SELECT func1_sql_setint_vol(func2_read_int_stb(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_68.sql
begin;
SELECT * FROM foo, (SELECT func1_sql_setint_vol(func2_mod_int_vol(5))) r order by 1,2,3;
rollback;

-- @description function_in_subqry_withfunc2_69.sql
begin;
SELECT * FROM foo, (SELECT func1_sql_setint_vol(func2_mod_int_stb(5))) r order by 1,2,3;
rollback;

-- @description function_in_subqry_withfunc2_70.sql
SELECT * FROM foo, (SELECT func1_sql_setint_stb(func2_nosql_vol(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_71.sql
SELECT * FROM foo, (SELECT func1_sql_setint_stb(func2_nosql_stb(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_72.sql
SELECT * FROM foo, (SELECT func1_sql_setint_stb(func2_nosql_imm(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_73.sql
SELECT * FROM foo, (SELECT func1_sql_setint_stb(func2_sql_int_vol(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_74.sql
SELECT * FROM foo, (SELECT func1_sql_setint_stb(func2_sql_int_stb(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_75.sql
SELECT * FROM foo, (SELECT func1_sql_setint_stb(func2_sql_int_imm(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_76.sql
SELECT * FROM foo, (SELECT func1_sql_setint_stb(func2_read_int_vol(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_77.sql
SELECT * FROM foo, (SELECT func1_sql_setint_stb(func2_read_int_stb(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_78.sql
begin;
SELECT * FROM foo, (SELECT func1_sql_setint_stb(func2_mod_int_vol(5))) r order by 1,2,3;
rollback;

-- @description function_in_subqry_withfunc2_79.sql
begin;
SELECT * FROM foo, (SELECT func1_sql_setint_stb(func2_mod_int_stb(5))) r order by 1,2,3;
rollback;

-- @description function_in_subqry_withfunc2_80.sql
SELECT * FROM foo, (SELECT func1_sql_setint_imm(func2_nosql_vol(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_81.sql
SELECT * FROM foo, (SELECT func1_sql_setint_imm(func2_nosql_stb(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_82.sql
SELECT * FROM foo, (SELECT func1_sql_setint_imm(func2_nosql_imm(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_83.sql
SELECT * FROM foo, (SELECT func1_sql_setint_imm(func2_sql_int_vol(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_84.sql
SELECT * FROM foo, (SELECT func1_sql_setint_imm(func2_sql_int_stb(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_85.sql
SELECT * FROM foo, (SELECT func1_sql_setint_imm(func2_sql_int_imm(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_86.sql
SELECT * FROM foo, (SELECT func1_sql_setint_imm(func2_read_int_vol(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_87.sql
SELECT * FROM foo, (SELECT func1_sql_setint_imm(func2_read_int_stb(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_88.sql
begin;
SELECT * FROM foo, (SELECT func1_sql_setint_imm(func2_mod_int_vol(5))) r order by 1,2,3;
rollback;

-- @description function_in_subqry_withfunc2_89.sql
begin;
SELECT * FROM foo, (SELECT func1_sql_setint_imm(func2_mod_int_stb(5))) r order by 1,2,3;
rollback;

-- @description function_in_subqry_withfunc2_90.sql
SELECT * FROM foo, (SELECT func1_read_int_sql_vol(func2_nosql_vol(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_91.sql
SELECT * FROM foo, (SELECT func1_read_int_sql_vol(func2_nosql_stb(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_92.sql
SELECT * FROM foo, (SELECT func1_read_int_sql_vol(func2_nosql_imm(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_93.sql
SELECT * FROM foo, (SELECT func1_read_int_sql_vol(func2_sql_int_vol(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_94.sql
SELECT * FROM foo, (SELECT func1_read_int_sql_vol(func2_sql_int_stb(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_95.sql
SELECT * FROM foo, (SELECT func1_read_int_sql_vol(func2_sql_int_imm(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_96.sql
SELECT * FROM foo, (SELECT func1_read_int_sql_vol(func2_read_int_vol(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_97.sql
SELECT * FROM foo, (SELECT func1_read_int_sql_vol(func2_read_int_stb(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_98.sql
begin;
SELECT * FROM foo, (SELECT func1_read_int_sql_vol(func2_mod_int_vol(5))) r order by 1,2,3;
rollback;

-- @description function_in_subqry_withfunc2_99.sql
begin;
SELECT * FROM foo, (SELECT func1_read_int_sql_vol(func2_mod_int_stb(5))) r order by 1,2,3;
rollback;

-- @description function_in_subqry_withfunc2_100.sql
SELECT * FROM foo, (SELECT func1_read_int_sql_stb(func2_nosql_vol(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_101.sql
SELECT * FROM foo, (SELECT func1_read_int_sql_stb(func2_nosql_stb(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_102.sql
SELECT * FROM foo, (SELECT func1_read_int_sql_stb(func2_nosql_imm(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_103.sql
SELECT * FROM foo, (SELECT func1_read_int_sql_stb(func2_sql_int_vol(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_104.sql
SELECT * FROM foo, (SELECT func1_read_int_sql_stb(func2_sql_int_stb(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_105.sql
SELECT * FROM foo, (SELECT func1_read_int_sql_stb(func2_sql_int_imm(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_106.sql
SELECT * FROM foo, (SELECT func1_read_int_sql_stb(func2_read_int_vol(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_107.sql
SELECT * FROM foo, (SELECT func1_read_int_sql_stb(func2_read_int_stb(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_108.sql
begin;
SELECT * FROM foo, (SELECT func1_read_int_sql_stb(func2_mod_int_vol(5))) r order by 1,2,3;
rollback;

-- @description function_in_subqry_withfunc2_109.sql
begin;
SELECT * FROM foo, (SELECT func1_read_int_sql_stb(func2_mod_int_stb(5))) r order by 1,2,3;
rollback;

-- @description function_in_subqry_withfunc2_110.sql
SELECT * FROM foo, (SELECT func1_read_setint_sql_vol(func2_nosql_vol(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_113.sql
SELECT * FROM foo, (SELECT func1_read_setint_sql_vol(func2_sql_int_vol(5))) r order by 1,2,3;


-- @description function_in_subqry_withfunc2_116.sql
SELECT * FROM foo, (SELECT func1_read_setint_sql_vol(func2_read_int_vol(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_118.sql
begin;
SELECT * FROM foo, (SELECT func1_read_setint_sql_vol(func2_mod_int_vol(5))) r order by 1,2,3;
rollback;

-- @description function_in_subqry_withfunc2_119.sql
begin;
SELECT * FROM foo, (SELECT func1_read_setint_sql_vol(func2_mod_int_stb(5))) r order by 1,2,3;
rollback;

-- @description function_in_subqry_withfunc2_120.sql
SELECT * FROM foo, (SELECT func1_read_setint_sql_stb(func2_nosql_vol(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_121.sql
SELECT * FROM foo, (SELECT func1_read_setint_sql_stb(func2_nosql_stb(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_122.sql
SELECT * FROM foo, (SELECT func1_read_setint_sql_stb(func2_nosql_imm(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_123.sql
SELECT * FROM foo, (SELECT func1_read_setint_sql_stb(func2_sql_int_vol(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_124.sql
SELECT * FROM foo, (SELECT func1_read_setint_sql_stb(func2_sql_int_stb(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_125.sql
SELECT * FROM foo, (SELECT func1_read_setint_sql_stb(func2_sql_int_imm(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_126.sql
SELECT * FROM foo, (SELECT func1_read_setint_sql_stb(func2_read_int_vol(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_127.sql
SELECT * FROM foo, (SELECT func1_read_setint_sql_stb(func2_read_int_stb(5))) r order by 1,2,3;

-- @description function_in_subqry_withfunc2_128.sql
begin;
SELECT * FROM foo, (SELECT func1_read_setint_sql_stb(func2_mod_int_vol(5))) r order by 1,2,3;
rollback;

-- @description function_in_subqry_withfunc2_129.sql
begin;
SELECT * FROM foo, (SELECT func1_read_setint_sql_stb(func2_mod_int_stb(5))) r order by 1,2,3;
rollback;

-- @description function_in_subqry_withfunc2_130.sql
begin;
SELECT * FROM foo, (SELECT func1_mod_int_vol(func2_nosql_vol(5))) r order by 1,2,3;
rollback;

-- @description function_in_subqry_withfunc2_131.sql
begin;
SELECT * FROM foo, (SELECT func1_mod_int_vol(func2_nosql_stb(5))) r order by 1,2,3;
rollback;

-- @description function_in_subqry_withfunc2_132.sql
begin;
SELECT * FROM foo, (SELECT func1_mod_int_vol(func2_nosql_imm(5))) r order by 1,2,3;
rollback;

-- @description function_in_subqry_withfunc2_133.sql
begin;
SELECT * FROM foo, (SELECT func1_mod_int_vol(func2_sql_int_vol(5))) r order by 1,2,3;
rollback;

-- @description function_in_subqry_withfunc2_134.sql
begin;
SELECT * FROM foo, (SELECT func1_mod_int_vol(func2_sql_int_stb(5))) r order by 1,2,3;
rollback;

-- @description function_in_subqry_withfunc2_135.sql
begin;
SELECT * FROM foo, (SELECT func1_mod_int_vol(func2_sql_int_imm(5))) r order by 1,2,3;
rollback;

-- @description function_in_subqry_withfunc2_136.sql
begin;
SELECT * FROM foo, (SELECT func1_mod_int_vol(func2_read_int_vol(5))) r order by 1,2,3;
rollback;

-- @description function_in_subqry_withfunc2_137.sql
begin;
SELECT * FROM foo, (SELECT func1_mod_int_vol(func2_read_int_stb(5))) r order by 1,2,3;
rollback;

-- @description function_in_subqry_withfunc2_138.sql
begin;
SELECT * FROM foo, (SELECT func1_mod_int_vol(func2_mod_int_vol(5))) r order by 1,2,3;
rollback;

-- @description function_in_subqry_withfunc2_139.sql
begin;
SELECT * FROM foo, (SELECT func1_mod_int_vol(func2_mod_int_stb(5))) r order by 1,2,3;
rollback;

-- @description function_in_subqry_withfunc2_140.sql
begin;
SELECT * FROM foo, (SELECT func1_mod_int_stb(func2_nosql_vol(5))) r order by 1,2,3;
rollback;

-- @description function_in_subqry_withfunc2_141.sql
begin;
SELECT * FROM foo, (SELECT func1_mod_int_stb(func2_nosql_stb(5))) r order by 1,2,3;
rollback;

-- @description function_in_subqry_withfunc2_142.sql
begin;
SELECT * FROM foo, (SELECT func1_mod_int_stb(func2_nosql_imm(5))) r order by 1,2,3;
rollback;

-- @description function_in_subqry_withfunc2_143.sql
begin;
SELECT * FROM foo, (SELECT func1_mod_int_stb(func2_sql_int_vol(5))) r order by 1,2,3;
rollback;

-- @description function_in_subqry_withfunc2_144.sql
begin;
SELECT * FROM foo, (SELECT func1_mod_int_stb(func2_sql_int_stb(5))) r order by 1,2,3;
rollback;

-- @description function_in_subqry_withfunc2_145.sql
begin;
SELECT * FROM foo, (SELECT func1_mod_int_stb(func2_sql_int_imm(5))) r order by 1,2,3;
rollback;

-- @description function_in_subqry_withfunc2_146.sql
begin;
SELECT * FROM foo, (SELECT func1_mod_int_stb(func2_read_int_vol(5))) r order by 1,2,3;
rollback;

-- @description function_in_subqry_withfunc2_147.sql
begin;
SELECT * FROM foo, (SELECT func1_mod_int_stb(func2_read_int_stb(5))) r order by 1,2,3;
rollback;

-- @description function_in_subqry_withfunc2_148.sql
begin;
SELECT * FROM foo, (SELECT func1_mod_int_stb(func2_mod_int_vol(5))) r order by 1,2,3;
rollback;

-- @description function_in_subqry_withfunc2_149.sql
begin;
SELECT * FROM foo, (SELECT func1_mod_int_stb(func2_mod_int_stb(5))) r order by 1,2,3;
rollback;

-- @description function_in_subqry_withfunc2_150.sql
begin;
SELECT * FROM foo, (SELECT func1_mod_setint_vol(func2_nosql_vol(5))) r order by 1,2,3;
rollback;

-- @description function_in_subqry_withfunc2_153.sql
begin;
SELECT * FROM foo, (SELECT func1_mod_setint_vol(func2_sql_int_vol(5))) r order by 1,2,3;
rollback;

-- @description function_in_subqry_withfunc2_156.sql
begin;
SELECT * FROM foo, (SELECT func1_mod_setint_vol(func2_read_int_vol(5))) r order by 1,2,3;
rollback;

-- @description function_in_subqry_withfunc2_158.sql
begin;
SELECT * FROM foo, (SELECT func1_mod_setint_vol(func2_mod_int_vol(5))) r order by 1,2,3;
rollback;

-- @description function_in_subqry_withfunc2_159.sql
begin;
SELECT * FROM foo, (SELECT func1_mod_setint_vol(func2_mod_int_stb(5))) r order by 1,2,3;
rollback;

-- @description function_in_subqry_withfunc2_160.sql
begin;
SELECT * FROM foo, (SELECT func1_mod_setint_stb(func2_nosql_vol(5))) r order by 1,2,3;
rollback;

-- @description function_in_subqry_withfunc2_161.sql
begin;
SELECT * FROM foo, (SELECT func1_mod_setint_stb(func2_nosql_stb(5))) r order by 1,2,3;
rollback;

-- @description function_in_subqry_withfunc2_162.sql
begin;
SELECT * FROM foo, (SELECT func1_mod_setint_stb(func2_nosql_imm(5))) r order by 1,2,3;
rollback;

-- @description function_in_subqry_withfunc2_163.sql
begin;
SELECT * FROM foo, (SELECT func1_mod_setint_stb(func2_sql_int_vol(5))) r order by 1,2,3;
rollback;

-- @description function_in_subqry_withfunc2_164.sql
begin;
SELECT * FROM foo, (SELECT func1_mod_setint_stb(func2_sql_int_stb(5))) r order by 1,2,3;
rollback;

-- @description function_in_subqry_withfunc2_165.sql
begin;
SELECT * FROM foo, (SELECT func1_mod_setint_stb(func2_sql_int_imm(5))) r order by 1,2,3;
rollback;

-- @description function_in_subqry_withfunc2_166.sql
begin;
SELECT * FROM foo, (SELECT func1_mod_setint_stb(func2_read_int_vol(5))) r order by 1,2,3;
rollback;

-- @description function_in_subqry_withfunc2_167.sql
begin;
SELECT * FROM foo, (SELECT func1_mod_setint_stb(func2_read_int_stb(5))) r order by 1,2,3;
rollback;

-- @description function_in_subqry_withfunc2_168.sql
begin;
SELECT * FROM foo, (SELECT func1_mod_setint_stb(func2_mod_int_vol(5))) r order by 1,2,3;
rollback;

-- @description function_in_subqry_withfunc2_169.sql
begin;
SELECT * FROM foo, (SELECT func1_mod_setint_stb(func2_mod_int_stb(5))) r order by 1,2,3;
rollback;

-- Test for the target list of RTE_RESULT relation contains unevaluated functions.
-- Functions that return record cannot eval to const during planning time.
EXPLAIN (VERBOSE, COSTS OFF) SELECT func_nosql_record_imm(1) union all select func_nosql_record_imm(2);
SELECT func_nosql_record_imm(1) union all select func_nosql_record_imm(2);
