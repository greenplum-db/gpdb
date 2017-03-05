-- See qp_with_functional.sql
--
-- Launch the tests with CTE sharing off (with planner), or with CTE inlining
-- off (with ORCA).
set gp_cte_sharing = off;
set optimizer_cte_inlining = off;

create schema qp_with_functional_noinlining;
set search_path='qp_with_functional_noinlining';
\i sql/qp_with_functional.sql
